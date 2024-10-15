#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "pin.H"
#include "sde-init.H"

#include "entropy.h"
#include "memory_buffer.h"
#include "memory_buffer_map.h"
#include "memoryregioninfo.h"
#include "staticinstructioninfo.h"
#include "util.h"

// File stream used to write the human-readable log output to.
static std::ofstream log_file;

// File streams to write the CSV output to.
static std::ofstream csv_instructions;
static std::ofstream csv_buffers;
static std::ofstream csv_regions;

// Constant representing an invalid address.
static constexpr ADDRINT INVALID_ADDRESS = -1;

// Stores the address of the application's main() function.
static ADDRINT main_address = INVALID_ADDRESS;

// Stores the address of the instruction after the call to main().
static ADDRINT end_address = INVALID_ADDRESS;

// Determines whether execution has reached main() yet.
static bool main_reached = false;

// Determines whether main() has returned already.
static bool end_reached = false;

// Option (-o) to set the output filename.
KNOB<std::string>
    KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "output", "tool.log",
                   "Specify the filename of the human-readable log file");

// Option (-csv_prefix) to set the prefix used for the CSV output files.
KNOB<std::string>
    KnobCsvPrefix(KNOB_MODE_WRITEONCE, "pintool", "csv_prefix", "",
                  "Set the prefix used for the CSV output files. The output "
                  "files will be of the form <prefix>.instructions.csv etc.");

// Option (-start_from_main) to only start analysis from the point that main()
// is called.
KNOB<bool> KnobStartFromMain(
    KNOB_MODE_WRITEONCE, "pintool", "start_from_main", "1",
    "When true, starts analysis from the point that main() is called");

// Option (-end_after_main) to end analysis after main() is finished.
KNOB<bool>
    KnobEndAfterMain(KNOB_MODE_WRITEONCE, "pintool", "end_after_main", "1",
                     "When true, ends analysis after main() is finished");

// Option (-entropy_sample_interval) that determines how frequently spatial and
// temporal entropy is sampled for a memory buffer/region. This setting is
// expressed in number of writes to that memory buffer/region.
KNOB<int> KnobEntropySampleInterval(
    KNOB_MODE_WRITEONCE, "pintool", "entropy_sample_interval", "100",
    "Sample interval in time for the calculations of spatial and temporal "
    "entropy. This setting determines how frequently spatial and temporal "
    "entropy is sampled for a given memory buffer or region. It is expressed "
    "in number of writes to that specific memory buffer or region.");

// Option (-instruction_values_limit) that determines how many unique values to
// keep per static instruction.
KNOB<std::size_t> KnobInstructionValuesLimit(
    KNOB_MODE_WRITEONCE, "pintool", "instruction_values_limit", "5",
    "Number of unique read/written values to keep per static instruction.");

// Remembers the last size that was used as argument to malloc().
static ADDRINT last_malloc_size = -1;

// Remembers the last allocation site address.
static ADDRINT last_allocation_address = 0;

// Remembers allocation backtrace for the last call to malloc().
static std::string last_allocation_backtrace;

// Global Pin lock.
static PIN_LOCK pin_lock;

// Contains the MemoryRegionInfo for memory buffers that are not free'd yet.
static MemoryBufferMap<MemoryRegionInfo> active_mem_buf_infos;

// Contains the MemoryRegionInfo for memory buffers that are free'd.
// Note that this is a vector, and not a map, because it may contain buffers
// with the same key (= end address).
static std::vector<std::pair<MemoryBuffer, MemoryRegionInfo>>
    freed_mem_buf_infos;

// Contains the MemoryRegionInfo for regions of memory.
static MemoryBufferMap<MemoryRegionInfo> memory_regions_infos;

// Contains the StaticInstructionInfo for static instructions.
static std::map<ADDRINT, StaticInstructionInfo> static_instruction_infos;

// Size of each region of memory in bytes.
static constexpr std::size_t MEMORY_REGION_SIZE = 16;

// Remembers the effective address of memory operands.
// Key = memory operand index, value = effective address.
static std::map<UINT32, ADDRINT> memory_operands_map;

