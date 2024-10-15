#include <fstream>
#include <iostream>
#include <string>

#include "pin.H"
#include "sde-init.H"

// File stream used to write the human-readable log output to.
static std::ofstream log_file;

// File stream to write the CSV output to.
static std::ofstream csv_basic_blocks;

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
                   "basicblockprofiler.log",
                   "Specify the filename of the human-readable log file");

// Option (-csv_prefix) to set the prefix used for the CSV output file.
KNOB<std::string>
    KnobCsvPrefix(KNOB_MODE_WRITEONCE, "pintool", "csv_prefix", "",
                  "Set the prefix used for the CSV output file. The output "
                  "file will be of the form <prefix>.basic-blocks.csv.");

// Option (-start_from_main) to only start analysis from the point that main()
// is called.
KNOB<bool> KnobStartFromMain(
    KNOB_MODE_WRITEONCE, "pintool", "start_from_main", "1",
    "When true, starts analysis from the point that main() is called");

// Option (-end_after_main) to end analysis after main() is finished.
KNOB<bool>
    KnobEndAfterMain(KNOB_MODE_WRITEONCE, "pintool", "end_after_main", "1",
                     "When true, ends analysis after main() is finished");

// The address of a basic block.
struct BasicBlockAddress {
  BasicBlockAddress(ADDRINT address_begin, ADDRINT address_end)
      : image_name("???"), section_name("???"), routine_name("???"),
        image_offset_begin(-1), image_offset_end(-1), routine_offset_begin(-1),
        routine_offset_end(-1) {
    // Routine info.
    RTN routine_begin = RTN_FindByAddress(address_begin);
    if (!RTN_Valid(routine_begin)) {
      return;
    }

    this->routine_name = RTN_Name(routine_begin);
    auto routine_base = RTN_Address(routine_begin);
    this->routine_offset_begin = address_begin - routine_base;
    this->routine_offset_end = address_end - routine_base;

    // Section info.
    SEC section = RTN_Sec(routine_begin);
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
    auto image_base = IMG_LowAddress(image);
    this->image_offset_begin = address_begin - image_base;
    this->image_offset_end = address_end - image_base;
  }

  // The name of the image this basic block belongs to.
  std::string image_name;

  // The name of the section this basic block belongs to.
  std::string section_name;

  // The name of the routine this basic block belongs to.
  std::string routine_name;

  // The offset of the basic block's begin relative to the base of the image.
  ADDRINT image_offset_begin;

  // The offset of the basic block's end relative to the base of the image.
  ADDRINT image_offset_end;

  // The offset of the basic block's begin relative to the base of the routine.
  ADDRINT routine_offset_begin;

  // The offset of the basic block's end relative to the base of the routine.
  ADDRINT routine_offset_end;
};

// Contains the information for each basic block.
struct BasicBlockInfo {
  BasicBlockInfo(ADDRINT address_begin, ADDRINT address_end)
      : address(address_begin, address_end), num_executions(0) {}

  BasicBlockAddress address; // The address of the basic block.
  unsigned int
      num_executions; // The number of times this basic block was executed.
};

// Maps the start and end address of a basic block to its info.
static std::map<std::pair<ADDRINT, ADDRINT>, BasicBlockInfo> basic_block_infos;

// Mutex for accessing basic_block_infos.
static PIN_MUTEX basic_block_infos_lock;

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

