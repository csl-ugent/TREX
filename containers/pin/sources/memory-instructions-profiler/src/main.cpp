#include <cmath>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

#include "pin.H"
#include "sde-init.H"

// File stream used to write the human-readable log output to.
static std::ofstream log_file;

// File stream to write the CSV output to.
static std::ofstream csv_memory_instructions;

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
    KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "output",
                   "memoryinstructionsprofiler.log",
                   "Specify the filename of the human-readable log file");

// Option (-csv_prefix) to set the prefix used for the CSV output file.
KNOB<std::string>
    KnobCsvPrefix(KNOB_MODE_WRITEONCE, "pintool", "csv_prefix", "",
                  "Set the prefix used for the CSV output file. The output "
                  "file will be of the form <prefix>.memory-instructions.csv.");

// Option (-start_from_main) to only start analysis from the point that main()
// is called.
KNOB<bool> KnobStartFromMain(
    KNOB_MODE_WRITEONCE, "pintool", "start_from_main", "1",
    "When true, starts analysis from the point that main() is called");

// Option (-end_after_main) to end analysis after main() is finished.
KNOB<bool>
    KnobEndAfterMain(KNOB_MODE_WRITEONCE, "pintool", "end_after_main", "1",
                     "When true, ends analysis after main() is finished");

// Option (-instruction_values_limit) that determines how many unique values to
// keep per static instruction.
KNOB<std::size_t> KnobInstructionValuesLimit(
    KNOB_MODE_WRITEONCE, "pintool", "instruction_values_limit", "5",
    "Number of unique read/written values to keep per static instruction.");

// The address of an instruction.
class StaticInstructionAddress {
public:
  // Creates a new StaticInstructionAddress corresponding to the given
  // instruction address.
  StaticInstructionAddress(ADDRINT address)
      : image_name("???"), section_name("???"), routine_name("???"),
        image_offset(-1), routine_offset(-1) {
    // Routine info.
    RTN routine = RTN_FindByAddress(address);
    if (!RTN_Valid(routine)) {
      return;
    }

    this->routine_name = RTN_Name(routine);
    this->routine_offset = address - RTN_Address(routine);

    // Section info.
    SEC section = RTN_Sec(routine);
    if (!SEC_Valid(section)) {
      return;
    }

    this->section_name = SEC_Name(section);

    // Image info.
    IMG image = SEC_Img(section);
    if (!IMG_Valid(image)) {
      return;
    }

    this->image_name = IMG_Name(image);
    this->image_offset = address - IMG_LowAddress(image);
  }

  // The name of the image this instruction belongs to.
  std::string image_name;

  // The name of the section this instruction belongs to.
  std::string section_name;

  // The name of the routine this instruction belongs to.
  std::string routine_name;

  // The offset of the instruction address relative to the base of the image.
  ADDRINT image_offset;

  // The offset of the instruction address relative to the base of the routine.
  ADDRINT routine_offset;
};

// Contains information for reads or writes of an instruction.
struct ReadWriteInfo {
  ReadWriteInfo() : values(), byte_counts(), byte_addresses() {}

  // A map that maps values read/written by the instruction to the number of
  // occurrences of that value. Each element is itself a vector, representing
  // the different bytes of the read/written value.
  std::map<std::vector<unsigned char>, unsigned int> values;

  // Counters for the number of times a given byte value was read or written.
  unsigned int byte_counts[256];

  // A set of unique byte addresses that this instruction read from or wrote to.
  std::set<ADDRINT> byte_addresses;
};

// Contains the information for each memory instruction.
struct MemoryInstructionInfo {
  MemoryInstructionInfo(ADDRINT address)
      : address(address), num_executions(0), read_info(), write_info() {}

  // The address of the memory instruction.
  StaticInstructionAddress address;

  // The number of times this instruction was executed.
  unsigned int num_executions;

  // Information on reads.
  ReadWriteInfo read_info;

