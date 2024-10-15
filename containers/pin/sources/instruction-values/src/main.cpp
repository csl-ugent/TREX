#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "pin.H"
#include "sde-init.H"

// File stream used to write the human-readable log output to.
static std::ofstream log_file;

// File stream to write the CSV output to.
static std::ofstream csv_instruction_values;

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
                   "instructionvalues.log",
                   "Specify the filename of the human-readable log file");

// Option (-csv_prefix) to set the prefix used for the CSV output file.
KNOB<std::string>
    KnobCsvPrefix(KNOB_MODE_WRITEONCE, "pintool", "csv_prefix", "",
                  "Set the prefix used for the CSV output file. The output "
                  "file will be of the form <prefix>.instruction-values.csv.");

// Option (-start_from_main) to only start analysis from the point that main()
// is called.
KNOB<bool> KnobStartFromMain(
    KNOB_MODE_WRITEONCE, "pintool", "start_from_main", "1",
    "When true, starts analysis from the point that main() is called");

// Option (-end_after_main) to end analysis after main() is finished.
KNOB<bool>
    KnobEndAfterMain(KNOB_MODE_WRITEONCE, "pintool", "end_after_main", "1",
                     "When true, ends analysis after main() is finished");

// Option (-instruction_values_limit) to set the maximum number of unique
// read/written values stored per instruction operand.
KNOB<std::size_t> KnobInstructionValuesLimit(
    KNOB_MODE_WRITEONCE, "pintool", "instruction_values_limit", "-1",
    "The maximum number of unique read/written "
    "values stored per instruction operand. Set to '-1' to store ALL values.");

// Option (-range_image) that determines the image name of ranges of
// instructions to profile.
KNOB<std::string> KnobRangeImage(
    KNOB_MODE_APPEND, "pintool", "range_image", "",
    "The name of the image of the instruction range. Both the beginning and "
    "end of the range are instructions in this image. Note that you can repeat "
    "this option multiple times to specify multiple ranges. Also see: "
    "-range_begin_offset and -range_end_offset.");

// Option (-range_begin_offset) that determines the image offset of the begin
// instruction of instruction ranges to profile.
KNOB<ADDRINT> KnobRangeBeginOffset(
    KNOB_MODE_APPEND, "pintool", "range_begin_offset", "",
    "The offset of the beginning instruction of the instruction range. Note "
    "that you can repeat this option multiple times to specify multiple "
    "ranges. Also see: -range_image and -range_end_offset.");

// Option (-range_end_offset) that determines the image offset of the
// beyond-the-end instruction of instruction ranges to profile.
KNOB<ADDRINT> KnobRangeEndOffset(
    KNOB_MODE_APPEND, "pintool", "range_end_offset", "",
    "The offset of the beyond-the-end instruction of the instruction range. "
    "Note that you can repeat this option multiple times to specify multiple "
    "ranges. Also see: -range_image and -range_begin_offset.");

// Option (-ranges_file) that contains the path to a file that determines which
// instruction ranges to collect data for.
KNOB<std::string> KnobRangesFile(
    KNOB_MODE_WRITEONCE, "pintool", "ranges_file", "",
    "Set the file containing the instruction ranges to collect data for. Each "
    "line in this file represents a range and should contain the image name, "
    "begin offset, and range offset separated by whitespace. This option is an "
    "alternative for -range_image, -range_begin_offset, and -range_end_offset "
    "when you want to specify more instruction ranges than allowed by the "
    "maximum argument length.");

// Represents an instruction range, i.e. the set of instructions inside an
// image, and with an offset in the interval [begin, end[.
struct InstructionRange {
  InstructionRange(const std::string &image_name, ADDRINT begin_offset,
                   ADDRINT end_offset)
      : image_name(image_name), begin_offset(begin_offset),
        end_offset(end_offset) {}

  std::string image_name; // The name of the image of begin and end.
  ADDRINT begin_offset;   // The offset of the begin instruction.
  ADDRINT end_offset;     // The offset of the end instruction.

