#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "create_map.h"

#include "pin.H"
#include "sde-init.H"

#include <sys/mman.h>
#include <sys/syscall.h>

// File stream used to write the human-readable log output to.
static std::ofstream log_file;

// File stream to write the CSV output to.
static std::ofstream csv_system_calls;

// Lock to control access to the csv_system_calls file.
static PIN_MUTEX csv_system_calls_lock;

// Mutex for sysCallIdMap.
static PIN_MUTEX sysCallIdMapLock;

// Option (-o) to set the output filename.
KNOB<std::string>
    KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "output", "systemcalls.log",
                   "Specify the filename of the human-readable log file");

// Option (-csv_prefix) to set the prefix used for the CSV output file.
KNOB<std::string>
    KnobCsvPrefix(KNOB_MODE_WRITEONCE, "pintool", "csv_prefix", "",
                  "Set the prefix used for the CSV output file. The output "
                  "file will be of the form <prefix>.system-calls.csv.");

static constexpr std::size_t MAX_SYSCALL_ARGUMENTS = 6;

// Contains all information for a syscall that can only be determined before the
// syscall is executed.
struct SyscallInfo {
  ADDRINT number; // The system call number.
  std::string
      arguments[MAX_SYSCALL_ARGUMENTS]; // The arguments of the system call.
  unsigned int threadId;
  unsigned int syscallId;
};

typedef SyscallInfo ThreadData;

// Key for accessing TLS storage in threads. Initialised once in main().
static TLS_KEY tls_key = INVALID_TLS_KEY;

// Keep track of how many times we executed any system call in each thread.
static std::map<THREADID, unsigned int> sysCallIdMap;

// =============================================================================
// Argument formatters
// =============================================================================

typedef std::string (*Formatter)(ADDRINT);

std::string FormatAsVoid(ADDRINT arg) { return "void"; }

std::string FormatAsHex(ADDRINT arg) {
  std::ostringstream oss;
  oss << "0x" << std::noshowbase << std::hex << arg;
  return oss.str();
}

std::string FormatAsSigned(ADDRINT arg) {
  std::ostringstream oss;
#ifdef PIN_TARGET_ARCH_X64
  oss << static_cast<INT64>(arg);
#else
  oss << static_cast<INT32>(arg);
#endif
  return oss.str();
}

std::string FormatAsUnsigned(ADDRINT arg) {
  std::ostringstream oss;
  oss << arg;
  return oss.str();
}

void writeFlag(std::ostringstream &oss, ADDRINT arg, bool &firstFlag,
               ADDRINT flag, const std::string &flagName) {
  if ((arg & flag) == flag) {
    oss << (firstFlag ? "" : "|") << flagName;
    firstFlag = false;
  }
}

std::string FormatAsProtection(ADDRINT arg) {
  std::ostringstream oss;

  if (arg == 0) {
    oss << "PROT_NONE";
  } else {
    bool firstFlag = true;

    writeFlag(oss, arg, firstFlag, PROT_EXEC, "PROT_EXEC");
    writeFlag(oss, arg, firstFlag, PROT_READ, "PROT_READ");
    writeFlag(oss, arg, firstFlag, PROT_WRITE, "PROT_WRITE");
    writeFlag(oss, arg, firstFlag, PROT_SEM, "PROT_SEM");
    writeFlag(oss, arg, firstFlag, PROT_GROWSUP, "PROT_GROWSUP");
    writeFlag(oss, arg, firstFlag, PROT_GROWSDOWN, "PROT_GROWSDOWN");
  }

  return oss.str();
}

std::string FormatAsFlags(ADDRINT arg) {
  std::ostringstream oss;

  bool firstFlag = true;

  writeFlag(oss, arg, firstFlag, MAP_SHARED, "MAP_SHARED");
  writeFlag(oss, arg, firstFlag, MAP_PRIVATE, "MAP_PRIVATE");
  writeFlag(oss, arg, firstFlag, MAP_32BIT, "MAP_32BIT");
  writeFlag(oss, arg, firstFlag, MAP_ANONYMOUS, "MAP_ANONYMOUS");
  writeFlag(oss, arg, firstFlag, MAP_FIXED, "MAP_FIXED");
  writeFlag(oss, arg, firstFlag, MAP_GROWSDOWN, "MAP_GROWSDOWN");
  writeFlag(oss, arg, firstFlag, MAP_HUGETLB, "MAP_HUGETLB");
  writeFlag(oss, arg, firstFlag, MAP_HUGE_2MB, "MAP_HUGE_2MB");
  writeFlag(oss, arg, firstFlag, MAP_HUGE_1GB, "MAP_HUGE_1GB");
  writeFlag(oss, arg, firstFlag, MAP_LOCKED, "MAP_LOCKED");
  writeFlag(oss, arg, firstFlag, MAP_NONBLOCK, "MAP_NONBLOCK");
  writeFlag(oss, arg, firstFlag, MAP_NORESERVE, "MAP_NORESERVE");
  writeFlag(oss, arg, firstFlag, MAP_POPULATE, "MAP_POPULATE");
  writeFlag(oss, arg, firstFlag, MAP_STACK, "MAP_STACK");
  // NOTE: MAP_UNINITIALIZED is 0x0, so is always printed...
  // writeFlag(oss, arg, firstFlag, MAP_UNINITIALIZED, "MAP_UNINITIALIZED");

  return oss.str();
}