// Maximal size of the backtrace.
static std::size_t BACKTRACE_MAX_SIZE = 10;

// =============================================================================
// Analysis routines
// =============================================================================

// Run before every instruction.
VOID InstructionBefore(ADDRINT instruction_pointer) {
  // Check if main if reached yet.
  if (instruction_pointer == main_address) {
    log_file << "\nMain reached, setting global flag.\n";
    main_reached = true;
  }

  // Check if main is finished.
  if (instruction_pointer == end_address) {
    log_file
        << "\nInstruction after call to main reached, setting global flag.\n";
    end_reached = true;
  }
}

// Runs before every call instruction.
VOID InstructionCallBefore(ADDRINT target_address,
                           ADDRINT next_instruction_pointer) {
  // Check if this is the first call to main().
  if ((end_address == INVALID_ADDRESS) && (target_address == main_address)) {
    log_file << "\nMain called, next ip = " << std::hex << std::showbase
             << next_instruction_pointer << std::dec << "\n";
    end_address = next_instruction_pointer;
  }
}

// Run at the start of __libc_start_main().
VOID LibcStartMainBefore(ADDRINT main_addr) {
  log_file << "\n__libc_start_main(main = " << std::hex << std::showbase
           << main_addr << std::dec << ", ...)\n";

  main_address = main_addr;
}

// Run at the start of malloc().
VOID MallocBefore(const CONTEXT *ctx, ADDRINT image_id, ADDRINT size) {
  if (!main_reached || end_reached)
    return;

  // Acquire lock.
  PIN_GetLock(&pin_lock, 1);

  // Find the image name corresponding to this malloc.
  IMG image = IMG_FindImgById(image_id);

  // Signature: malloc(size_t size).
  log_file << "\n" << IMG_Name(image) << "::malloc(size = " << size << ")\n";

  // Save malloc size.
  last_malloc_size = size;

  // Save the backtrace of the allocation site.
  std::ostringstream allocation_backtrace_stream;

  PIN_LockClient();

  // Reset last allocation address to 'unknown' value.
  last_allocation_address = 0;

  void *bt[BACKTRACE_MAX_SIZE];
  std::size_t bt_size = PIN_Backtrace(ctx, bt, sizeof(bt) / sizeof(*bt));

  for (std::size_t i = 0; i < bt_size; ++i) {
    ADDRINT addr = reinterpret_cast<ADDRINT>(bt[i]);

    // Get the image corresponding to this address in the backtrace.
    IMG image = IMG_FindByAddress(addr);
    std::string image_name = IMG_Valid(image) ? IMG_Name(image) : "???";
    ADDRINT image_offset = IMG_Valid(image) ? (addr - IMG_LowAddress(image))
                                            : static_cast<ADDRINT>(-1);

    // Use | instead of newline, because we can't embed newlines in CSV.
    allocation_backtrace_stream
        << std::hex << bt[i] << std::dec << " = "
        << "$DEBUG(" << image_name << "," << image_offset
        << ",filename)" // filename
        << ":"
        << "$DEBUG(" << image_name << "," << image_offset << ",line)" // line
        << ":"
        << "$DEBUG(" << image_name << "," << image_offset << ",column)" // col
        << '|';

    // Only update last_allocation_address if it's not already set
    if (last_allocation_address == 0) {
      // Check if address is in libc.
      if (image_name.find("libc.so.6") == std::string::npos) {
        last_allocation_address = addr;
      }
    }
  }

  PIN_UnlockClient();

  last_allocation_backtrace = allocation_backtrace_stream.str();

  // Release lock.
  PIN_ReleaseLock(&pin_lock);
}

// Run at the end of malloc().
VOID MallocAfter(ADDRINT addr) {
  if (!main_reached || end_reached)
    return;

  // Acquire lock.
  PIN_GetLock(&pin_lock, 1);

  // Return value of malloc: pointer of first element of allocated buffer.
  log_file << "---> address = " << std::hex << std::showbase << addr << std::dec
           << "\n";

  // Store buffer in map.
  MemoryRegionInfo info(last_malloc_size, last_allocation_address);
  info.DEBUG_allocation_backtrace = last_allocation_backtrace;
  active_mem_buf_infos.insert(
      {MemoryBuffer(addr, addr + last_malloc_size), info});

  // Release lock.
  PIN_ReleaseLock(&pin_lock);
}

