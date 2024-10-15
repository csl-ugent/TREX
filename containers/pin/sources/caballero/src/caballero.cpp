#include "pin.H"
#include "sde-init.H"
#include <fstream>
#include <iostream>
#include <list>
#include <set>
#include <sstream>
#include <unordered_map>

#define MAX_WINDOW_SIZE 100
#define MIN_INTERVAL_SIZE 15

/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

// Option (-o) to set the output filename.
KNOB<std::string>
    KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "output", "caballero.log",
                   "Specify the filename of the human-readable log file");

// Option (-csv_prefix) to set the prefix used for the CSV output file.
KNOB<std::string>
    KnobCsvPrefix(KNOB_MODE_WRITEONCE, "pintool", "csv_prefix", "",
                  "Set the prefix used for the CSV output file. The output "
                  "file will be of the form <prefix>.caballero.csv.");

KNOB<double> KnobRatio(KNOB_MODE_WRITEONCE, "pintool", "r", "0.4",
                       "specify golden ratio");

/* ======= */
/* Structs */
/* ======= */

// Struct used to keep information for one basic block.
struct BasicBlock {
  BasicBlock() : BasicBlock("???", -1, -1, -1, 0, 0) {}

  BasicBlock(const std::string &image_name, ADDRINT address_begin,
             ADDRINT address_end, ADDRINT address, int size,
             int caballero_count)
      : image_name(image_name), image_offset_begin(address_begin),
        image_offset_end(address_end), address(address), size(size),
        caballero_count(caballero_count) {}

  std::string image_name;     // The image name of this basic block.
  ADDRINT image_offset_begin; // The address of the first instruction of this
                              // basic block.
  ADDRINT image_offset_end;   // The end address of this basic block.
  ADDRINT address;     // The absolute beginning address of this basic block.
  int size;            // The number of instructions in this basic block.
  int caballero_count; // The number of caballero instructions (arithmetic,
                       // logical) in this basic block.
};

// Struct used to keep information per thread.
struct thread_data_t {
  std::list<BasicBlock>
      *bbl_log;        // The list of the last basic blocks that are
                       // executed. We only keep enough basic blocks to
                       // fill the window of MAX_WINDOW_SIZE instructions.
  int total_count = 0; // The total number of instructions of all the basic
                       // blocks in bbl_log.
  int total_caballero_count = 0; // The total number of caballero instructiosn
                                 // (arithmetic, logical) in this basic block.
};

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

float golden_ratio = 0.4;
float min_total_caballero_count = golden_ratio * MIN_INTERVAL_SIZE;

std::ofstream csv_basic_blocks;
std::ofstream log_file;

static PIN_MUTEX csv_basic_blocks_lock;
static PIN_MUTEX log_file_lock;

std::set<ADDRINT> golden_blocks;
std::map<ADDRINT, BasicBlock> basic_blocks;

static PIN_MUTEX basic_blocks_lock;

// ======================================================================
// Helper functions
// ======================================================================

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

/* ======================================================================*/
/* Thread Data Management                                                */
/* ======================================================================*/

int numThreads = 0;
static TLS_KEY tls_key = INVALID_TLS_KEY;
PIN_LOCK golden_blocks_lock;

VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v) {
  numThreads++;
  thread_data_t *tdata = new thread_data_t;
  tdata->bbl_log = new std::list<BasicBlock>;
  if (PIN_SetThreadData(tls_key, tdata, threadid) == FALSE) {
    std::cerr << "PIN_SetThreadData failed" << std::endl;
    PIN_ExitProcess(1);
  }
}

VOID ThreadFini(THREADID threadIndex, const CONTEXT *ctxt, INT32 code,
                VOID *v) {
  thread_data_t *tdata =
      static_cast<thread_data_t *>(PIN_GetThreadData(tls_key, threadIndex));
  delete tdata->bbl_log;
  delete tdata;
}

/* ===================================================================== */
/* Execution time analysis routine */
/* */

int counter = 0;
int early_rets = 0;
int new_blocks = 0;
VOID updateStats(ADDRINT address, THREADID threadid) {
  PIN_MutexLock(&basic_blocks_lock);
  BasicBlock new_bbl = basic_blocks[address];
  PIN_MutexUnlock(&basic_blocks_lock);

  thread_data_t *td =
      static_cast<thread_data_t *>(PIN_GetThreadData(tls_key, threadid));

  td->total_count += new_bbl.size;
  td->total_caballero_count += new_bbl.caballero_count;
  while (td->total_count >= MAX_WINDOW_SIZE && td->bbl_log->size() > 0) {
    BasicBlock dropped = td->bbl_log->back();
    td->total_count -= dropped.size;
    td->total_caballero_count -= dropped.caballero_count;
    td->bbl_log->pop_back();
  }
  counter++;

  td->bbl_log->push_front(new_bbl);

  // This set of basic blocks can't possibly contain a golden interval, so
  // skip it.
  if (td->total_caballero_count < min_total_caballero_count) {
    return;
  }

  PIN_GetLock(&golden_blocks_lock, threadid + 1);
  if (golden_blocks.find(address) != golden_blocks.end()) {
    early_rets++;
    PIN_ReleaseLock(&golden_blocks_lock);
    return;
  }

  PIN_ReleaseLock(&golden_blocks_lock);
  new_blocks++;

  // Check if any of the ratios is golden
  int interval_size = 0;
  int interval_caballero_count = 0;
  float ratio = 0;
  std::string interval_bbls = "";
  // std::list<bbl>::iterator prev_interval_end = bbl_log->begin();

  for (std::list<BasicBlock>::iterator i = td->bbl_log->begin();
       i != td->bbl_log->end(); i++) {
    // std::cerr << (*i).address << std::endl;
    interval_size += (*i).size;
    interval_caballero_count += (*i).caballero_count;
    // This basic block is already part of a golden interval, so skip it.

    PIN_GetLock(&golden_blocks_lock, threadid + 1);

    if (golden_blocks.find(i->address) != golden_blocks.end()) {
      PIN_ReleaseLock(&golden_blocks_lock);
      continue;
    }

    PIN_ReleaseLock(&golden_blocks_lock);

    if (interval_size >= MIN_INTERVAL_SIZE) {
      ratio = (double)interval_caballero_count / (double)interval_size;
      // std::cerr << ratio << std::endl;
      if (ratio >= golden_ratio) {
        PIN_GetLock(&golden_blocks_lock, threadid + 1);
        if (golden_blocks.find(i->address) != golden_blocks.end()) {
          PIN_ReleaseLock(&golden_blocks_lock);
          continue;
        }
        PIN_ReleaseLock(&golden_blocks_lock);

        PIN_GetLock(&golden_blocks_lock, threadid + 1);
        golden_blocks.insert(i->address);
        PIN_ReleaseLock(&golden_blocks_lock);

        PIN_MutexLock(&csv_basic_blocks_lock);
        csv_basic_blocks << get_filename(i->image_name) << ',' // image_name
                         << i->image_offset_begin << ',' // image_offset_begin
                         << i->image_offset_end << '\n'; // image_offset_end
        PIN_MutexUnlock(&csv_basic_blocks_lock);
      }
    }
  }
}

