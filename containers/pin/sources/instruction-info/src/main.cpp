#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <iomanip>
#include <vector>

#include "pin.H"
#include "sde-init.H"

// File stream used to write the human-readable log output to.
static std::ofstream log_file;

// File stream to write the CSV output to.
static std::ofstream csv_instructions;

// Mutex for writing to csv_instructions.
static PIN_MUTEX csv_instructions_lock;

// Option (-o) to set the output filename.
KNOB<std::string>
    KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "output",
                   "instructioninfo.log",
                   "Specify the filename of the human-readable log file");

// Option (-csv_prefix) to set the prefix used for the CSV output file.
KNOB<std::string>
    KnobCsvPrefix(KNOB_MODE_WRITEONCE, "pintool", "csv_prefix", "",
                  "Set the prefix used for the CSV output file. The output "
                  "file will be of the form <prefix>.instruction-info.csv.");

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
// Instrumentation routines
// =============================================================================

// Instrumentation routine run for every trace.
VOID OnTrace(TRACE trace, VOID *v) {
  PIN_MutexLock(&csv_instructions_lock);

  // Iterate over all basic blocks in the trace.
  for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
    // Iterate over all instructions in the basic block.
    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
      // Get the address of the instruction.
      auto ins_addr = INS_Address(ins);

      // Initialise routine, section, and image to default invalid values.
      RTN rtn = RTN_Invalid();
      SEC sec = SEC_Invalid();
      IMG img = IMG_Invalid();

      // Find routine
      rtn = RTN_FindByAddress(ins_addr);

      // Find section
      if (RTN_Valid(rtn)) {
        sec = RTN_Sec(rtn);

        // Find image
        if (SEC_Valid(sec)) {
          img = SEC_Img(sec);
        }
      }

      USIZE ins_size = INS_Size(ins);

      std::vector<UINT8> ins_bytes(ins_size);
      PIN_SafeCopy(&ins_bytes[0], reinterpret_cast<void*>(ins_addr), ins_size);

      // Write the information of the instruction to the CSV output file.
      auto full_image_name = (IMG_Valid(img) ? IMG_Name(img) : "???");

      // image_name
      csv_instructions << get_filename(full_image_name) << ",";

      // full_image_name
      csv_instructions << full_image_name << ",";

      // section_name
      csv_instructions << (SEC_Valid(sec) ? SEC_Name(sec) : "???") << ",";

      // image_offset
      const auto image_offset =
          (int)(IMG_Valid(img) ? ins_addr - IMG_LowAddress(img) : -1);
      csv_instructions << image_offset << ",";

      // routine_name
      csv_instructions << (RTN_Valid(rtn) ? RTN_Name(rtn) : "???") << ",";

      // routine_offset
      csv_instructions << (int)(RTN_Valid(rtn) ? ins_addr - RTN_Address(rtn)
                                               : -1)
                       << ",";

      const std::string disassembly = INS_Disassemble(ins);

      // Skip prefixes by finding the last occurrence of a known prefix string.
      const char *prefixes[] = {"lock ", "rep ", "repne ", "data16 "};
      std::string::size_type sep_pos = 0;

      for (const auto &prefix : prefixes) {
        auto pos = disassembly.find(prefix, sep_pos);
        if (pos != std::string::npos)
          sep_pos = pos + std::strlen(prefix);
      }

      // Find the first space after the prefixes.
      sep_pos = disassembly.find(' ', sep_pos);

      const auto opcode = disassembly.substr(0, sep_pos);
      const auto operands = disassembly.substr(sep_pos + 1);

      // opcode
      csv_instructions << "\"" << opcode << "\",";

      // operands
      csv_instructions << "\"" << operands << "\",";

      // category
      csv_instructions << "\"" << CATEGORY_StringShort(INS_Category(ins))
                       << "\",";

      // DEBUG_filename
      csv_instructions << "\"$DEBUG(" << full_image_name << "," << image_offset
                       << ","
                       << "filename"
                       << ")\""
                       << ",";

      // DEBUG_line
      csv_instructions << "\"$DEBUG(" << full_image_name << "," << image_offset
                       << ","
                       << "line"
                       << ")\""
                       << ",";

      // DEBUG_column
      csv_instructions << "\"$DEBUG(" << full_image_name << "," << image_offset
                       << ","
                       << "column"
                       << ")\""
                       << ",";

      csv_instructions << std::hex;

      for(size_t i = 0; i<ins_size; i++){
        csv_instructions << std::setfill('0') << std::setw(2) << static_cast<unsigned int>(ins_bytes[i]);
      }

      csv_instructions << std::dec;

      csv_instructions << "\n";
    }
  }

  PIN_MutexUnlock(&csv_instructions_lock);
}

// =============================================================================
// Other routines
// =============================================================================

// Finalizer routine.
VOID OnFinish(INT32 code, VOID *v) {
  // -------
  // Cleanup
  // -------

  // Flush and close the output file.
  log_file.flush();
  log_file.close();

  // Flush and close the CSV file.
  csv_instructions.flush();
  csv_instructions.close();
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
  std::cerr << "This Pin tool collects some information for each instruction "
               "that is executed: which image and routine it is in, its "
               "opcode, operands, etc.\n\n";

  std::cerr << KNOB_BASE::StringKnobSummary() << '\n';

  return EXIT_FAILURE;
}

int main(int argc, char *argv[]) {
  // Initialise the PIN symbol manager.
  PIN_InitSymbols();

  // Initialise Pin and SDE.
  sde_pin_init(argc, argv);
  sde_init();

  // Open output file.
  log_file.open(KnobOutputFile.Value().c_str());

  // If the prefix for CSV files is not specified, just use the output filename.
  std::string csv_prefix = KnobCsvPrefix.Value();

  if (csv_prefix.empty()) {
    csv_prefix = KnobOutputFile.Value();
  }

  // Open the CSV files.
  csv_instructions.open((csv_prefix + ".instruction-info.csv").c_str());

  // Print the CSV header.
  csv_instructions << "image_name,full_image_name,section_name,image_offset,"
                      "routine_name,routine_offset,opcode,operands,category,"
                      "DEBUG_filename,DEBUG_line,DEBUG_column,bytes\n";

  // Register trace callback.
  TRACE_AddInstrumentFunction(OnTrace, nullptr);

  // Register finish callback.
  PIN_AddFiniFunction(OnFinish, nullptr);

  // Prevent the application from blocking SIGTERM.
  PIN_UnblockSignal(15, true);

  // Intercept SIGTERM so we can kill the application and still obtain results.
  PIN_InterceptSignal(15, OnSigTerm, nullptr);

  // Initialise mutex.
  PIN_MutexInit(&csv_instructions_lock);

  // Start the program (never returns).
  PIN_StartProgram();

  return EXIT_SUCCESS;
}