// Run at the start of free().
VOID FreeBefore(ADDRINT image_id, ADDRINT addr) {
  if (!main_reached || end_reached)
    return;

  // Acquire lock.
  PIN_GetLock(&pin_lock, 1);

  // Find the image name corresponding to this free.
  IMG image = IMG_FindImgById(image_id);

  // Signature: free(void* ptr).
  log_file << "\n"
           << IMG_Name(image) << "::free(ptr = " << std::hex << std::showbase
           << addr << std::dec << ")\n";

  // The memory buffer is free'd now, so remove it from the list of active
  // memory buffers, and add it to the list of free'd buffers.
  auto it = active_mem_buf_infos.find_buffer_containing_address(addr);
  if (it != active_mem_buf_infos.end()) {
    freed_mem_buf_infos.push_back(*it);
    active_mem_buf_infos.erase(it);
  }

  // Release lock.
  PIN_ReleaseLock(&pin_lock);
}

// Run before every memory access, i.e. both read and write.
VOID MemoryAccessBefore(ADDRINT instruction_address, BOOL is_write,
                        UINT32 mem_op, ADDRINT memory_address, ADDRINT size) {
  if (!main_reached || end_reached)
    return;

  // Acquire lock.
  PIN_GetLock(&pin_lock, 1);

  // Store the memory address so we can reuse it later.
  memory_operands_map[mem_op] = memory_address;

  // Log read values per static instruction.
  if (!is_write) {
    // Get the read value.
    unsigned char *value_buf = new unsigned char[size];
    PIN_SafeCopy(value_buf, reinterpret_cast<void *>(memory_address), size);

    // Add the read value.
    auto it = static_instruction_infos.find(instruction_address);
    if (it != static_instruction_infos.end()) {
      auto &container = it->second.read_values;
      if (container.size() < KnobInstructionValuesLimit.Value()) {
        container.insert(
            std::vector<unsigned char>(value_buf, value_buf + size));
      }

      // Update counters for read value.
      for (ADDRINT i = 0; i < size; ++i) {
        ++it->second.byte_counts_read[value_buf[i]];
      }
    }

    delete[] value_buf;
  }

  // Release lock.
  PIN_ReleaseLock(&pin_lock);
}

// Processing code after memory accesses that is common to known buffers and to
// unknown memory locations.
template <typename map_iterator>
VOID ProcessMemoryAccessAfter(ADDRINT instruction_address, BOOL is_write,
                              ADDRINT memory_address, ADDRINT size,
                              map_iterator &it) {
  // Update the #reads/#writes counters.
  if (is_write)
    ++it->second.num_writes;
  else
    ++it->second.num_reads;

  // Update the spatial and temporal entropy information every x writes.
  if (is_write &&
      (it->second.num_writes % KnobEntropySampleInterval.Value() == 0)) {
    // Get the start address and size of the buffer/region corresponding to this
    // memory access.
    const unsigned char *start_address =
        reinterpret_cast<const unsigned char *>(it->first.start_address);
    const unsigned int buf_size =
        it->first.end_address - it->first.start_address;

    // Update the spatial and temporal entropy infos.
    it->second.spatial_entropy_info.record(start_address, buf_size);
    it->second.temporal_entropy_info.record(start_address, buf_size);
  }

  // Update the list of static instructions that read from/write to this memory
  // location.
  if (is_write)
    it->second.write_static_instructions.insert(instruction_address);
  else
    it->second.read_static_instructions.insert(instruction_address);
}

