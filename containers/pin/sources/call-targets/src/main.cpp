#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <utility>

#include "pin.H"
#include "sde-init.H"

// File stream used to write the human-readable log output to.
static std::ofstream log_file;

// File stream to write the CSV output to.
static std::ofstream csv_call_targets;

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
    KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "output", "calltargets.log",
                   "Specify the filename of the human-readable log file");

// Option (-csv_prefix) to set the prefix used for the CSV output file.
KNOB<std::string>
    KnobCsvPrefix(KNOB_MODE_WRITEONCE, "pintool", "csv_prefix", "",
                  "Set the prefix used for the CSV output file. The output "
                  "file will be of the form <prefix>.call-targets.csv.");

// Option (-start_from_main) to only start analysis from the point that main()
// is called.
KNOB<bool> KnobStartFromMain(
    KNOB_MODE_WRITEONCE, "pintool", "start_from_main", "1",
    "When true, starts analysis from the point that main() is called");

// Option (-end_after_main) to end analysis after main() is finished.
KNOB<bool>
    KnobEndAfterMain(KNOB_MODE_WRITEONCE, "pintool", "end_after_main", "1",
                     "When true, ends analysis after main() is finished");

// Contains the information for each instruction.
struct InstructionInfo {
  InstructionInfo(const std::string &image_name, ADDRINT image_offset,
                  const std::string &function_name)
      : image_name(image_name), image_offset(image_offset),
        function_name(function_name) {}

  // The name of the image this instruction belongs to.
  std::string image_name;

  // The offset of the instruction relative to the base of the image.
  ADDRINT image_offset;

  // The function name of this instruction.
  std::string function_name;
};

// Maps the address of a call instruction to the set of callees.
static std::map<ADDRINT, std::set<ADDRINT>> call_instruction_infos;

// Mutex for accessing call_instruction_infos.
static PIN_MUTEX call_instruction_infos_lock;

// Maps the address of an instruction to its instruction info.
static std::map<ADDRINT, InstructionInfo> static_instruction_infos;

// Mutex for accessing static_instruction_infos.
static PIN_MUTEX static_instruction_infos_lock;

// =============================================================================
// Helper functions
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
VOID InstructionCallBefore(ADDRINT instruction_address, ADDRINT target_address,
                           ADDRINT next_instruction_pointer) {
  // Check if this is the first call to main().
  if ((end_address == INVALID_ADDRESS) && (target_address == main_address)) {
    log_file << "\nMain called, next ip = " << std::hex << std::showbase
             << next_instruction_pointer << std::dec << "\n";
    end_address = next_instruction_pointer;
  }

  if (!main_reached || end_reached)
    return;

  // Find the call instruction info.
  PIN_MutexLock(&call_instruction_infos_lock);

  auto it = call_instruction_infos.find(instruction_address);
  if (it != call_instruction_infos.end()) {
    it->second.insert(target_address);
  } else {
    assert(false &&
           "Call instruction info not added to call_instruction_infos!");
  }

  PIN_MutexUnlock(&call_instruction_infos_lock);
}