  bool operator<(const InstructionRange &other) const {
    if (this->image_name != other.image_name) {
      return this->image_name < other.image_name;
    }

    if (this->begin_offset != other.begin_offset) {
      return this->begin_offset < other.begin_offset;
    }

    return this->end_offset < other.end_offset;
  }
};

// A set of instruction ranges to profile.
static std::set<InstructionRange> instruction_ranges;

// The data for each operand of an instruction.
struct OperandInfo {
  OperandInfo()
      : repr("???"), width(0), is_read(false), is_written(false), read_values(),
        written_values() {}

  std::string repr; // A string representation of the operand
  UINT32 width;     // The width of the operand in bits
  bool is_read;     // True if the operand was read; false otherwise.
  bool is_written;  // True if the operand was written; false otherwise.
  std::map<std::vector<unsigned char>, unsigned int>
      read_values; // The set of values of this operand, sampled
                   // _before_ the instruction.
  std::map<std::vector<unsigned char>, unsigned int>
      written_values; // The set of values of this operand,
                      // sampled _after_ the instruction.
};

// The data for each instruction.
struct InstructionInfo {
  // NOTE: Keep this in sync with the value in modules/instruction_values.py
  static constexpr std::size_t MAX_NUM_OPERANDS =
      4; // Maximum number of operands that will be tracked.

  InstructionInfo() : InstructionInfo("???", 0) {}

  InstructionInfo(const std::string &image_name, ADDRINT image_offset)
      : image_name(image_name), image_offset(image_offset), num_operands(0) {}

  std::string image_name; // Name of the image.
  ADDRINT image_offset;   // Offset from the start of the image.
  UINT32 num_operands;    // The number of register+memory operands of this
                          // instruction.

  OperandInfo operands[MAX_NUM_OPERANDS];
};

// Maps an instruction address (value of RIP) to its information.
std::map<ADDRINT, InstructionInfo> instruction_infos;

// Mutex to control access to instruction_infos.
static PIN_MUTEX instruction_infos_lock;

// Remembers the effective address of memory operands.
// Key = (threadID, operand index), value = effective address.
std::map<std::pair<THREADID, UINT32>, ADDRINT> memory_operands_map;

// Mutex to control access to memory_operands_map.
static PIN_MUTEX memory_operands_map_lock;

// =============================================================================
// Helper routines
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

// Run before an instruction, for every read from a register.
VOID InstructionReadRegisterBefore(ADDRINT instruction_address,
                                   ADDRINT operand_index, UINT8 *reg_value,
                                   ADDRINT reg_size) {
  if (!main_reached || end_reached)
    return;

  PIN_MutexLock(&instruction_infos_lock);

  auto &value_set = instruction_infos[instruction_address]
                        .operands[operand_index]
                        .read_values;

  if (value_set.size() < KnobInstructionValuesLimit.Value()) {
    value_set[std::vector<unsigned char>(&reg_value[0],
                                         &reg_value[reg_size])]++;
  }

  PIN_MutexUnlock(&instruction_infos_lock);
}

// Run before an instruction, for every read from memory.
VOID InstructionReadMemoryBefore(ADDRINT instruction_address,
                                 ADDRINT operand_index, ADDRINT memoryop_ea,
                                 ADDRINT memoryop_size) {
  if (!main_reached || end_reached)
    return;

  PIN_MutexLock(&instruction_infos_lock);

  auto &value_set = instruction_infos[instruction_address]
                        .operands[operand_index]
                        .read_values;

  if (value_set.size() < KnobInstructionValuesLimit.Value()) {
    unsigned char *buf = new unsigned char[memoryop_size];
    PIN_SafeCopy(buf, reinterpret_cast<const void *>(memoryop_ea),
                 memoryop_size);
    value_set[std::vector<unsigned char>(buf, buf + memoryop_size)]++;
    delete[] buf;
  }

  PIN_MutexUnlock(&instruction_infos_lock);
}