  // Information on writes.
  ReadWriteInfo write_info;
};

// Maps the address of a memory instruction to its info.
static std::map<ADDRINT, MemoryInstructionInfo> memory_instruction_infos;

// Mutex to control accesses to memory_instruction_infos.
static PIN_MUTEX memory_instruction_infos_lock;

// Remembers the effective address of memory operands.
// This is needed because Pin doesn't allow us to access the effective address
// in the analysis routine _after_ a memory instruction.
// Key = (thread, memory operand index), value = effective address.
static std::map<std::pair<THREADID, UINT32>, ADDRINT> memory_operands_map;

// Mutex to control access to memory_operands_map.
static PIN_MUTEX memory_operands_map_lock;

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

// Helper function to update read_info or write_info.
void UpdateReadWriteInfo(ReadWriteInfo &info, ADDRINT memory_address,
                         ADDRINT size) {
  // Copy the read/written value.
  unsigned char *value_buf = new unsigned char[size];
  PIN_SafeCopy(value_buf, reinterpret_cast<void *>(memory_address), size);

  // Update the set of read/written values.
  if (info.values.size() < KnobInstructionValuesLimit.Value()) {
    info.values[std::vector<unsigned char>(value_buf, value_buf + size)]++;
  }

  // Update the byte counters.
  for (ADDRINT i = 0; i < size; ++i) {
    ++info.byte_counts[value_buf[i]];
  }

  // Cleanup value_buf array.
  delete[] value_buf;

  // Update the set of unique byte addresses.
  for (ADDRINT i = 0; i < size; ++i) {
    info.byte_addresses.insert(memory_address + i);
  }
}

// Run before every memory access, i.e. both read and write.
VOID MemoryAccessBefore(THREADID threadID, ADDRINT instruction_address,
                        BOOL is_write, UINT32 mem_op, ADDRINT memory_address,
                        ADDRINT size) {
  if (!main_reached || end_reached)
    return;

  // Store the memory address so we can reuse it later in MemoryAccessAfter.
  PIN_MutexLock(&memory_operands_map_lock);
  memory_operands_map[std::make_pair(threadID, mem_op)] = memory_address;
  PIN_MutexUnlock(&memory_operands_map_lock);

  // Update instruction info for reads.
  if (!is_write) {
    // Find the instruction info.
    PIN_MutexLock(&memory_instruction_infos_lock);
    auto it = memory_instruction_infos.find(instruction_address);

    if (it != memory_instruction_infos.end()) {
      // Update the num_executions counter.
      ++it->second.num_executions;

      // Update the read_info.
      UpdateReadWriteInfo(it->second.read_info, memory_address, size);
    } else {
      assert(false &&
             "Memory instruction not added to memory_instruction_infos!");
    }

    PIN_MutexUnlock(&memory_instruction_infos_lock);
  }
}

