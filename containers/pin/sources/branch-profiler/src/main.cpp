#include <fstream>
#include <iostream>
#include <string>

#include "pin.H"
#include "sde-init.H"

// File stream used to write the human-readable log output to.
static std::ofstream log_file;

// File stream to write the CSV output to.
static std::ofstream csv_branches;

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
                   "branchprofiler.log",
                   "Specify the filename of the human-readable log file");

// Option (-csv_prefix) to set the prefix used for the CSV output file.
KNOB<std::string>
    KnobCsvPrefix(KNOB_MODE_WRITEONCE, "pintool", "csv_prefix", "",
                  "Set the prefix used for the CSV output file. The output "
                  "file will be of the form <prefix>.branches.csv.");

// Option (-start_from_main) to only start analysis from the point that main()
// is called.
KNOB<bool> KnobStartFromMain(
    KNOB_MODE_WRITEONCE, "pintool", "start_from_main", "1",
    "When true, starts analysis from the point that main() is called");

// Option (-end_after_main) to end analysis after main() is finished.
KNOB<bool>
    KnobEndAfterMain(KNOB_MODE_WRITEONCE, "pintool", "end_after_main", "1",
                     "When true, ends analysis after main() is finished");

// Address of a static instruction.
struct StaticInstructionAddress {
  StaticInstructionAddress() : image_name("???"), image_offset(-1) {}

  StaticInstructionAddress(const std::string &image_name, ADDRINT image_offset)
      : image_name(image_name), image_offset(image_offset) {}

  std::string image_name; // Name of the image.
  ADDRINT image_offset;   // Offset from the start of the image.
};

// Maps a numerical instruction address (value of RIP) to a more readable
// version of the address (image name + offset).
static std::map<ADDRINT, StaticInstructionAddress> staticInstructionAddresses;

// Information kept for each branch instruction.
struct BranchInfo {
  BranchInfo() : taken(0), not_taken(0) {}
  unsigned int taken;     // Number of times this branch was taken.
  unsigned int not_taken; // Number of times this branch was not taken.
};

// Maps each instruction address (of branches only) to the number of times it
// was taken or not taken.
static std::map<ADDRINT, BranchInfo> branch_infos;

// Mutex for access to branch_infos.
static PIN_MUTEX branch_infos_lock;

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

// Run before each branch.
VOID BranchBefore(ADDRINT instruction_address, BOOL is_taken) {
  if (!main_reached || end_reached)
    return;

  // Find the branch info.
  PIN_MutexLock(&branch_infos_lock);

  auto it = branch_infos.find(instruction_address);
  if (it != branch_infos.end()) {
    // Increment taken/not taken counters.
    if (is_taken)
      ++it->second.taken;
    else
      ++it->second.not_taken;
  } else {
    assert(false && "Branch not added to branch_infos!");
  }

  PIN_MutexUnlock(&branch_infos_lock);
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
    // Iterate over all instructions in the trace.
    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
      // We are only interested in branches, so skip any other type of
      // instruction.
      if (INS_IsBranch(ins)) {
        // Get the address of this instruction.
        ADDRINT ins_address = INS_Address(ins);

        // Get information of this instruction.
        PIN_LockClient();
        IMG image = IMG_FindByAddress(ins_address);

        std::string image_name = IMG_Valid(image) ? IMG_Name(image) : "???";
        ADDRINT image_offset = IMG_Valid(image)
                                   ? (ins_address - IMG_LowAddress(image))
                                   : static_cast<ADDRINT>(-1);

        PIN_UnlockClient();

        // Add the static address of this instruction to the static instructions
        // map.
        staticInstructionAddresses.insert(std::make_pair(
            ins_address, StaticInstructionAddress(image_name, image_offset)));

        // Add entry to branch_infos map.
        PIN_MutexLock(&branch_infos_lock);
        branch_infos.insert(std::make_pair(ins_address, BranchInfo()));
        PIN_MutexUnlock(&branch_infos_lock);

        // Call BranchBefore() before every branch instruction.
        INS_InsertCall(ins, IPOINT_BEFORE,
                       reinterpret_cast<AFUNPTR>(BranchBefore), IARG_INST_PTR,
                       IARG_BRANCH_TAKEN, IARG_END);
      }
    }
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

// Dump the information for branches in CSV format.
void dump_csv_branches(std::ofstream &ofs,
                       const std::map<ADDRINT, BranchInfo> &map) {

  // Print header.
  ofs << "image_name,image_offset,num_taken,num_not_taken\n";
  // Print data.
  for (const auto &p : map) {
    // Grab the address of the branch.
    auto addr = p.first;

    // Get the address entry in the staticInstructionAddresses map.
    auto staticAddrEntry = staticInstructionAddresses[addr];

    ofs << get_filename(staticAddrEntry.image_name); // image_name

    // Print "-1" for unknown offsets.
    if (staticAddrEntry.image_offset != static_cast<ADDRINT>(-1))
      ofs << ',' << staticAddrEntry.image_offset; // image_offset
    else
      ofs << ',' << -1; // image_offset

    ofs << ',' << p.second.taken      // num_taken
        << ',' << p.second.not_taken; // num_not_taken

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

  dump_csv_branches(csv_branches, branch_infos);

  // -------
  // Cleanup
  // -------

  // Flush and close the output file.
  log_file.flush();
  log_file.close();

  // Flush and close the CSV file.
  csv_branches.flush();
  csv_branches.close();
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
  std::cerr << "This Pin tool profiles branches: for each branch, it returns "
               "how many times the branch was taken or not taken.\n\n";

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
  csv_branches.open((csv_prefix + ".branches.csv").c_str());

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
  PIN_MutexInit(&branch_infos_lock);

  // Start the program (never returns).
  PIN_StartProgram();

  return EXIT_SUCCESS;
}