// Run after an instruction, for every write to a register.
VOID InstructionWriteRegisterAfter(ADDRINT instruction_address,
                                   ADDRINT operand_index, UINT8 *reg_value,
                                   ADDRINT reg_size) {
  if (!main_reached || end_reached)
    return;

  PIN_MutexLock(&instruction_infos_lock);

  auto &value_set = instruction_infos[instruction_address]
                        .operands[operand_index]
                        .written_values;

  if (value_set.size() < KnobInstructionValuesLimit.Value()) {
    value_set[std::vector<unsigned char>(&reg_value[0],
                                         &reg_value[reg_size])]++;
  }

  PIN_MutexUnlock(&instruction_infos_lock);
}

// Run before an instruction, for every write to memory. This routine is only
// used to store the effective address of the memory operand.
VOID InstructionWriteMemoryBefore(THREADID thread_id, ADDRINT operand_index,
                                  ADDRINT memoryop_ea) {
  if (!main_reached || end_reached)
    return;

  // Store effective address of this memory operand, so we can reuse it later in
  // InstructionWriteMemoryAfter.
  PIN_MutexLock(&memory_operands_map_lock);
  memory_operands_map[std::make_pair(thread_id, operand_index)] = memoryop_ea;
  PIN_MutexUnlock(&memory_operands_map_lock);
}