// =============================================================================
// Other routines
// =============================================================================

struct SysCallInfo {
  SysCallInfo(const std::string &name = "",
              const Formatter &retValFormatter = FormatAsVoid)
      : name(name), retValFormatter(retValFormatter) {}

  SysCallInfo &addArg(const Formatter &f) {
    argFormatters.emplace_back(f);
    return *this;
  }

  std::string name;                     // Human-readable name
  std::vector<Formatter> argFormatters; // Argument formatters
  Formatter retValFormatter;            // Return value formatter
};

// Maps system calls to their name, and printers of their arguments and return
// value.
static const std::map<ADDRINT, SysCallInfo> SystemCalls =
    create_map<ADDRINT, SysCallInfo> // SYSCALL_MAP_BEGIN
    // clang-format off

    (SYS_mmap, SysCallInfo("mmap", FormatAsHex) // void *mmap(
            .addArg(FormatAsHex)         // void *addr,
            .addArg(FormatAsUnsigned)    // size_t length,
            .addArg(FormatAsProtection)  // int prot,
            .addArg(FormatAsFlags)       // int flags,
            .addArg(FormatAsSigned)      // int fd,
            .addArg(FormatAsUnsigned)    // off_t offset)
    )

#ifdef TARGET_IA32
    (SYS_mmap2, SysCallInfo("mmap2", FormatAsHex) // void *mmap2(
            .addArg(FormatAsHex)         // void *addr,
            .addArg(FormatAsUnsigned)    // size_t length,
            .addArg(FormatAsProtection)  // int prot,
            .addArg(FormatAsFlags)       // int flags,
            .addArg(FormatAsSigned)      // int fd,
            .addArg(FormatAsUnsigned)    // off_t pgoffset)
    )
#endif

    (SYS_mprotect, SysCallInfo("mprotect", FormatAsSigned) // int mprotect(
            .addArg(FormatAsHex)                 // void *addr,
            .addArg(FormatAsUnsigned)            // size_t len,
            .addArg(FormatAsProtection)          // int prot)
    )

    (SYS_munmap, SysCallInfo("munmap", FormatAsSigned) // int munmap(
            .addArg(FormatAsHex)               // void *addr,
            .addArg(FormatAsUnsigned)          // size_t length)
    )

    (SYS_brk, SysCallInfo("brk", FormatAsHex) // void *brk(
            .addArg(FormatAsHex)         // void *addr)
    )

    (SYS_read, SysCallInfo("read", FormatAsSigned) // ssize_t read(
                .addArg(FormatAsSigned) // int fildes,
                .addArg(FormatAsHex) // void *buf,
                .addArg(FormatAsUnsigned) // size_t nbyte)
    )

    // clang-format on
    ; // SYSCALL_MAP_END

// Callback that is executed before each system call.
VOID OnSyscallEntry(THREADID threadId, CONTEXT *ctx, SYSCALL_STANDARD std,
                    VOID *v) {
  PIN_MutexLock(&sysCallIdMapLock);
  auto sysCallId = sysCallIdMap[threadId]++;
  PIN_MutexUnlock(&sysCallIdMapLock);

  ADDRINT sysCallNo = PIN_GetSyscallNumber(ctx, std);
  ThreadData *thread_data =
      static_cast<ThreadData *>(PIN_GetThreadData(tls_key, threadId));
  thread_data->number = sysCallNo;
  thread_data->threadId = threadId;
  thread_data->syscallId = sysCallId;

  auto it = SystemCalls.find(sysCallNo);
  if (it != SystemCalls.end()) {
    const auto &argFormatters = it->second.argFormatters;
    unsigned int numArguments = argFormatters.size();

    for (unsigned int i = 0; i < numArguments; ++i) {
      thread_data->arguments[i] =
          argFormatters[i](PIN_GetSyscallArgument(ctx, std, i));
    }

    for (unsigned int i = numArguments; i < MAX_SYSCALL_ARGUMENTS; ++i) {
      thread_data->arguments[i] = "";
    }
  }
}