// Run after every memory access, i.e. both read and write.
VOID MemoryAccessAfter(THREADID threadID, ADDRINT instruction_address,
                       BOOL is_write, UINT32 mem_op, ADDRINT size) {
  if (!main_reached || end_reached)
    return;

  // Retrieve the memory address stored in the memory_operands_map.
  PIN_MutexLock(&memory_operands_map_lock);
  const ADDRINT memory_address =
      memory_operands_map[std::make_pair(threadID, mem_op)];
  PIN_MutexUnlock(&memory_operands_map_lock);

  // Update instruction info for writes.
  if (is_write) {
    // Find the instruction info.
    PIN_MutexLock(&memory_instruction_infos_lock);
    auto it = memory_instruction_infos.find(instruction_address);

    if (it != memory_instruction_infos.end()) {
      // Update the num_executions counter.
      ++it->second.num_executions;

      // Update the write_info.
      UpdateReadWriteInfo(it->second.write_info, memory_address, size);
    } else {
      assert(false &&
             "Memory instruction not added to memory_instruction_infos!");
    }
    PIN_MutexUnlock(&memory_instruction_infos_lock);
  }
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

  /* Insert analysis calls for memory-related instructions. */

  // Get the number of memory operands this instruction has.
  UINT32 num_mem_operands = INS_MemoryOperandCount(instruction);

  // Create a new entry in the memory instruction map for instructions that read
  // from/write to memory.
  if (num_mem_operands > 0) {
    ADDRINT ins_addr = INS_Address(instruction);

    PIN_MutexLock(&memory_instruction_infos_lock);
    memory_instruction_infos.insert(
        {ins_addr, MemoryInstructionInfo(ins_addr)});
    PIN_MutexUnlock(&memory_instruction_infos_lock);
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
                               IARG_THREAD_ID, IARG_INST_PTR, IARG_BOOL, false,
                               IARG_UINT32, mem_op, IARG_MEMORYOP_EA, mem_op,
                               IARG_ADDRINT, size, IARG_END);
    }

    if (INS_MemoryOperandIsWritten(instruction, mem_op)) {
      INS_InsertPredicatedCall(instruction, IPOINT_BEFORE,
                               reinterpret_cast<AFUNPTR>(MemoryAccessBefore),
                               IARG_THREAD_ID, IARG_INST_PTR, IARG_BOOL, true,
                               IARG_UINT32, mem_op, IARG_MEMORYOP_EA, mem_op,
                               IARG_ADDRINT, size, IARG_END);
    }

    // MemoryAccessAfter()
    if (INS_IsValidForIpointAfter(instruction)) {
      if (INS_MemoryOperandIsRead(instruction, mem_op)) {
        INS_InsertPredicatedCall(instruction, IPOINT_AFTER,
                                 reinterpret_cast<AFUNPTR>(MemoryAccessAfter),
                                 IARG_THREAD_ID, IARG_INST_PTR, IARG_BOOL,
                                 false, IARG_UINT32, mem_op, IARG_ADDRINT, size,
                                 IARG_END);
      }

      if (INS_MemoryOperandIsWritten(instruction, mem_op)) {
        INS_InsertPredicatedCall(instruction, IPOINT_AFTER,
                                 reinterpret_cast<AFUNPTR>(MemoryAccessAfter),
                                 IARG_THREAD_ID, IARG_INST_PTR, IARG_BOOL, true,
                                 IARG_UINT32, mem_op, IARG_ADDRINT, size,
                                 IARG_END);
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
}

// =============================================================================
// Machine-parsable output routines
// =============================================================================

// Get the filename from an absolute path. For example, /path/to/test.so becomes
// test.so.
std::string get_filename(const std::string &path) {
  // Look for last path separator in path string.
  auto last_path_sep = path.rfind('/');

  if (last_path_sep != std::string::npos) {
    // Return string after last path separator.
    return path.substr(last_path_sep + 1);
  } else {
    // No path separator found, so just return entire string.
    return path;
  }
}

// Dump the list of read/written values in CSV format.
void dump_csv_value_list(
    std::ofstream &ofs,
    const std::map<std::vector<unsigned char>, unsigned int> &values) {
  std::ostringstream oss;
  oss << std::right << std::noshowbase << std::hex << std::setfill('0');

  bool first = true;

  for (const auto &entry : values) {
    const auto &val = entry.first;
    const auto &count = entry.second;

    oss << (first ? "" : ", ");
    first = false;

    bool first_byte = true;

    for (const auto &byte : val) {
      oss << std::hex << (first_byte ? "" : " ") << std::setw(2)
          << static_cast<unsigned int>(byte);
      first_byte = false;
    }

    oss << " (occurs " << std::dec << count << " time(s))";
  }

  ofs << "\"[";
  ofs << oss.str();
  ofs << "]\"";
}

// Dump the information for read/write info in CSV format.
void dump_csv_readwrite_info(std::ofstream &ofs, const ReadWriteInfo &info) {
  // {...}_values
  dump_csv_value_list(ofs, info.values);

  // Get the total amount of bytes read/written by this instruction.
  std::size_t total_count = 0;
  for (std::size_t i = 0; i < 256; ++i) {
    total_count += info.byte_counts[i];
  }

  // Calculate byte-shannon entropy
  float entropy = 0;

  for (std::size_t i = 0; i < 256; ++i) {
    auto count = info.byte_counts[i];

    if (count > 0) {
      float p = static_cast<float>(count) / static_cast<float>(total_count);
      entropy -= p * std::log2(p);
    }
  }

  entropy = entropy / std::log2(static_cast<float>(256));

  ofs << ',' << entropy;                    // {...}_values_entropy
  ofs << ',' << total_count;                // num_bytes_{...}
  ofs << ',' << info.byte_addresses.size(); // num_unique_byte_addresses_{...}
}

// Dump the information for memory instructions in CSV format.
void dump_csv_memory_instructions(
    std::ofstream &ofs, const std::map<ADDRINT, MemoryInstructionInfo> &map) {
  // Print header.
  ofs << "ip,image_name,full_image_name,section_name,image_offset,routine_name,"
         "routine_offset,"
         "read_values,read_values_entropy,num_bytes_read,num_unique_byte_"
         "addresses_read,"
         "written_values,written_values_entropy,num_bytes_written,num_unique_"
         "byte_addresses_written,"
         "num_executions\n";

  // Print data.
  for (const auto &p : map) {
    ofs << p.first                                          // ip
        << ',' << get_filename(p.second.address.image_name) // image_name
        << ',' << p.second.address.image_name               // full_image_name
        << ',' << p.second.address.section_name             // section_name
        << ',';

    // Print "-1" for unknown offsets.
    if (p.second.address.image_offset != static_cast<ADDRINT>(-1))
      ofs << p.second.address.image_offset << ','; // image_offset
    else
      ofs << -1 << ',';

    ofs << p.second.address.routine_name // routine_name
        << ',';

    // Print "-1" for unknown offsets.
    if (p.second.address.routine_offset != static_cast<ADDRINT>(-1))
      ofs << p.second.address.routine_offset << ','; // routine_offset
    else
      ofs << -1 << ',';

    // Print info for reads.
    dump_csv_readwrite_info(ofs, p.second.read_info);

    ofs << ',';

    // Print info for writes.
    dump_csv_readwrite_info(ofs, p.second.write_info);

    ofs << ',' << p.second.num_executions; // num_executions

    ofs << '\n';
  }
}

// =============================================================================
// Other routines
// =============================================================================

// Finalizer routine.
VOID OnFinish(INT32 code, VOID *v) {
  // -----------------------
  // Machine parsable output
  // -----------------------

  dump_csv_memory_instructions(csv_memory_instructions,
                               memory_instruction_infos);

  // -------
  // Cleanup
  // -------

  // Flush and close the output file.
  log_file.flush();
  log_file.close();

  // Flush and close the CSV file.
  csv_memory_instructions.flush();
  csv_memory_instructions.close();
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
      << "This Pin tool profiles memory instructions, i.e. instructions that "
         "read from or write to memory.\n"
         "For each memory instruction, it collects the values read/written, "
         "their entropy, and some statistics about the instruction.\n\n";

  std::cerr << KNOB_BASE::StringKnobSummary() << '\n';

  return EXIT_FAILURE;
}

int main(int argc, char *argv[]) {
  // Initialise the PIN symbol manager.
  PIN_InitSymbols();

  // Initialise Pin and SDE.
  sde_pin_init(argc, argv);
  sde_init();

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
  csv_memory_instructions.open(
      (csv_prefix + ".memory-instructions.csv").c_str());

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

  // Initialise mutexes.
  PIN_MutexInit(&memory_instruction_infos_lock);
  PIN_MutexInit(&memory_operands_map_lock);

  // Start the program (never returns).
  PIN_StartProgram();

  return EXIT_SUCCESS;
}