// Run after every memory access, i.e. both read and write.
VOID MemoryAccessAfter(ADDRINT instruction_address, BOOL is_write,
                       UINT32 mem_op, ADDRINT size) {
  if (!main_reached || end_reached)
    return;

  // Acquire lock.
  PIN_GetLock(&pin_lock, 1);

  // Retrieve the memory address stored in memory_operands_map.
  ADDRINT memory_address = memory_operands_map[mem_op];

  // Check if address lies within known buffer.
  auto it = active_mem_buf_infos.find_buffer_containing_address(memory_address);
  if (it != active_mem_buf_infos.end()) {
    ProcessMemoryAccessAfter(instruction_address, is_write, memory_address,
                             size, it);
  }

  // Insert memory region at this address, but only if it does not exist
  // already.
  const auto start = memory_address & (-MEMORY_REGION_SIZE);
  const auto end = start + MEMORY_REGION_SIZE;
  const MemoryBuffer cur_region(start, end);

  auto res = memory_regions_infos.insert(
      {cur_region, MemoryRegionInfo(MEMORY_REGION_SIZE)});

  // Process memory access.
  ProcessMemoryAccessAfter(instruction_address, is_write, memory_address, size,
                           res.first);

  // Log written values per static instruction.
  if (is_write) {
    // Get the written value.
    unsigned char *value_buf = new unsigned char[size];
    PIN_SafeCopy(value_buf, reinterpret_cast<void *>(memory_address), size);

    // Add the written value.
    auto it = static_instruction_infos.find(instruction_address);
    if (it != static_instruction_infos.end()) {
      auto &container = it->second.written_values;
      if (container.size() < KnobInstructionValuesLimit.Value()) {
        container.insert(
            std::vector<unsigned char>(value_buf, value_buf + size));
      }

      // Update counters for written value.
      for (ADDRINT i = 0; i < size; ++i) {
        ++it->second.byte_counts_written[value_buf[i]];
      }
    }

    delete[] value_buf;
  }

  // Release lock.
  PIN_ReleaseLock(&pin_lock);
}

// Run at the start of the debug annotation routine
// (MATE_FRAMEWORK_DEBUG_ANNOTATE_MEMORY)
VOID AnnotateMemoryBefore(ADDRINT address, ADDRINT size, ADDRINT annotation,
                          ADDRINT filename, ADDRINT line) {
  // Signature: MATE_FRAMEWORK_DEBUG_ANNOTATE_MEMORY(const char* address,
  // unsigned int size, const char* annotation, const char* filename, unsigned
  // int line);

  // Acquire lock.
  PIN_GetLock(&pin_lock, 1);

  // Check if the address corresponds to a known memory buffer.
  auto it = active_mem_buf_infos.find_buffer_containing_address(address);
  if (it != active_mem_buf_infos.end()) {
    // Add the annotation to the memory buffer info.
    // Separate different entries with ';'.
    std::ostringstream annotation_oss;
    annotation_oss << reinterpret_cast<const char *>(annotation);

    // Add offset, size and location information to the annotation string.
    // Format: "(offset=<off>,size=<sz>,location=<file>:<line>)"
    annotation_oss << "(";
    annotation_oss << "offset=" << address - it->first.start_address;
    annotation_oss << ",size=" << size;
    annotation_oss << ",location=" << reinterpret_cast<const char *>(filename)
                   << ":" << line;
    annotation_oss << ")";

    if (!it->second.DEBUG_annotation.empty()) {
      it->second.DEBUG_annotation += ";";
    }

    it->second.DEBUG_annotation += annotation_oss.str();
  }

  // Release lock.
  PIN_ReleaseLock(&pin_lock);
}

// =============================================================================
// Instrumentation routines
// =============================================================================