// Callback that is executed after each system call.
VOID OnSyscallExit(THREADID threadId, CONTEXT *ctx, SYSCALL_STANDARD std,
                   VOID *v) {
  ThreadData *thread_data =
      static_cast<ThreadData *>(PIN_GetThreadData(tls_key, threadId));

  ADDRINT sysCallNo = thread_data->number;
  ADDRINT retVal = PIN_GetSyscallReturn(ctx, std);

  auto it = SystemCalls.find(sysCallNo);
  if (it != SystemCalls.end()) {
    PIN_MutexLock(&csv_system_calls_lock);

    const auto &numArguments = it->second.argFormatters.size();
    const auto &retFormatter = it->second.retValFormatter;

    csv_system_calls << it->second.name << ',' // name
                     << numArguments << ',';   // num_arguments

    for (unsigned int i = 0; i < MAX_SYSCALL_ARGUMENTS; ++i) {
      csv_system_calls << thread_data->arguments[i] << ','; // argument_{N}
    }

    csv_system_calls << retFormatter(retVal)          // return_value
                     << ',' << thread_data->threadId  // thread_id
                     << ',' << thread_data->syscallId // syscall_id
                     << '\n';

    PIN_MutexUnlock(&csv_system_calls_lock);
  }
}

// Callback that is called when a thread starts.
VOID OnThreadStart(THREADID threadId, CONTEXT *ctx, INT32 flags, VOID *v) {
  ThreadData *thread_data = new ThreadData();
  if (!PIN_SetThreadData(tls_key, thread_data, threadId)) {
    std::cerr << "PIN_SetThreadData failed!\n" << std::endl;
    PIN_ExitProcess(1);
  }
}

// Callback that is called when a thread exits.
VOID OnThreadFini(THREADID threadId, const CONTEXT *ctx, INT32 code, VOID *v) {
  ThreadData *thread_data =
      static_cast<ThreadData *>(PIN_GetThreadData(tls_key, threadId));
  delete thread_data;
}

// Finalizer routine.
VOID OnFinish(INT32 code, VOID *v) {
  // -------
  // Cleanup
  // -------

  // Flush and close the output file.
  log_file.flush();
  log_file.close();

  // Flush and close the CSV file.
  csv_system_calls.flush();
  csv_system_calls.close();
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
      << "This Pin tool collects the system calls executed by the process.\n\n";

  std::cerr << KNOB_BASE::StringKnobSummary() << '\n';

  return EXIT_FAILURE;
}

int main(int argc, char *argv[]) {
  // Initialise the PIN symbol manager.
  PIN_InitSymbols();

  // Initialise Pin and SDE.
  sde_pin_init(argc, argv);
  sde_init();

  // Initialise thread-local storage.
  tls_key = PIN_CreateThreadDataKey(nullptr);
  if (tls_key == INVALID_TLS_KEY) {
    std::cerr << "Maximum amount of allocated TLS keys reached!" << std::endl;
    PIN_ExitProcess(1);
    return EXIT_FAILURE;
  }

  // Open output file.
  log_file.open(KnobOutputFile.Value().c_str());

  // If the prefix for CSV files is not specified, just use the output filename.
  std::string csv_prefix = KnobCsvPrefix.Value();

  if (csv_prefix.empty()) {
    csv_prefix = KnobOutputFile.Value();
  }

  // Open the CSV files.
  csv_system_calls.open((csv_prefix + ".system-calls.csv").c_str());

  // Write the CSV header.
  csv_system_calls << "name,num_arguments,";
  for (unsigned int i = 0; i < MAX_SYSCALL_ARGUMENTS; ++i) {
    csv_system_calls << "argument_" << i << ",";
  }
  csv_system_calls << "return_value,thread_id,syscall_id\n";

  // Register callbacks for system call entry and exit.
  PIN_AddSyscallEntryFunction(OnSyscallEntry, nullptr);
  PIN_AddSyscallExitFunction(OnSyscallExit, nullptr);

  // Register callbacks when a thread starts or ends.
  PIN_AddThreadStartFunction(OnThreadStart, nullptr);
  PIN_AddThreadFiniFunction(OnThreadFini, nullptr);

  // Register finish callback.
  PIN_AddFiniFunction(OnFinish, nullptr);

  // Prevent the application from blocking SIGTERM.
  PIN_UnblockSignal(15, true);

  // Intercept SIGTERM so we can kill the application and still obtain results.
  PIN_InterceptSignal(15, OnSigTerm, nullptr);

  // Initialise mutexes.
  PIN_MutexInit(&csv_system_calls_lock);
  PIN_MutexInit(&sysCallIdMapLock);

  // Start the program (never returns).
  PIN_StartProgram();

  return EXIT_SUCCESS;
}