// Run after an instruction, for every write to memory.
VOID InstructionWriteMemoryAfter(THREADID thread_id,
                                 ADDRINT instruction_address,
                                 ADDRINT operand_index, ADDRINT memoryop_size) {
  if (!main_reached || end_reached)
    return;

  // Obtain the effective address of this memory operand, stored by
  // InstructionReadMemoryBefore. We need to do this because IARG_MEMORYOP_EA is
  // only valid at IPOINT_BEFORE.
  PIN_MutexLock(&memory_operands_map_lock);
  ADDRINT memoryop_ea =
      memory_operands_map[std::make_pair(thread_id, operand_index)];
  PIN_MutexUnlock(&memory_operands_map_lock);

  PIN_MutexLock(&instruction_infos_lock);

  auto &value_set = instruction_infos[instruction_address]
                        .operands[operand_index]
                        .written_values;

  if (value_set.size() < KnobInstructionValuesLimit.Value()) {
    unsigned char *buf = new unsigned char[memoryop_size];
    PIN_SafeCopy(buf, reinterpret_cast<const void *>(memoryop_ea),
                 memoryop_size);
    value_set[std::vector<unsigned char>(buf, buf + memoryop_size)]++;
    delete[] buf;
  }

  PIN_MutexUnlock(&instruction_infos_lock);
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

  // Get instruction address.
  const ADDRINT instruction_address = INS_Address(instruction);

  // Get the image name and offset.
  IMG img = IMG_FindByAddress(instruction_address);
  std::string image_name = IMG_Valid(img) ? get_filename(IMG_Name(img)) : "???";
  ADDRINT image_offset =
      IMG_Valid(img) ? (instruction_address - IMG_LowAddress(img)) : -1;

  // Only profile instructions that are specified in instruction_ranges.
  bool should_profile = false;

  // If there are no ranges specified, just profile the entire executable.
  if (instruction_ranges.empty()) {
    should_profile = true;
  }

  // In the other case, just check if at least one range contains this
  // instruction.
  for (const auto &range : instruction_ranges) {
    if ((image_name == range.image_name) &&
        (range.begin_offset <= image_offset) &&
        (image_offset < range.end_offset)) {
      should_profile = true;
      break; // No need to continue searching.
    }
  }

  // Only profile instructions that we are interested in.
  if (should_profile) {
    // Fill in instruction info, if it doesn't exist already.
    PIN_MutexLock(&instruction_infos_lock);
    instruction_infos.insert(std::make_pair(
        instruction_address, InstructionInfo(image_name, image_offset)));
    PIN_MutexUnlock(&instruction_infos_lock);

    // Remember the next free slot for operands.
    std::size_t next_free_operand_slot = 0;

    PIN_MutexLock(&instruction_infos_lock);

    for (UINT32 i = 0; i < INS_OperandCount(instruction); ++i) {
      if (INS_OperandIsReg(instruction, i)) {
        REG reg = INS_OperandReg(instruction, i);

        // Skip invalid registers.
        if (!REG_valid(reg))
          continue;

        // Ensure that we still have slots available.
        assert(next_free_operand_slot < InstructionInfo::MAX_NUM_OPERANDS &&
               "No more operand slots available!");

        const bool is_read = INS_OperandRead(instruction, i);
        const bool is_written = INS_OperandWritten(instruction, i);

        // Fill in operand info.
        OperandInfo &op_info = instruction_infos[instruction_address]
                                   .operands[next_free_operand_slot];

        op_info.repr = REG_StringShort(reg);
        op_info.width = INS_OperandWidth(instruction, i);
        op_info.is_read = is_read;
        op_info.is_written = is_written;

        // Register instrumentation routines.
        if (is_read) {
          INS_InsertPredicatedCall(
              instruction, IPOINT_BEFORE,
              reinterpret_cast<AFUNPTR>(InstructionReadRegisterBefore),

              IARG_INST_PTR, // ADDRINT instruction_address
              IARG_ADDRINT, next_free_operand_slot, // ADDRINT operand_index
              IARG_REG_CONST_REFERENCE, reg,        // UINT8* reg_value
              IARG_ADDRINT,
              static_cast<ADDRINT>(REG_Size(reg)), // ADDRINT reg_size
              IARG_END);
        }

        if (is_written && INS_IsValidForIpointAfter(instruction)) {
          INS_InsertPredicatedCall(
              instruction, IPOINT_AFTER,
              reinterpret_cast<AFUNPTR>(InstructionWriteRegisterAfter),

              IARG_INST_PTR, // ADDRINT instruction_address
              IARG_ADDRINT, next_free_operand_slot, // ADDRINT operand_index
              IARG_REG_CONST_REFERENCE, reg,        // UINT8* reg_value
              IARG_ADDRINT,
              static_cast<ADDRINT>(REG_Size(reg)), // ADDRINT reg_size
              IARG_END);
        }

        // Increment next free slot.
        ++next_free_operand_slot;
      } else if (INS_OperandIsMemory(instruction, i)) {
        // Ensure that we still have slots available.
        assert(next_free_operand_slot < InstructionInfo::MAX_NUM_OPERANDS &&
               "No more operand slots available!");

        REG base = INS_OperandMemoryBaseReg(instruction, i);
        REG index = INS_OperandMemoryIndexReg(instruction, i);
        REG segment = INS_OperandMemorySegmentReg(instruction, i);
        auto scale = INS_OperandMemoryScale(instruction, i);
        auto disp = INS_OperandMemoryDisplacement(instruction, i);

        std::ostringstream oss;

        // segment:[base + (index * scale) + displacement]
        oss << REG_StringShort(segment) << ":[" << REG_StringShort(base)
            << " + (" << REG_StringShort(index) << " * " << scale << ") + "
            << disp << "]";

        const bool is_read = INS_OperandRead(instruction, i);
        const bool is_written = INS_OperandWritten(instruction, i);

        // Fill in operand info.
        OperandInfo &op_info = instruction_infos[instruction_address]
                                   .operands[next_free_operand_slot];

        op_info.repr = oss.str();
        op_info.width = INS_OperandWidth(instruction, i);
        op_info.is_read = is_read;
        op_info.is_written = is_written;

        // Find the memory operand index corresponding to this operand index.
        UINT32 memory_operand_index = -1;

        for (UINT32 mem_op = 0; mem_op < INS_MemoryOperandCount(instruction);
             ++mem_op) {
          if (INS_MemoryOperandIndexToOperandIndex(instruction, mem_op) == i) {
            memory_operand_index = mem_op;
          }
        }

        // Check if the memory operand is found.
        if (memory_operand_index != static_cast<UINT32>(-1)) {
          // Register instrumentation routines.
          if (is_read) {
            INS_InsertPredicatedCall(
                instruction, IPOINT_BEFORE,
                reinterpret_cast<AFUNPTR>(InstructionReadMemoryBefore),
                IARG_INST_PTR, // ADDRINT instruction_address
                IARG_ADDRINT, next_free_operand_slot,   // ADDRINT operand_index
                IARG_MEMORYOP_EA, memory_operand_index, // ADDRINT memoryop_ea
                IARG_ADDRINT,
                static_cast<ADDRINT>(INS_MemoryOperandSize(
                    instruction,
                    memory_operand_index)), // ADDRINT memoryop_size
                IARG_END);
          }

          if (is_written) {
            if (INS_IsValidForIpointAfter(instruction)) {
              // Store the effective address in memory_operands_map.
              INS_InsertPredicatedCall(
                  instruction, IPOINT_BEFORE,
                  reinterpret_cast<AFUNPTR>(InstructionWriteMemoryBefore),
                  IARG_THREAD_ID,                       // THREADID thread_id
                  IARG_ADDRINT, next_free_operand_slot, // ADDRINT operand_index
                  IARG_MEMORYOP_EA, memory_operand_index, // ADDRINT memoryop_ea
                  IARG_END);

              INS_InsertPredicatedCall(
                  instruction, IPOINT_AFTER,
                  reinterpret_cast<AFUNPTR>(InstructionWriteMemoryAfter),
                  IARG_THREAD_ID, // THREADID thread_id
                  IARG_INST_PTR,  // ADDRINT instruction_address
                  IARG_ADDRINT, next_free_operand_slot, // ADDRINT operand_index
                  IARG_ADDRINT,
                  static_cast<ADDRINT>(INS_MemoryOperandSize(
                      instruction,
                      memory_operand_index)), // ADDRINT memoryop_size
                  IARG_END);
            } else {
              log_file << "Cannot add predicated call to "
                          "InstructionWriteMemoryAfter "
                          "at IPOINT_AFTER for instruction '"
                       << INS_Disassemble(instruction) << "' at " << image_name
                       << "+" << std::hex << image_offset << std::dec << "\n";
            }
          }
        } else {
          log_file << "Could not find memory operand for operand " << i
                   << " of instruction '" << INS_Disassemble(instruction)
                   << "' at " << image_name << "+" << std::hex << image_offset
                   << std::dec << "\n";
        }

        // Increment next free slot.
        ++next_free_operand_slot;
      }
    }

    // Set the number of operands for this instruction.
    instruction_infos[instruction_address].num_operands =
        next_free_operand_slot;

    PIN_MutexUnlock(&instruction_infos_lock);
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

// Pretty print the read/written values.
void pretty_print_value_list(
    std::ofstream &ofs,
    const std::map<std::vector<unsigned char>, unsigned int> &values) {
  ofs << '"';

  bool first_value = true;
  for (const auto &entry : values) {
    const auto &value = entry.first;
    const auto &count = entry.second;

    ofs << (first_value ? "" : ",");
    first_value = false;

    bool first_byte = true;
    for (const unsigned char byte : value) {
      std::ostringstream oss;
      oss << std::hex << std::setw(2) << std::setfill('0')
          << static_cast<unsigned int>(byte);
      ofs << (first_byte ? "" : " ") << oss.str();
      first_byte = false;
    }

    ofs << " (occurs " << std::dec << count << " time(s))";
  }

  ofs << '"';
}

// Dump the information for instruction values in CSV format.
void dump_csv_instruction_values(
    std::ofstream &ofs,
    const std::map<ADDRINT, InstructionInfo> &instruction_infos) {
  // Print header.
  ofs << "image_name,image_offset,num_operands";

  for (std::size_t i = 0; i < InstructionInfo::MAX_NUM_OPERANDS; ++i) {
    ofs << ",operand_" << i << "_repr";
    ofs << ",operand_" << i << "_width";
    ofs << ",operand_" << i << "_is_read";
    ofs << ",operand_" << i << "_is_written";
    ofs << ",operand_" << i << "_read_values";
    ofs << ",operand_" << i << "_written_values";
  }

  ofs << "\n";

  // Print data.
  for (const auto &p : instruction_infos) {
    const InstructionInfo &info = p.second;
    ofs << info.image_name; // image_name

    // Print "-1" for unknown offsets.
    if (info.image_offset != static_cast<ADDRINT>(-1))
      ofs << ',' << info.image_offset; // image_offset
    else
      ofs << ',' << -1; // image_offset

    ofs << ',' << info.num_operands; // num_operands

    for (std::size_t i = 0; i < InstructionInfo::MAX_NUM_OPERANDS; ++i) {
      if (i < info.num_operands) {
        // Filled in operand.
        const OperandInfo &op = info.operands[i];

        ofs << ',' << '"' << op.repr << '"' // operand_<i>_repr
            << ',' << op.width              // operand_<i>_width
            << ',' << op.is_read            // operand_<i>_is_read
            << ',' << op.is_written;        // operand_<i>_is_written

        // operand_<i>_read_values
        ofs << ',';
        pretty_print_value_list(ofs, op.read_values);

        // operand_<i>_written_values
        ofs << ',';
        pretty_print_value_list(ofs, op.written_values);
      } else {
        // Operand not filled in.
        ofs << ',' << "\"\""  // operand_<i>_repr
            << ',' << 0       // operand_<i>_width
            << ',' << false   // operand_<i>_is_read
            << ',' << false   // operand_<i>_is_written
            << ',' << "\"\""  // operand_<i>_read_values
            << ',' << "\"\""; // operand_<i>_written_values
      }
    }

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

  dump_csv_instruction_values(csv_instruction_values, instruction_infos);

  // -------
  // Cleanup
  // -------

  // Flush and close the output file.
  log_file.flush();
  log_file.close();

  // Flush and close the CSV file.
  csv_instruction_values.flush();
  csv_instruction_values.close();
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
  std::cerr << "This Pin tool collects the values of each operand of an "
               "instruction at run time.\n\n";

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

  // Ensure that for each range, the user specified the image name, begin
  // offset, and end offset.
  const unsigned int image_count = KnobRangeImage.NumberOfValues();
  const unsigned int begin_count = KnobRangeBeginOffset.NumberOfValues();
  const unsigned int end_count = KnobRangeEndOffset.NumberOfValues();

  if (!((image_count == begin_count) && (begin_count == end_count))) {
    std::cerr << "You must provide the image name, begin offset, and end "
                 "offset for each instruction range!\n";
    return EXIT_FAILURE;
  }

  // Store the instruction ranges in instruction_ranges.
  for (unsigned int i = 0; i < image_count; ++i) {
    std::string image_name = KnobRangeImage.Value(i);
    ADDRINT begin_offset = KnobRangeBeginOffset.Value(i);
    ADDRINT end_offset = KnobRangeEndOffset.Value(i);

    InstructionRange range(image_name, begin_offset, end_offset);
    instruction_ranges.insert(range);
  }

  // Check if the user specified an instruction ranges configuration file.
  std::string ranges_file = KnobRangesFile.Value();
  if (!ranges_file.empty()) {
    std::string image;
    ADDRINT begin_offset;
    ADDRINT end_offset;

    std::ifstream ifs(ranges_file.c_str());

    while (ifs >> image >> begin_offset >> end_offset) {
      InstructionRange range(image, begin_offset, end_offset);
      instruction_ranges.insert(range);
    }
  }

  // Open output file.
  log_file.open(KnobOutputFile.Value().c_str());

  // If the prefix for CSV files is not specified, just use the output filename.
  std::string csv_prefix = KnobCsvPrefix.Value();

  if (csv_prefix.empty()) {
    csv_prefix = KnobOutputFile.Value();
  }

  // Open the CSV files.
  csv_instruction_values.open((csv_prefix + ".instruction-values.csv").c_str());

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
  PIN_MutexInit(&instruction_infos_lock);
  PIN_MutexInit(&memory_operands_map_lock);

  // Start the program (never returns).
  PIN_StartProgram();

  return EXIT_SUCCESS;
}