/* ===================================================================== */
/* Decoding time analysis routine */
/* */

VOID Trace(TRACE trace, VOID *v) {
  for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
    int size = BBL_NumIns(bbl);
    int caballero_count = 0;
    ADDRINT bbl_addr = INS_Address(BBL_InsHead(bbl));

    for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) {
      INT32 category = INS_Category(ins);

      if (category == XED_CATEGORY_LOGICAL || category == XED_CATEGORY_SHIFT) {
        if (INS_Opcode(ins) != XED_ICLASS_SQRTSD) {
          caballero_count += 1;
        }
      } else if (category == XED_CATEGORY_MMX) {
        // TODO: verify xed category
        size = size - 1;
      }
    }

    IMG img = IMG_FindByAddress(bbl_addr);

    if (IMG_Valid(img)) {
      std::string image_name = IMG_Name(img);
      ADDRINT image_base = IMG_LowAddress(img);

      ADDRINT address_begin = INS_Address(BBL_InsHead(bbl));
      INS bbl_tail = BBL_InsTail(bbl);
      ADDRINT address_end = INS_Address(bbl_tail) + INS_Size(bbl_tail);

      PIN_MutexLock(&basic_blocks_lock);
      basic_blocks.insert(std::pair<ADDRINT, BasicBlock>(
          bbl_addr, BasicBlock(image_name, address_begin - image_base,
                               address_end - image_base, bbl_addr, size,
                               caballero_count)));
      PIN_MutexUnlock(&basic_blocks_lock);

      BBL_InsertCall(bbl, IPOINT_ANYWHERE, AFUNPTR(updateStats), IARG_ADDRINT,
                     bbl_addr,

                     IARG_THREAD_ID, IARG_END);
    }
  }
}

/* ===================================================================== */
/* Process Finishing                                                     */
/* ===================================================================== */

VOID Fini(INT32 code, VOID *v) {
  log_file.flush();
  log_file.close();

  csv_basic_blocks.flush();
  csv_basic_blocks.close();
}

bool earlyFini(unsigned int a, int b, LEVEL_VM::CONTEXT *, bool c,
               const LEVEL_BASE::EXCEPTION_INFO *d, void *e) {
  std::cerr << "signal caught" << std::endl;
  Fini(0, 0);
  PIN_ExitProcess(0);
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

INT32 Usage() {
  std::cerr << KNOB_BASE::StringKnobSummary();

  std::cerr << std::endl;

  return -1;
}

int main(int argc, char *argv[]) {
  // Initialise the PIN symbol manager.
  PIN_InitSymbols();

  // Initialise Pin and SDE.
  sde_pin_init(argc, argv);
  sde_init();

  // Obtain  a key for TLS storage.
  tls_key = PIN_CreateThreadDataKey(NULL);
  if (tls_key == INVALID_TLS_KEY) {
    std::cerr
        << "number of already allocated keys reached the MAX_CLIENT_TLS_KEYS "
           "limit"
        << std::endl;
    PIN_ExitProcess(1);
  }

  // Open output file.
  log_file.open(KnobOutputFile.Value().c_str());

  // If the prefix for CSV files is not specified, just use the output
  // filename.
  std::string csv_prefix = KnobCsvPrefix.Value();

  if (csv_prefix.empty()) {
    csv_prefix = KnobOutputFile.Value();
  }

  // Open the CSV files.
  csv_basic_blocks.open((csv_prefix + ".caballero.csv").c_str());

  csv_basic_blocks << "image_name,image_offset_begin,image_offset_end\n";

  // Intercept thread creation
  PIN_AddThreadStartFunction(ThreadStart, NULL);
  PIN_AddThreadFiniFunction(ThreadFini, NULL);

  // Add analysis calls
  golden_ratio = KnobRatio.Value();
  min_total_caballero_count = golden_ratio * MIN_INTERVAL_SIZE;

  TRACE_AddInstrumentFunction(Trace, 0);
  PIN_AddFiniFunction(Fini, 0);
  PIN_UnblockSignal(15, true);
  PIN_InterceptSignal(15, earlyFini, 0);

  // Initialise mutexes.
  PIN_MutexInit(&csv_basic_blocks_lock);
  PIN_MutexInit(&log_file_lock);
  PIN_MutexInit(&basic_blocks_lock);

  // Never returns
  PIN_StartProgram();

  return 0;
}