// Instrumentation routine run for every instruction.
VOID OnInstruction(INS instruction, VOID *v) {
  // Call InstructionBefore() before every instruction.
  // Pass RIP as argument.
  INS_InsertCall(instruction, IPOINT_BEFORE,
                 reinterpret_cast<AFUNPTR>(InstructionBefore), IARG_INST_PTR,
                 IARG_END);

  // Call InstructionCallBefore() before every call instruction.
  // Pass the target address (i.e. the address of the function being called) and
  // the address of the next instruction (i.e. the one following the call).
  if (INS_IsCall(instruction)) {
    INS_InsertCall(instruction, IPOINT_BEFORE,
                   reinterpret_cast<AFUNPTR>(InstructionCallBefore),
                   IARG_BRANCH_TARGET_ADDR, IARG_ADDRINT,
                   INS_NextAddress(instruction), IARG_END);
  }

  // Get the number of memory operands this instruction has.
  UINT32 num_mem_operands = INS_MemoryOperandCount(instruction);

  // Create a new entry in the static instruction map for instructions that read
  // from/write to memory.
  if (num_mem_operands > 0) {
    ADDRINT ins_addr = INS_Address(instruction);
    static_instruction_infos.insert(
        {ins_addr,
         StaticInstructionInfo(INS_Disassemble(instruction), ins_addr)});
  }

  // Call MemoryAccessBefore() before every memory read/write
  // and MemoryAccessAfter() after every memory read/write (if supported).
  // Pass the effective address and operand size as argument.
  // Note that an instruction may have multiple memory operands.
  for (UINT32 mem_op = 0; mem_op < num_mem_operands; ++mem_op) {
    // Get the number of bytes of this memory operand.
    ADDRINT size = INS_MemoryOperandSize(instruction, mem_op);

    // MemoryAccessBefore()

    // Use predicated call so that instrumentation is only called for
    // instructions that are actually executed. This is important for
    // conditional moves and instructions with a REP prefix.
    if (INS_MemoryOperandIsRead(instruction, mem_op)) {
      INS_InsertPredicatedCall(instruction, IPOINT_BEFORE,
                               reinterpret_cast<AFUNPTR>(MemoryAccessBefore),
                               IARG_INST_PTR, IARG_BOOL, false, IARG_UINT32,
                               mem_op, IARG_MEMORYOP_EA, mem_op, IARG_ADDRINT,
                               size, IARG_END);
    }

    if (INS_MemoryOperandIsWritten(instruction, mem_op)) {
      INS_InsertPredicatedCall(instruction, IPOINT_BEFORE,
                               reinterpret_cast<AFUNPTR>(MemoryAccessBefore),
                               IARG_INST_PTR, IARG_BOOL, true, IARG_UINT32,
                               mem_op, IARG_MEMORYOP_EA, mem_op, IARG_ADDRINT,
                               size, IARG_END);
    }

    // MemoryAccessAfter()
    if (INS_IsValidForIpointAfter(instruction)) {
      if (INS_MemoryOperandIsRead(instruction, mem_op)) {
        INS_InsertPredicatedCall(instruction, IPOINT_AFTER,
                                 reinterpret_cast<AFUNPTR>(MemoryAccessAfter),
                                 IARG_INST_PTR, IARG_BOOL, false, IARG_UINT32,
                                 mem_op, IARG_ADDRINT, size, IARG_END);
      }

      if (INS_MemoryOperandIsWritten(instruction, mem_op)) {
        INS_InsertPredicatedCall(instruction, IPOINT_AFTER,
                                 reinterpret_cast<AFUNPTR>(MemoryAccessAfter),
                                 IARG_INST_PTR, IARG_BOOL, true, IARG_UINT32,
                                 mem_op, IARG_ADDRINT, size, IARG_END);
      }
    }
  }
}