// Run at the start of __libc_start_main().
VOID LibcStartMainBefore(ADDRINT main_addr) {
  log_file << "\n__libc_start_main(main = " << std::hex << std::showbase
           << main_addr << std::dec << ", ...)\n";

  main_address = main_addr;
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

  ADDRINT ins_addr = INS_Address(instruction);
  IMG img = IMG_FindByAddress(ins_addr);
  RTN rtn = RTN_FindByAddress(ins_addr);

  std::string image_name = (IMG_Valid(img) ? IMG_Name(img) : "???");
  ADDRINT image_offset = (IMG_Valid(img) ? ins_addr - IMG_LowAddress(img) : -1);
  std::string function_name = (RTN_Valid(rtn) ? RTN_Name(rtn) : "???");

  // Add an entry for this instruction to the static_instruction_infos map.
  PIN_MutexLock(&static_instruction_infos_lock);
  static_instruction_infos.insert(std::make_pair(
      ins_addr, InstructionInfo(image_name, image_offset, function_name)));
  PIN_MutexUnlock(&static_instruction_infos_lock);

  // Check for call instructions.
  if (INS_IsCall(instruction)) {
    // Add an entry for this instruction to the call_instruction_infos map.
    PIN_MutexLock(&call_instruction_infos_lock);
    call_instruction_infos.insert(
        std::make_pair(ins_addr, std::set<ADDRINT>()));
    PIN_MutexUnlock(&call_instruction_infos_lock);

    // Call InstructionCallBefore() before every call instruction.
    // Pass the instruction address (i.e. the address of the call), the target
    // address (i.e. the address of the function being called) and the address
    // of the next instruction (i.e. the one following the call).
    INS_InsertCall(instruction, IPOINT_BEFORE,
                   reinterpret_cast<AFUNPTR>(InstructionCallBefore),
                   IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_ADDRINT,
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

// =============================================================================
// Machine-parsable output routines
// =============================================================================

// Dump the information for call instructions in CSV format.
void dump_csv_call_instructions(
    std::ofstream &ofs, const std::map<ADDRINT, std::set<ADDRINT>> &map,
    const std::map<ADDRINT, InstructionInfo> &instruction_info_map) {

  // Print header.
  ofs << "image_name,image_offset,DEBUG_filename,DEBUG_line,DEBUG_column,"
         "target_image_name,target_image_offset,target_function_name,DEBUG_"
         "target_filename,DEBUG_target_line,DEBUG_target_column\n";

  // Print data.
  for (const auto &p : map) {
    for (const auto &callee : p.second) {
      auto call_info_it = instruction_info_map.find(p.first);
      auto callee_info_it = instruction_info_map.find(callee);

      assert((call_info_it != instruction_info_map.end()) &&
             "Missing instruction info for call instruction!");
      assert((callee_info_it != instruction_info_map.end()) &&
             "Missing instruction info for callee!");

      const auto &call_info = call_info_it->second;
      const auto &callee_info = callee_info_it->second;

      ofs << '"' << get_filename(call_info.image_name) << '"'; // image_name

      // Print "-1" for unknown offsets.
      if (call_info.image_offset != static_cast<ADDRINT>(-1))
        ofs << ',' << call_info.image_offset; // image_offset
      else
        ofs << ',' << -1;

      ofs << ',' << "$DEBUG(" << call_info.image_name << ","
          << call_info.image_offset << ",filename)" // DEBUG_filename
          << ',' << "$DEBUG(" << call_info.image_name << ","
          << call_info.image_offset << ",line)" // DEBUG_line
          << ',' << "$DEBUG(" << call_info.image_name << ","
          << call_info.image_offset << ",column)"; // DEBUG_column

      ofs << ',' << '"' << get_filename(callee_info.image_name)
          << '"'; // target_image_name

      if (callee_info.image_offset != static_cast<ADDRINT>(-1))
        ofs << ',' << callee_info.image_offset; // target_image_offset
      else
        ofs << ',' << -1;

      ofs << ',' << callee_info.function_name // target_function_name
          << ',' << "$DEBUG(" << callee_info.image_name << ","
          << callee_info.image_offset << ",filename)" // DEBUG_target_filename
          << ',' << "$DEBUG(" << callee_info.image_name << ","
          << callee_info.image_offset << ",line)" // DEBUG_target_line
          << ',' << "$DEBUG(" << callee_info.image_name << ","
          << callee_info.image_offset << ",column)"; // DEBUG_target_column

      ofs << '\n';
    }
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

  dump_csv_call_instructions(csv_call_targets, call_instruction_infos,
                             static_instruction_infos);

  // -------
  // Cleanup
  // -------

  // Flush and close the output file.
  log_file.flush();
  log_file.close();

  // Flush and close the CSV file.
  csv_call_targets.flush();
  csv_call_targets.close();
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
  std::cerr << "This Pin tool collect the targets of (indirect) call "
               "instructions.\n\n";

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
  csv_call_targets.open((csv_prefix + ".call-targets.csv").c_str());

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
  PIN_MutexInit(&call_instruction_infos_lock);
  PIN_MutexInit(&static_instruction_infos_lock);

  // Start the program (never returns).
  PIN_StartProgram();

  return EXIT_SUCCESS;
}