// Run before each basic block.
VOID BasicBlockBefore(ADDRINT address_begin, ADDRINT address_end) {
  if (!main_reached || end_reached)
    return;

  // Find the basic block info.
  PIN_MutexLock(&basic_block_infos_lock);

  auto it = basic_block_infos.find(std::make_pair(address_begin, address_end));
  if (it != basic_block_infos.end()) {
    // Increment counter of number of executions.
    ++it->second.num_executions;
  } else {
    assert(false && "Basic block not added to basic_block_infos!");
  }

  PIN_MutexUnlock(&basic_block_infos_lock);
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

// Instrumentation routine run for every trace.
VOID OnTrace(TRACE trace, VOID *v) {
  // Iterate over all basic blocks in the trace.
  for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
    // Get the begin and end address of this basic block.
    // The end address is the address of the one-past-the-last instruction, NOT
    // the address of the last instruction!
    ADDRINT address_begin = INS_Address(BBL_InsHead(bbl));
    INS bbl_tail = BBL_InsTail(bbl);
    ADDRINT address_end = INS_Address(bbl_tail) + INS_Size(bbl_tail);

    // Add an entry for this basic block to the basic_block_infos map.
    PIN_MutexLock(&basic_block_infos_lock);
    basic_block_infos.insert({std::make_pair(address_begin, address_end),
                              BasicBlockInfo(address_begin, address_end)});
    PIN_MutexUnlock(&basic_block_infos_lock);

    // Call BasicBlockBefore() at the start of every basic block.
    BBL_InsertCall(bbl, IPOINT_BEFORE,
                   reinterpret_cast<AFUNPTR>(BasicBlockBefore), IARG_ADDRINT,
                   address_begin, IARG_ADDRINT, address_end, IARG_END);
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

// Dump the information for basic blocks in CSV format.
void dump_csv_basic_blocks(
    std::ofstream &ofs,
    const std::map<std::pair<ADDRINT, ADDRINT>, BasicBlockInfo> &map) {

  // Print header.
  ofs << "ip_begin,ip_end,image_name,full_image_name,section_name,image_offset_"
         "begin,image_offset_end,routine_name,routine_offset_begin,routine_"
         "offset_end,num_executions\n";

  // Print data.
  for (const auto &p : map) {
    ofs << p.first.first                                    // ip_begin
        << ',' << p.first.second                            // ip_end
        << ',' << get_filename(p.second.address.image_name) // image_name
        << ',' << p.second.address.image_name               // full_image_name
        << ',' << p.second.address.section_name             // section_name
        << ',';

    // Print "-1" for unknown offsets.
    if (p.second.address.image_offset_begin != static_cast<ADDRINT>(-1))
      ofs << p.second.address.image_offset_begin << ','; // image_offset_begin
    else
      ofs << -1 << ',';

    // Print "-1" for unknown offsets.
    if (p.second.address.image_offset_end != static_cast<ADDRINT>(-1))
      ofs << p.second.address.image_offset_end << ','; // image_offset_end
    else
      ofs << -1 << ',';

    ofs << p.second.address.routine_name // routine_name
        << ',';

    // Print "-1" for unknown offsets.
    if (p.second.address.routine_offset_begin != static_cast<ADDRINT>(-1))
      ofs << p.second.address.routine_offset_begin
          << ','; // routine_offset_begin
    else
      ofs << -1 << ',';

    // Print "-1" for unknown offsets.
    if (p.second.address.routine_offset_end != static_cast<ADDRINT>(-1))
      ofs << p.second.address.routine_offset_end << ','; // routine_offset_end
    else
      ofs << -1 << ',';

    ofs << p.second.num_executions; // num_executions

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

  dump_csv_basic_blocks(csv_basic_blocks, basic_block_infos);

  // -------
  // Cleanup
  // -------

  // Flush and close the output file.
  log_file.flush();
  log_file.close();

  // Flush and close the CSV file.
  csv_basic_blocks.flush();
  csv_basic_blocks.close();
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
  std::cerr << "This Pin tool profiles basic blocks: for each basic block, it "
               "returns how many times it was executed.\n\n";

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
  csv_basic_blocks.open((csv_prefix + ".basic-blocks.csv").c_str());

  // Register instruction callback.
  INS_AddInstrumentFunction(OnInstruction, nullptr);

  // Register image load callback.
  IMG_AddInstrumentFunction(OnImageLoad, nullptr);

  // Register trace callback.
  TRACE_AddInstrumentFunction(OnTrace, nullptr);

  // Register finish callback.
  PIN_AddFiniFunction(OnFinish, nullptr);

  // Prevent the application from blocking SIGTERM.
  PIN_UnblockSignal(15, true);

  // Intercept SIGTERM so we can kill the application and still obtain results.
  PIN_InterceptSignal(15, OnSigTerm, nullptr);

  // Initialise mutexes.
  PIN_MutexInit(&basic_block_infos_lock);

  // Start the program (never returns).
  PIN_StartProgram();

  return EXIT_SUCCESS;
}