// Instrumentation routine run for every image loaded.
VOID OnImageLoad(IMG image, VOID *v) {
  // Find __libc_start_main so we can start analysis at main().
  RTN libcStartMainRoutine = RTN_FindByName(image, "__libc_start_main");

  if (RTN_Valid(libcStartMainRoutine)) {
    log_file << "\nFound __libc_start_main in image '" << IMG_Name(image)
             << "'\n";

    RTN_Open(libcStartMainRoutine);

    // Call LibcStartMainBefore() at the start of __libc_start_main.
    // Pass the first argument of __libc_start_main, i.e. the address of main().
    RTN_InsertCall(libcStartMainRoutine, IPOINT_BEFORE,
                   reinterpret_cast<AFUNPTR>(LibcStartMainBefore),
                   IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END);

    RTN_Close(libcStartMainRoutine);
  }

  // Find the malloc function.
  RTN mallocRoutine = RTN_FindByName(image, "malloc");

  if (RTN_Valid(mallocRoutine)) {
    log_file << "\nFound malloc in image '" << IMG_Name(image) << "'\n";

    RTN_Open(mallocRoutine);

    // Call MallocBefore() at the start of malloc().
    // Pass the first argument of malloc (i.e. the size), and the image ID.
    RTN_InsertCall(mallocRoutine, IPOINT_BEFORE,
                   reinterpret_cast<AFUNPTR>(MallocBefore), IARG_CONTEXT,
                   IARG_ADDRINT, IMG_Id(image), IARG_FUNCARG_ENTRYPOINT_VALUE,
                   0, IARG_END);

    // Call MallocAfter() at the end of malloc().
    // NOTE: This is implemented by instrumenting every return instruction in a
    // routine. Pin tries its best to find all of them, but this is not
    // guaranteed!
    // FIXME: For some reason, the return value is not printed for the first
    // call to malloc() in the application. Figure out why this is!
    RTN_InsertCall(mallocRoutine, IPOINT_AFTER,
                   reinterpret_cast<AFUNPTR>(MallocAfter),
                   IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);

    RTN_Close(mallocRoutine);
  }

  // Find the free function.
  RTN freeRoutine = RTN_FindByName(image, "free");

  if (RTN_Valid(freeRoutine)) {
    log_file << "\nFound free in image '" << IMG_Name(image) << "'\n";

    RTN_Open(freeRoutine);

    // Call FreeBefore() at the start of free().
    // Pass the first argument of free (i.e. the memory address of the buffer to
    // be freed), and the image ID.
    RTN_InsertCall(freeRoutine, IPOINT_BEFORE,
                   reinterpret_cast<AFUNPTR>(FreeBefore), IARG_ADDRINT,
                   IMG_Id(image), IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END);

    RTN_Close(freeRoutine);
  }

  // Find the debug annotation function.
  RTN debugAnnotationRoutine =
      RTN_FindByName(image, "MATE_FRAMEWORK_DEBUG_ANNOTATE_MEMORY");

  if (RTN_Valid(debugAnnotationRoutine)) {
    log_file << "\nFound MATE_FRAMEWORK_DEBUG_ANNOTATE_MEMORY in image '"
             << IMG_Name(image) << "'\n";

    RTN_Open(debugAnnotationRoutine);

    // Call AnnotateMemoryBefore at the start of
    // MATE_FRAMEWORK_DEBUG_ANNOTATE_MEMORY(). Pass all arguments
    // (i.e. the memory address to annotate, the size of the buffer, a pointer
    // to the annotation string, and the filename and line number).
    RTN_InsertCall(
        debugAnnotationRoutine, IPOINT_BEFORE,
        reinterpret_cast<AFUNPTR>(AnnotateMemoryBefore),
        IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 2, IARG_FUNCARG_ENTRYPOINT_VALUE, 3,
        IARG_FUNCARG_ENTRYPOINT_VALUE, 4, IARG_END);

    RTN_Close(debugAnnotationRoutine);
  }
}

// =============================================================================
// Machine-parsable output routines
// =============================================================================

// Dump the items in a container, separated by ',' to a CSV.
template <typename Container>
void dump_csv_list(std::ofstream &ofs, const Container &container) {
  bool first = true;

  for (const auto &item : container) {
    ofs << (first ? "" : ",") << item;
    first = false;
  }
}

// Dump the information for memory buffers/regions in CSV format.
template <typename Container>
void dump_csv_buffer_info(std::ofstream &ofs, const Container &container) {
  // Counter that is used as a unique identifier of a memory buffer/region.
  unsigned int id = 0;

  // Print header.
  ofs << "id,start_address,end_address,num_reads,num_writes,average_spatial_"
         "entropy_bit_shannon,average_spatial_entropy_byte_shannon,average_"
         "spatial_entropy_byte_shannon_adapted,average_spatial_entropy_byte_"
         "num_different,average_spatial_entropy_byte_num_unique,average_"
         "spatial_entropy_bit_average,average_spatial_entropy_byte_average,"
         "average_temporal_entropy_bit_shannon,read_ips,write_ips,"
         "allocation_address,DEBUG_allocation_backtrace,DEBUG_annotation\n";

  // Print data.
  for (const auto &p : container) {
    ofs << id++ << ',' << p.first.start_address << ',' << p.first.end_address
        << ',' << p.second.num_reads << ',' << p.second.num_writes << ','
        << p.second.spatial_entropy_info.get_average_bit_shannon_entropy()
        << ','
        << p.second.spatial_entropy_info.get_average_byte_shannon_entropy()
        << ','
        << p.second.spatial_entropy_info
               .get_average_byte_shannon_adapted_entropy()
        << ','
        << p.second.spatial_entropy_info
               .get_average_byte_num_different_entropy()
        << ','
        << p.second.spatial_entropy_info.get_average_byte_num_unique_entropy()
        << ','
        << p.second.spatial_entropy_info.get_average_bit_average_entropy()
        << ','
        << p.second.spatial_entropy_info.get_average_byte_average_entropy()
        << ','
        << p.second.temporal_entropy_info.get_average_bit_shannon_entropy()
        << ",";

    ofs << "\"";
    dump_csv_list(ofs, p.second.read_static_instructions);
    ofs << "\",";

    ofs << "\"";
    dump_csv_list(ofs, p.second.write_static_instructions);
    ofs << "\"";

    ofs << "," << p.second.allocation_address;

    ofs << ",\"" << p.second.DEBUG_allocation_backtrace << "\"";

    ofs << ",\"" << p.second.DEBUG_annotation << "\"";

    ofs << "\n";
  }
}

// Get the total number of bytes read/written from counters.
unsigned int get_total_byte_count(const unsigned int *counters) {
  unsigned int total_count = 0;

  for (std::size_t i = 0; i < 256; ++i) {
    total_count += counters[i];
  }

  return total_count;
}

// Calculate the entropy of byte counters.
// TODO: Refactor this and reuse this function in src/spatialentropy.cpp.
float calculate_shannon_entropy_from_byte_counters(
    const unsigned int *counters) {
  unsigned int total_count = get_total_byte_count(counters);

  float entropy = 0;
  for (std::size_t i = 0; i < 256; ++i) {
    unsigned int count = counters[i];

    if (count > 0) {
      float p = static_cast<float>(count) / static_cast<float>(total_count);
      entropy -= p * log2(p);
    }
  }

  entropy = entropy / log2(static_cast<float>(256));

  return entropy;
}

// Dump the information for static instructions in CSV format.
void dump_csv_static_instructions(
    std::ofstream &ofs, const std::map<ADDRINT, StaticInstructionInfo> &map) {

  // Print header.
  ofs << "ip,image_name,full_image_name,section_name,image_offset,routine_name,"
         "routine_offset,"
         "opcode,operands,"
         "read_values,written_values,read_values_entropy,written_values_"
         "entropy,num_bytes_read,num_bytes_written\n";

  // Print data.
  for (const auto &p : map) {
    ofs << p.first << ',' << get_filename(p.second.address.image_name) << ','
        << p.second.address.image_name << ',' << p.second.address.section_name
        << ',';

    // Print "-1" for unknown offsets.
    if (p.second.address.image_offset != static_cast<ADDRINT>(-1))
      ofs << p.second.address.image_offset << ',';
    else
      ofs << -1 << ',';

    ofs << p.second.address.routine_name << ',';

    // Print "-1" for unknown offsets.
    if (p.second.address.routine_offset != static_cast<ADDRINT>(-1))
      ofs << p.second.address.routine_offset << ',';
    else
      ofs << -1 << ',';

    ofs << p.second.opcode << ",\"" << p.second.operands << "\",";

    ofs << "\"";
    pretty_print_values(ofs, p.second.read_values);
    ofs << "\",";

    ofs << "\"";
    pretty_print_values(ofs, p.second.written_values);
    ofs << "\",";

    ofs << calculate_shannon_entropy_from_byte_counters(
               p.second.byte_counts_read)
        << ",";
    ofs << calculate_shannon_entropy_from_byte_counters(
               p.second.byte_counts_written)
        << ",";

    ofs << get_total_byte_count(p.second.byte_counts_read) << ",";
    ofs << get_total_byte_count(p.second.byte_counts_written) << "\n";
  }
}

// =============================================================================
// Other routines
// =============================================================================

// Finalizer routine.
VOID OnFinish(INT32 code, VOID *v) {
  // Move any buffers that are malloc'ed but not yet free'd to the free'd list,
  // and clear active buffer list.
  freed_mem_buf_infos.insert(freed_mem_buf_infos.end(),
                             active_mem_buf_infos.begin(),
                             active_mem_buf_infos.end());
  active_mem_buf_infos.clear();

  // -----------------------
  // Machine parsable output
  // -----------------------

  // Print information for static instructions.
  dump_csv_static_instructions(csv_instructions, static_instruction_infos);

  // Print info for memory buffers.
  dump_csv_buffer_info(csv_buffers, freed_mem_buf_infos);

  // Print info for memory regions.
  dump_csv_buffer_info(csv_regions, memory_regions_infos);

  // -------
  // Cleanup
  // -------

  // Flush and close the output file.
  log_file.flush();
  log_file.close();

  // Flush and close the CSV files.
  csv_instructions.flush();
  csv_instructions.close();

  csv_buffers.flush();
  csv_buffers.close();

  csv_regions.flush();
  csv_regions.close();
}

// This function is run when signal 15 (SIGTERM) is sent to the application.
BOOL OnSigTerm(THREADID thread_id, INT32 signal, CONTEXT *context,
               BOOL has_handler, const EXCEPTION_INFO *exception_info,
               VOID *v) {
  // Write data to file.
  OnFinish(0, nullptr);

  // Exit the application.
  PIN_ExitProcess(0);

  // Return false to squash signal, so the application does not receive it.
  // (This is not stricly needed, because we exit the application anyway.)
  return FALSE;
}

// Print usage information of the tool.
INT32 Usage() {
  std::cerr
      << "The purpose of this Pin tool is threefold:\n\n"
         "1. It finds memory buffers (malloc'ed memory) and memory regions "
         "(aligned and adjacent memory locations), their address range, the "
         "number of reads from/writes to them, and their spatial and temporal "
         "entropy.\n"
         "2. For instructions that read from/write to memory, it traces their "
         "address, opcode, operands, and the values read or written by them.\n"
         "3. It links memory buffers and regions to the instructions that read "
         "from or write to them.\n\n";
  std::cerr << KNOB_BASE::StringKnobSummary() << '\n';

  return EXIT_FAILURE;
}

int main(int argc, char *argv[]) {
  // Initialise the PIN symbol manager.
  PIN_InitSymbols();

  // Initialise Pin and SDE.
  sde_pin_init(argc, argv);
  sde_init();

  // Initialise Pin lock.
  PIN_InitLock(&pin_lock);

  // If we do not want to start at main() only, set 'main_reached' to true
  // already. This results in the entire executable being analysed.
  if (!KnobStartFromMain.Value())
    main_reached = true;

  // Open output file.
  log_file.open(KnobOutputFile.Value().c_str());

  // If the prefix for CSV files is not specified, just use the output filename.
  std::string csv_prefix = KnobCsvPrefix.Value();

  if (csv_prefix.empty()) {
    csv_prefix = KnobOutputFile.Value();
  }

  // Open the CSV files.
  csv_instructions.open((csv_prefix + ".instructions.csv").c_str());
  csv_buffers.open((csv_prefix + ".buffers.csv").c_str());
  csv_regions.open((csv_prefix + ".regions.csv").c_str());

  // Register instruction callback.
  INS_AddInstrumentFunction(OnInstruction, nullptr);

  // Register image load callback.
  IMG_AddInstrumentFunction(OnImageLoad, nullptr);

  // Register finish callback.
  PIN_AddFiniFunction(OnFinish, nullptr);

  // Prevent the application from blocking SIGTERM.
  PIN_UnblockSignal(15, true);

  // Intercept SIGTERM so we can kill the application and still obtain results.
  PIN_InterceptSignal(15, OnSigTerm, nullptr);

  // Start the program (never returns).
  PIN_StartProgram();

  return EXIT_SUCCESS;
}
