#include "pin.H"
#include "sde-init.H"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <utility>

#include <sys/mman.h>
#include <sys/syscall.h>

#include "create_map.h"
#include "pretty_print_operand.h"

using std::cerr;
using std::endl;
using std::ofstream;

#ifdef i386
#define MAX_DECODED 0x7fffffff;
#else
#define MAX_DECODED 0x7fffffffffffffff;
#endif

// Pin tool options
// Option (-start_from_main) to only start analysis from the point that main()
// is called.
KNOB<bool> KnobStartFromMain(
    KNOB_MODE_WRITEONCE, "pintool", "start_from_main", "1",
    "When true, starts analysis from the point that main() is called");

// Option (-end_after_main) to end analysis after main() is finished.
KNOB<bool>
    KnobEndAfterMain(KNOB_MODE_WRITEONCE, "pintool", "end_after_main", "1",
                     "When true, ends analysis after main() is finished");

// Option (-csv_prefix) to set the prefix used for the CSV output file.
KNOB<std::string>
    KnobCsvPrefix(KNOB_MODE_WRITEONCE, "pintool", "csv_prefix", "",
                  "Set the prefix used for the CSV output file. The output "
                  "file will be of the form <prefix>.memory_dependencies.csv "
                  "and <prefix>.register_dependencies.csv.");

// Option (-shortcuts) to calculate shortcut dependencies instead of normal
// dependencies.
KNOB<bool> KnobShortcuts(
    KNOB_MODE_WRITEONCE, "pintool", "shortcuts", "0",
    "When true, output shortcut dependencies instead of normal dependencies.");

KNOB<bool>
    KnobIgnoreNops(KNOB_MODE_WRITEONCE, "pintool", "ignore_nops", "1",
                   "When true, ignore NOP instructions when constructing data "
                   "dependencies. This also handles 'endbr' instructions, "
                   "since these are regarded by Pin as NOPs.");

// Option (-v) to print verbose output. This is useful for debugging and unit
// tests.
KNOB<bool> KnobVerbose(KNOB_MODE_WRITEONCE, "pintool", "v", "0",
                       "When true, print more verbose output. This is useful "
                       "for debugging and unit tests.");

// Option (-syscall_file) to also propagate dependencies for certain system
// calls (like the generic deobfuscator extension).
KNOB<std::string> KnobSyscallFile(
    KNOB_MODE_WRITEONCE, "pintool", "syscall_file", "",
    "When set, read system call specifications from the specified file, and "
    "also propagate dependencies for them.");

// Option (-ignore_rsp) to ignore dependencies through rsp.
KNOB<bool>
    KnobIgnoreRsp(KNOB_MODE_WRITEONCE, "pintool", "ignore_rsp", "0",
                  "When true, ignore dependencies through the stack pointer.");

// Option (-ignore_rip) to ignore dependencies through rip.
KNOB<bool> KnobIgnoreRip(
    KNOB_MODE_WRITEONCE, "pintool", "ignore_rip", "0",
    "When true, ignore dependencies through the instruction pointer.");

// Option (-ignore_rbp_stack_memory) to ignore dependencies through rbp, if rbp
// is only used to calculate the memory address of a memory operation, and that
// address falls into the stack region.
KNOB<bool> KnobIgnoreRbpStackMemory(
    KNOB_MODE_WRITEONCE, "pintool", "ignore_rbp_stack_memory", "0",
    "When true, ignore dependencies through rbp, but only if that dependency "
    "through rbp is used to calculate a memory address of a memory "
    "instruction, AND that address falls within the stack region, as indicated "
    "by memory mappings.");

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

// File streams to write the CSV output to.
static ofstream memoryDependenciesFile;
static ofstream registerDependenciesFile;
static ofstream syscallsFile;

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

// Maps each (thread, register) pair to the address of the instruction that last
// wrote to that register in that thread.
static std::map<std::pair<THREADID, REG>, ADDRINT> lastRegisterWrite;

// Maps each thread ID to the initial value of the stack pointer (i.e., the
// largest stack address).
static std::map<THREADID, ADDRINT> initialStackPointers;

// Maps each instruction to the set of (register, instruction) pairs on which it
// depends.
static std::map<ADDRINT, std::set<std::pair<REG, ADDRINT>>>
    registerDependencies;

// Maps each memory address to the address of the instruction that last wrote to
// it.
static std::map<ADDRINT, ADDRINT> lastMemoryWrite;

// Maps each instruction to the set of (memory address, instruction) pairs on
// which it depends.
static std::map<ADDRINT, std::set<std::pair<ADDRINT, ADDRINT>>>
    memoryDependencies;

// Mutex for the register-related data structures.
static PIN_MUTEX registerLock;

// Mutex for the memory-related data structures.
static PIN_MUTEX memoryLock;

// Mutex for sysCallInstructions.
static PIN_MUTEX sysCallInstructionsLock;

// Mutex for sysCallIdMap.
static PIN_MUTEX sysCallIdMapLock;

// Mutex for staticInstructionAddresses.
static PIN_MUTEX staticInstructionAddressesLock;

// Mutex for initialStackPointers.
static PIN_MUTEX initialStackPointersLock;

// Map to reuse virtual instructions with the same set of predecessors.
static std::map<std::set<std::pair<ADDRINT, ADDRINT>>, ADDRINT>
    virtualInstructionsMap;

static ADDRINT virtualInstructionCounter = 0;

// Map containing the operand overrides for each mov-like instruction. See the
// documentation of AddWriteAnalysisCalls for more information.
static const std::map<OPCODE, std::map<UINT32, UINT32>> movOperandMap =
    create_map<OPCODE, std::map<UINT32, UINT32>> // MOV_MAP_BEGIN

    (XED_ICLASS_CMOVB, create_map<UINT32, UINT32>(0, 1))     // cmovb dst, src
    (XED_ICLASS_CMOVBE, create_map<UINT32, UINT32>(0, 1))    // cmovbe dst, src
    (XED_ICLASS_CMOVL, create_map<UINT32, UINT32>(0, 1))     // cmovl dst, src
    (XED_ICLASS_CMOVLE, create_map<UINT32, UINT32>(0, 1))    // cmovle dst, src
    (XED_ICLASS_CMOVNB, create_map<UINT32, UINT32>(0, 1))    // cmovnb dst, src
    (XED_ICLASS_CMOVNBE, create_map<UINT32, UINT32>(0, 1))   // cmovnbe dst, src
    (XED_ICLASS_CMOVNL, create_map<UINT32, UINT32>(0, 1))    // cmovnl dst, src
    (XED_ICLASS_CMOVNLE, create_map<UINT32, UINT32>(0, 1))   // cmovnle dst, src
    (XED_ICLASS_CMOVNO, create_map<UINT32, UINT32>(0, 1))    // cmovno dst, src
    (XED_ICLASS_CMOVNP, create_map<UINT32, UINT32>(0, 1))    // cmovnp dst, src
    (XED_ICLASS_CMOVNS, create_map<UINT32, UINT32>(0, 1))    // cmovns dst, src
    (XED_ICLASS_CMOVNZ, create_map<UINT32, UINT32>(0, 1))    // cmovnz dst, src
    (XED_ICLASS_CMOVO, create_map<UINT32, UINT32>(0, 1))     // cmovo dst, src
    (XED_ICLASS_CMOVP, create_map<UINT32, UINT32>(0, 1))     // cmovp dst, src
    (XED_ICLASS_CMOVS, create_map<UINT32, UINT32>(0, 1))     // cmovs dst, src
    (XED_ICLASS_CMOVZ, create_map<UINT32, UINT32>(0, 1))     // cmovz dst, src
    (XED_ICLASS_LODSB, create_map<UINT32, UINT32>(0, 1))     // lodsb
    (XED_ICLASS_LODSD, create_map<UINT32, UINT32>(0, 1))     // lodsd
    (XED_ICLASS_LODSQ, create_map<UINT32, UINT32>(0, 1))     // lodsq
    (XED_ICLASS_LODSW, create_map<UINT32, UINT32>(0, 1))     // lodsw
    (XED_ICLASS_MOV, create_map<UINT32, UINT32>(0, 1))       // mov dst, src
    (XED_ICLASS_MOVAPS, create_map<UINT32, UINT32>(0, 1))    // movaps dst, src
    (XED_ICLASS_MOVD, create_map<UINT32, UINT32>(0, 1))      // movd dst, src
    (XED_ICLASS_MOVDQA, create_map<UINT32, UINT32>(0, 1))    // movdqa dst, src
    (XED_ICLASS_MOVDQU, create_map<UINT32, UINT32>(0, 1))    // movdqu dst, src
    (XED_ICLASS_MOVQ, create_map<UINT32, UINT32>(0, 1))      // movq dst, src
    (XED_ICLASS_MOVSB, create_map<UINT32, UINT32>(0, 2))     // movsb
    (XED_ICLASS_MOVSD, create_map<UINT32, UINT32>(0, 2))     // movsd
    (XED_ICLASS_MOVSQ, create_map<UINT32, UINT32>(0, 2))     // movsq
    (XED_ICLASS_MOVSW, create_map<UINT32, UINT32>(0, 2))     // movsw
    (XED_ICLASS_MOVSX, create_map<UINT32, UINT32>(0, 1))     // movsx dst, src
    (XED_ICLASS_MOVZX, create_map<UINT32, UINT32>(0, 1))     // movzx dst, src
    (XED_ICLASS_POP, create_map<UINT32, UINT32>(0, 2))       // pop dst
    (XED_ICLASS_PUSH, create_map<UINT32, UINT32>(2, 0))      // push src
    (XED_ICLASS_REP_LODSB, create_map<UINT32, UINT32>(0, 1)) // rep lodsb
    (XED_ICLASS_REP_LODSD, create_map<UINT32, UINT32>(0, 1)) // rep lodsd
    (XED_ICLASS_REP_LODSQ, create_map<UINT32, UINT32>(0, 1)) // rep lodsq
    (XED_ICLASS_REP_LODSW, create_map<UINT32, UINT32>(0, 1)) // rep lodsw
    (XED_ICLASS_REP_MOVSB, create_map<UINT32, UINT32>(0, 2)) // rep movsb
    (XED_ICLASS_REP_MOVSD, create_map<UINT32, UINT32>(0, 2)) // rep movsd
    (XED_ICLASS_REP_MOVSQ, create_map<UINT32, UINT32>(0, 2)) // rep movsq
    (XED_ICLASS_REP_MOVSW, create_map<UINT32, UINT32>(0, 2)) // rep movsw
    (XED_ICLASS_REP_STOSB, create_map<UINT32, UINT32>(0, 2)) // rep stosb
    (XED_ICLASS_REP_STOSD, create_map<UINT32, UINT32>(0, 2)) // rep stosd
    (XED_ICLASS_REP_STOSQ, create_map<UINT32, UINT32>(0, 2)) // rep stosq
    (XED_ICLASS_REP_STOSW, create_map<UINT32, UINT32>(0, 2)) // rep stosw
    (XED_ICLASS_STOSB, create_map<UINT32, UINT32>(0, 2))     // stosb
    (XED_ICLASS_STOSD, create_map<UINT32, UINT32>(0, 2))     // stosd
    (XED_ICLASS_STOSQ, create_map<UINT32, UINT32>(0, 2))     // stosq
    (XED_ICLASS_STOSW, create_map<UINT32, UINT32>(0, 2))     // stosw
    (XED_ICLASS_VMOVDQA, create_map<UINT32, UINT32>(0, 1))   // vmovdqa dst, src
    (XED_ICLASS_VMOVDQA32,
     create_map<UINT32, UINT32>(0, 1)) // vmovdqa32 dst, src
    (XED_ICLASS_VMOVDQA64,
     create_map<UINT32, UINT32>(0, 1))                     // vmovdqa64 dst, src
    (XED_ICLASS_VMOVDQU, create_map<UINT32, UINT32>(0, 1)) // vmovdqu dst, src
    (XED_ICLASS_VMOVDQU8, create_map<UINT32, UINT32>(0, 1)) // vmovdqu8 dst, src
    (XED_ICLASS_VMOVDQU16,
     create_map<UINT32, UINT32>(0, 1)) // vmovdqu16 dst, src
    (XED_ICLASS_VMOVDQU32,
     create_map<UINT32, UINT32>(0, 1)) // vmovdqu32 dst, src
    (XED_ICLASS_VMOVDQU64,
     create_map<UINT32, UINT32>(0, 1)) // vmovdqu64 dst, src

    ; // MOV_MAP_END

// System call to also track dependencies for.
struct SyscallEntry {
  SyscallEntry(unsigned int syscall, unsigned int index,
               unsigned int number_of_bytes_to_taint)
      : syscall(syscall), index(index),
        number_of_bytes_to_taint(number_of_bytes_to_taint) {}

  unsigned int syscall; // Syscall number to track.
  unsigned int index;   // 0-based index of which occurrence of that particular
                        // system call to track.
  unsigned int number_of_bytes_to_taint; // Override to determine how many bytes
                                         // of the associated buffer to taint.
};

// Convert between system call names and numbers.
// clang-format off
static const std::map<std::string, ADDRINT> syscallNos =
    create_map<std::string, ADDRINT>

    ("read", SYS_read)

    ;
// clang-format on

// List of system calls we need to track.
// TODO(perf): Use a more efficient datastructure for search.
static std::vector<SyscallEntry> syscallsToTrack;

// Keep track of how many times we executed each system call.
static std::map<ADDRINT, ADDRINT> sysCallCounters;

// Keep track of which instructions correspond to syscalls.
struct SyscallInfo {
  SyscallInfo(unsigned int index, unsigned int threadId, unsigned int syscallId)
      : index(index), threadId(threadId), syscallId(syscallId) {}

  unsigned int index;     // Index in the source file.
  unsigned int threadId;  // ID of the invoking thread.
  unsigned int syscallId; // ID of the syscall within its thread.

  bool operator<(const SyscallInfo &other) const {
    return std::tie(index, threadId, syscallId) <
           std::tie(other.index, other.threadId, other.syscallId);
  }
};

static std::map<SyscallInfo, ADDRINT> sysCallInstructions;

// Keep track of how many times we executed any system call in each tread.
static std::map<THREADID, unsigned int> sysCallIdMap;

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
    std::cerr << "\nMain reached, setting global flag.\n";
    main_reached = true;
  }

  // Check if main is finished.
  if (instruction_pointer == end_address) {
    std::cerr
        << "\nInstruction after call to main reached, setting global flag.\n";
    end_reached = true;
  }
}

// Runs before every call instruction.
VOID InstructionCallBefore(ADDRINT target_address,
                           ADDRINT next_instruction_pointer) {
  // Check if this is the first call to main().
  if ((end_address == INVALID_ADDRESS) && (target_address == main_address)) {
    std::cerr << "\nMain called, next ip = " << std::hex << std::showbase
              << next_instruction_pointer << std::dec << "\n";
    end_address = next_instruction_pointer;
  }
}

// Run at the start of __libc_start_main().
VOID LibcStartMainBefore(THREADID, ADDRINT main_addr) {
  std::cerr << "\n__libc_start_main(main = " << std::hex << std::showbase
            << main_addr << std::dec << ", ...)\n";

  main_address = main_addr;
}

// Run before every write to a register, excluding those handled by the next
// couple of functions.
VOID RegisterWriteBefore(THREADID threadID, ADDRINT ip, ADDRINT reg_) {
  if (!main_reached || end_reached)
    return;

  REG reg = (REG)reg_;

  PIN_MutexLock(&registerLock);

  // Update register map.
  lastRegisterWrite[std::make_pair(threadID, reg)] = ip;

  PIN_MutexUnlock(&registerLock);
}

// Run before every write to a register that is:
// 1) part of a mov-like instruction...
// 2) ... where the source is a register.
// i.e. mov dst_reg, src_reg
VOID RegisterWriteBeforeMovRegToReg(THREADID threadID, ADDRINT ip,
                                    ADDRINT dst_reg_, ADDRINT src_reg_) {
  if (!main_reached || end_reached)
    return;

  REG src_reg = (REG)src_reg_;
  REG dst_reg = (REG)dst_reg_;

  PIN_MutexLock(&registerLock);

  // Update register map.
  if (lastRegisterWrite.count(std::make_pair(threadID, src_reg))) {
    lastRegisterWrite[std::make_pair(threadID, dst_reg)] =
        lastRegisterWrite[std::make_pair(threadID, src_reg)];
  }

  PIN_MutexUnlock(&registerLock);
}

// Run before every write to a register that is:
// 1) part of a mov-like instruction...
// 2) ... where the source is a memory location.
// i.e. mov dst_reg, [memloc]
VOID RegisterWriteBeforeMovMemToReg(THREADID threadID, ADDRINT ip,
                                    ADDRINT dst_reg_, ADDRINT src_memLoc,
                                    ADDRINT src_size) {
  if (!main_reached || end_reached)
    return;

  REG dst_reg = (REG)dst_reg_;

  PIN_MutexLock(&registerLock);
  PIN_MutexLock(&memoryLock);

  // Update register map.

  // Keep track of (memory_address, instruction_address) of the last writes.
  std::set<std::pair<ADDRINT, ADDRINT>> lastWrites;
  std::set<ADDRINT> lastWriteInstructions;

  for (ADDRINT readAddr = src_memLoc; readAddr < src_memLoc + src_size;
       ++readAddr) {
    if (lastMemoryWrite.count(readAddr)) {
      lastWriteInstructions.insert(lastMemoryWrite[readAddr]);
      lastWrites.insert(std::pair(readAddr, lastMemoryWrite[readAddr]));
    }
  }

  if (lastWriteInstructions.size() == 0)
    ; // Nothing to do, as we haven't found an instruction that wrote to the
      // read memory location.
  else if (lastWriteInstructions.size() == 1)
    // Simple case: we found one instruction that wrote to the read memory
    // location.
    lastRegisterWrite[std::make_pair(threadID, dst_reg)] =
        *lastWriteInstructions.cbegin();
  else {
    // Complex case: we found more than one instruction that wrote to the read
    // memory location. In this case, we add a virtual "merge" node, which
    // merges these instructions into one. That is to say, this "merge" node has
    // as predecessors all those found instructions, and as successor(s) the
    // instruction(s) that read from the loaded-to register later on.
    PIN_MutexLock(&staticInstructionAddressesLock);

    // Try to reuse virtual instructions.
    ADDRINT virtualInsId;

    if (virtualInstructionsMap.count(lastWrites)) {
      virtualInsId = virtualInstructionsMap[lastWrites];
    } else {
      virtualInsId = static_cast<ADDRINT>(-1) - virtualInstructionCounter;

      staticInstructionAddresses.insert(std::make_pair(
          virtualInsId, StaticInstructionAddress("virtual-instructions",
                                                 virtualInstructionCounter)));
      virtualInstructionsMap[lastWrites] = virtualInsId;
      ++virtualInstructionCounter;
    }

    PIN_MutexUnlock(&staticInstructionAddressesLock);

    // Add predecessors, i.e. make sure that this virtual instruction depends on
    // all the instructions we found.
    for (const auto &lastWrite : lastWrites) {
      memoryDependencies[virtualInsId].insert(lastWrite);
    }

    lastRegisterWrite[std::make_pair(threadID, dst_reg)] = virtualInsId;
  }

  PIN_MutexUnlock(&memoryLock);
  PIN_MutexUnlock(&registerLock);
}

// Run after every read from a register.
VOID RegisterReadBefore(THREADID threadID, ADDRINT ip, ADDRINT reg_,
                        ADDRINT rbpMemLoc, ADDRINT rspValue) {
  if (!main_reached || end_reached)
    return;

  REG reg = (REG)reg_;

  PIN_MutexLock(&registerLock);
  PIN_MutexLock(&initialStackPointersLock);

  bool skipRegRead = false;

  // 128-byte buffer for red zone.
  if (KnobIgnoreRbpStackMemory.Value() &&
      (rbpMemLoc <= initialStackPointers[threadID]) &&
      (rspValue - 128 <= rbpMemLoc)) {
    skipRegRead = true;
  }

  if (!skipRegRead) {
    ADDRINT ip_read = ip;
    auto it = lastRegisterWrite.find(std::make_pair(threadID, reg));
    if (it != lastRegisterWrite.end()) {
      ADDRINT ip_write = it->second;

      // Add register dependency.
      registerDependencies[ip_read].insert(std::make_pair(reg, ip_write));
    }
  }

  PIN_MutexUnlock(&initialStackPointersLock);
  PIN_MutexUnlock(&registerLock);
}

// Run before every write to memory, excluding those handled by the next couple
// of functions.
VOID MemoryWriteBefore(ADDRINT ip, ADDRINT memLoc, ADDRINT size) {
  if (!main_reached || end_reached)
    return;

  PIN_MutexLock(&memoryLock);

  // Update memory map for every byte written.
  for (ADDRINT writtenAddr = memLoc; writtenAddr < memLoc + size;
       ++writtenAddr) {
    lastMemoryWrite[writtenAddr] = ip;
  }

  PIN_MutexUnlock(&memoryLock);
}

// Run before every write to memory that is:
// 1) part of a mov-like instruction...
// 2) ... where the source is a register.
// i.e. mov [memloc], src_reg
VOID MemoryWriteBeforeMovRegToMem(THREADID threadID, ADDRINT ip,
                                  ADDRINT dst_memLoc, ADDRINT dst_size,
                                  ADDRINT src_reg_) {
  if (!main_reached || end_reached)
    return;

  REG src_reg = (REG)src_reg_;

  PIN_MutexLock(&registerLock);
  PIN_MutexLock(&memoryLock);

  if (lastRegisterWrite.count(std::make_pair(threadID, src_reg))) {
    // Find instruction that last wrote to src_reg.
    auto instruction_source =
        lastRegisterWrite[std::make_pair(threadID, src_reg)];

    // Update memory map for every byte written.
    for (ADDRINT writtenAddr = dst_memLoc; writtenAddr < dst_memLoc + dst_size;
         ++writtenAddr) {
      lastMemoryWrite[writtenAddr] = instruction_source;
    }
  }

  PIN_MutexUnlock(&memoryLock);
  PIN_MutexUnlock(&registerLock);
}

// Run before every write to memory that is:
// 1) part of a mov-like instruction...
// 2) ... where the source is a memory location.
// i.e. mov [memloc_src], [memLoc_dst]
VOID MemoryWriteBeforeMovMemToMem(ADDRINT ip, ADDRINT dst_memLoc,
                                  ADDRINT dst_size, ADDRINT src_memLoc,
                                  ADDRINT src_size) {
  if (!main_reached || end_reached)
    return;

  assert((dst_size == src_size) && "Sizes must match!");

  PIN_MutexLock(&memoryLock);

  // Update memory map for every byte written.
  for (ADDRINT writtenAddr = dst_memLoc, readAddr = src_memLoc;
       (writtenAddr < dst_memLoc + dst_size) &&
       (readAddr < src_memLoc + src_size);
       ++writtenAddr, ++readAddr) {
    if (lastMemoryWrite.count(readAddr)) {
      lastMemoryWrite[writtenAddr] = lastMemoryWrite[readAddr];
    }
  }

  PIN_MutexUnlock(&memoryLock);
}

// Run before every read from memory.
VOID MemoryReadBefore(ADDRINT ip, ADDRINT memLoc, ADDRINT size) {
  if (!main_reached || end_reached)
    return;

  PIN_MutexLock(&memoryLock);

  ADDRINT ip_read = ip;

  // Iterate over all addresses read by this instruction.
  for (ADDRINT readAddr = memLoc; readAddr < memLoc + size; ++readAddr) {
    auto it = lastMemoryWrite.find(readAddr);
    if (it != lastMemoryWrite.end()) {
      ADDRINT ip_write = it->second;

      // Add memory dependency.
      memoryDependencies[ip_read].insert(std::make_pair(readAddr, ip_write));
    }
  }

  PIN_MutexUnlock(&memoryLock);
}

// =============================================================================
// Instrumentation routines
// =============================================================================

// Adds the default analysis calls for register and memory reads for an
// instruction.
VOID AddReadAnalysisCalls(INS ins) {
  // Ignore NOP instructions, since they don't read/write from their operands.
  // This also handles 'endbr64' instructions, because they are regarded by Pin
  // as 'nop edx, edi' instructions.
  if (KnobIgnoreNops.Value() && INS_IsNop(ins))
    return;

  std::optional<UINT32> memoryOperandUsingRbp;

  // Call RegisterReadBefore() before every register
  // read.
  for (UINT32 i = 0; i < INS_MaxNumRRegs(ins); ++i) {
    // NOTE: Canonicalise %al, %ah, %eax etc. to %rax using REG_FullRegName.
    REG fullReg = REG_FullRegName(INS_RegR(ins, i));

    // Special case "xor reg, reg" to not introduce a dependency.
    if ((INS_Opcode(ins) == XED_ICLASS_XOR) && (INS_OperandCount(ins) >= 2) &&
        (INS_OperandIsReg(ins, 0)) && (INS_OperandIsReg(ins, 1)) &&
        (INS_OperandReg(ins, 0) == INS_OperandReg(ins, 1)))
      continue;

    // Special case "pxor reg, reg" to not introduce a dependency.
    if ((INS_Opcode(ins) == XED_ICLASS_PXOR) && (INS_OperandCount(ins) >= 2) &&
        (INS_OperandIsReg(ins, 0)) && (INS_OperandIsReg(ins, 1)) &&
        (INS_OperandReg(ins, 0) == INS_OperandReg(ins, 1)))
      continue;

    // Ignore %rsp and %rip if requested by the user.
    if (KnobIgnoreRsp.Value() && fullReg == REG_STACK_PTR)
      continue;

    if (KnobIgnoreRip.Value() && fullReg == REG_INST_PTR)
      continue;

    if (KnobIgnoreRbpStackMemory.Value() && (fullReg == REG_GBP)) {
      for (UINT32 memOp = 0; memOp < INS_MemoryOperandCount(ins); ++memOp) {
        UINT32 op = INS_MemoryOperandIndexToOperandIndex(ins, memOp);

        if ((REG_FullRegName(INS_OperandMemoryBaseReg(ins, op)) == fullReg) ||
            (REG_FullRegName(INS_OperandMemoryIndexReg(ins, op)) == fullReg)) {

          assert((!memoryOperandUsingRbp || *memoryOperandUsingRbp == memOp) &&
                 "Expected only one memory operand to use rbp!");
          memoryOperandUsingRbp = memOp;
        }
      }
    }

    // NOTE: We run the 'read' analysis calls before all 'write' analysis
    // calls, because otherwise we may run into problems with instructions
    // such as 'add eax, 1'.
    if (!memoryOperandUsingRbp) {
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RegisterReadBefore,
                     IARG_THREAD_ID, IARG_INST_PTR, IARG_ADDRINT,
                     (ADDRINT)fullReg, IARG_ADDRINT, 0, IARG_REG_VALUE,
                     REG_STACK_PTR, IARG_CALL_ORDER, CALL_ORDER_FIRST,
                     IARG_END);
    } else {
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RegisterReadBefore,
                     IARG_THREAD_ID, IARG_INST_PTR, IARG_ADDRINT,
                     (ADDRINT)fullReg, IARG_MEMORYOP_EA, memoryOperandUsingRbp,
                     IARG_REG_VALUE, REG_STACK_PTR, IARG_CALL_ORDER,
                     CALL_ORDER_FIRST, IARG_END);
    }
  }

  // Call MemoryReadBefore() before every memory read. Note that an instruction
  // may have multiple memory operands.
  for (UINT32 memOp = 0; memOp < INS_MemoryOperandCount(ins); memOp++) {
    // Get the number of bytes of this memory operand.
    ADDRINT size = INS_MemoryOperandSize(ins, memOp);

    // Use predicated call so that instrumentation is only called for
    // instructions that are actually executed. This is important for
    // conditional moves and instructions with a REP prefix.

    // NOTE: We run the 'read' analysis calls before all 'write' analysis
    // calls, because otherwise we may run into problems with instructions
    // such as 'add eax, 1'.
    if (INS_MemoryOperandIsRead(ins, memOp)) {
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)MemoryReadBefore,
                               IARG_INST_PTR, IARG_MEMORYOP_EA, memOp,
                               IARG_ADDRINT, size, IARG_CALL_ORDER,
                               CALL_ORDER_FIRST, IARG_END);
    }
  }
}

struct RegMemOverrides {
  std::map<REG, REG> overrideRegToReg; // Written register, read register.
  std::map<REG, UINT32>
      overrideRegToMem; // Written register, read memory location.
  std::map<UINT32, REG>
      overrideMemToReg; // Written memory location, read register.
  std::map<UINT32, UINT32>
      overrideMemToMem; // Written memory location, read memory location.
};

// Splits the operandOverrides map (which contains operand indices) to four
// maps, mapping {registers, memory operands} to {registers, memory operands}.
// For example, a "mov rax, rcx" instruction with an operandOverrides map of
// {(operand 0, operand 1)} would result in a register to register map of {(rax,
// rcx)}. Similarly, a "mov [memLoc], rdx" with an operandOverrides of {(operand
// 0, operand 1)} will result in a memory to register map of {(memOp 0, rdx)}.
RegMemOverrides operandOverridesToRegMemOverrides(
    INS ins, const std::map<UINT32, UINT32> &operandOverrides) {
  // Convert the operand indices (key) to memory operand indices (value).
  std::map<UINT32, UINT32> operand_index_to_memory_operand_index;

  for (UINT32 i = 0; i < INS_MemoryOperandCount(ins); ++i) {
    operand_index_to_memory_operand_index[INS_MemoryOperandIndexToOperandIndex(
        ins, i)] = i;
  }

  // Create return value.
  RegMemOverrides ret;

  for (const auto &operandOverride : operandOverrides) {
    const auto &writeOperand = operandOverride.first;
    const auto &readOperand = operandOverride.second;

    if (INS_OperandIsReg(ins, writeOperand) &&
        INS_OperandIsReg(ins, readOperand)) {
      // Case 1: writeOperand = register, readOperand = register
      ret.overrideRegToReg[REG_FullRegName(INS_OperandReg(ins, writeOperand))] =
          REG_FullRegName(INS_OperandReg(ins, readOperand));
    } else if (INS_OperandIsReg(ins, writeOperand) &&
               INS_OperandIsMemory(ins, readOperand)) {
      // Case 2: writeOperand = register, readOperand = memory
      ret.overrideRegToMem[REG_FullRegName(INS_OperandReg(ins, writeOperand))] =
          operand_index_to_memory_operand_index[readOperand];
    } else if (INS_OperandIsMemory(ins, writeOperand) &&
               INS_OperandIsReg(ins, readOperand)) {
      // Case 3: writeOperand = memory, readOperand = register
      ret.overrideMemToReg
          [operand_index_to_memory_operand_index[writeOperand]] =
          REG_FullRegName(INS_OperandReg(ins, readOperand));
    } else if (INS_OperandIsMemory(ins, writeOperand) &&
               INS_OperandIsMemory(ins, readOperand)) {
      // Case 4: writeOperand = memory, readOperand = memory
      ret.overrideMemToMem
          [operand_index_to_memory_operand_index[writeOperand]] =
          operand_index_to_memory_operand_index[readOperand];
    } else {
      // Case 5: anything else (e.g. readOperand is an immediate).
    }
  }

  return ret;
}

// Adds the default analysis calls for register and memory writes for an
// instruction.

// The parameter 'operandOverrides' contains a set of overrides for 'shortcut'
// arrows. The key represents the index of the 'write' operand, and the value
// represents the index of the 'read' operand.

// This override works as follows. Suppose an item (dst_op, src_op) is part of
// the map. This means that the instruction that last wrote to dst_op will be
// set to the instruction that last wrote to src_op, instead of to the current
// instruction. This essentially comes down to adding a 'shorcut' arrow for an
// instruction that is semantically equivalent to "mov dst_op, src_op".
VOID AddWriteAnalysisCalls(
    INS ins, const std::map<UINT32, UINT32> &operandOverrides = {}) {
  // Ignore NOP instructions, since they don't read/write from their operands.
  // This also handles 'endbr64' instructions, because they are regarded by Pin
  // as 'nop edx, edi' instructions.
  if (KnobIgnoreNops.Value() && INS_IsNop(ins))
    return;

  // Convert overrides in terms of operand indices to overrides in terms of
  // registers and memory operands.
  RegMemOverrides overrides =
      operandOverridesToRegMemOverrides(ins, operandOverrides);

  // Call RegisterWriteBefore() before every register
  // write.
  for (UINT32 i = 0; i < INS_MaxNumWRegs(ins); ++i) {
    // NOTE: Canonicalise %al, %ah, %eax etc. to %rax using REG_FullRegName.
    REG fullReg = REG_FullRegName(INS_RegW(ins, i));

    // Ignore %rsp and %rip if requested by the user.
    if (KnobIgnoreRsp.Value() && fullReg == REG_STACK_PTR)
      continue;

    if (KnobIgnoreRip.Value() && fullReg == REG_INST_PTR)
      continue;

    // NOTE: We run the 'read' analysis calls before all 'write' analysis
    // calls, because otherwise we may run into problems with instructions
    // such as 'add eax, 1'.

    if (overrides.overrideRegToReg.count(fullReg)) {
      INS_InsertCall(
          ins, IPOINT_BEFORE, (AFUNPTR)RegisterWriteBeforeMovRegToReg,
          IARG_THREAD_ID, IARG_INST_PTR, IARG_ADDRINT, (ADDRINT)fullReg,
          IARG_ADDRINT, (ADDRINT)(overrides.overrideRegToReg[fullReg]),
          IARG_CALL_ORDER, CALL_ORDER_FIRST + 1, IARG_END);
    } else if (overrides.overrideRegToMem.count(fullReg)) {
      INS_InsertCall(
          ins, IPOINT_BEFORE, (AFUNPTR)RegisterWriteBeforeMovMemToReg,
          IARG_THREAD_ID, IARG_INST_PTR, IARG_ADDRINT, (ADDRINT)fullReg,
          IARG_MEMORYOP_EA, overrides.overrideRegToMem[fullReg], IARG_ADDRINT,
          (ADDRINT)INS_MemoryOperandSize(ins,
                                         overrides.overrideRegToMem[fullReg]),
          IARG_CALL_ORDER, CALL_ORDER_FIRST + 1, IARG_END);
    } else {
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RegisterWriteBefore,
                     IARG_THREAD_ID, IARG_INST_PTR, IARG_ADDRINT,
                     (ADDRINT)fullReg, IARG_CALL_ORDER, CALL_ORDER_FIRST + 1,
                     IARG_END);
    }
  }

  // Call MemoryWriteBefore() before every memory write. Note that an
  // instruction may have multiple memory operands.
  for (UINT32 memOp = 0; memOp < INS_MemoryOperandCount(ins); memOp++) {
    // Use predicated call so that instrumentation is only called for
    // instructions that are actually executed. This is important for
    // conditional moves and instructions with a REP prefix.

    // NOTE: We run the 'read' analysis calls before all 'write' analysis
    // calls, because otherwise we may run into problems with instructions
    // such as 'add eax, 1'.
    if (INS_MemoryOperandIsWritten(ins, memOp)) {
      if (overrides.overrideMemToReg.count(memOp)) {
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, (AFUNPTR)MemoryWriteBeforeMovRegToMem,
            IARG_THREAD_ID, IARG_INST_PTR, IARG_MEMORYOP_EA, memOp,
            IARG_ADDRINT, (ADDRINT)INS_MemoryOperandSize(ins, memOp),
            IARG_ADDRINT, (ADDRINT)(overrides.overrideMemToReg[memOp]),
            IARG_CALL_ORDER, CALL_ORDER_FIRST + 1, IARG_END);
      } else if (overrides.overrideMemToMem.count(memOp)) {
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, (AFUNPTR)MemoryWriteBeforeMovMemToMem,
            IARG_INST_PTR, IARG_MEMORYOP_EA, memOp, IARG_ADDRINT,
            (ADDRINT)INS_MemoryOperandSize(ins, memOp), IARG_MEMORYOP_EA,
            overrides.overrideMemToMem[memOp], IARG_ADDRINT,
            (ADDRINT)INS_MemoryOperandSize(ins,
                                           overrides.overrideMemToMem[memOp]),
            IARG_CALL_ORDER, CALL_ORDER_FIRST + 1, IARG_END);
      } else {
        INS_InsertPredicatedCall(
            ins, IPOINT_BEFORE, (AFUNPTR)MemoryWriteBefore, IARG_INST_PTR,
            IARG_MEMORYOP_EA, memOp, IARG_ADDRINT,
            (ADDRINT)INS_MemoryOperandSize(ins, memOp), IARG_CALL_ORDER,
            CALL_ORDER_FIRST + 1, IARG_END);
      }
    }
  }
}

// Pin calls this function every time a new instruction is encountered
VOID OnInstruction(INS ins, VOID *) {
  // Call InstructionBefore() before every instruction.
  INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)InstructionBefore, IARG_INST_PTR,
                 IARG_END);

  // Call InstructionCallBefore() before every call instruction.
  // Pass the target address (i.e. the address of the function being called)
  // and the address of the next instruction (i.e. the one following the
  // call).
  if (INS_IsCall(ins)) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)InstructionCallBefore,
                   IARG_BRANCH_TARGET_ADDR, IARG_ADDRINT, INS_NextAddress(ins),
                   IARG_END);
  }

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

  PIN_MutexLock(&staticInstructionAddressesLock);
  // Add the static address of this instruction to the static instructions
  // map.
  staticInstructionAddresses.insert(std::make_pair(
      ins_address, StaticInstructionAddress(image_name, image_offset)));

  PIN_MutexUnlock(&staticInstructionAddressesLock);

  // Add read calls for all instructions.
  AddReadAnalysisCalls(ins);

  // For the write calls, we need to special case move-like instructions, so
  // check if this instruction needs to be special cased.
  auto it = movOperandMap.find(INS_Opcode(ins));
  if (KnobShortcuts.Value() && (it != movOperandMap.end())) {
    // Debug output.
    if (KnobVerbose.Value()) {

      // NOTE: We replace '[' by '(' and ']' by ')', because '[' and ']' are
      // special characters in FileCheck.
      auto escape = [](std::string s) {
        for (auto &c : s) {
          if (c == '[')
            c = '(';
          else if (c == ']')
            c = ')';
        }

        return s;
      };

      // Print instruction disassembly.
      std::cerr << "[DEBUG] [MOVOPERANDMAP] Instruction: "
                << escape(INS_Disassemble(ins)) << "\n";

      // Print info for registered (dst, src) operand pairs.
      for (const auto &pair : it->second) {
        std::cerr << "[DEBUG] [MOVOPERANDMAP] Pair: dst = "
                  << escape(ins_operand_to_string(ins, pair.first))
                  << ", src = "
                  << escape(ins_operand_to_string(ins, pair.second)) << "\n";
      }

      // Print info for operands.
      std::cerr << "[DEBUG] [MOVOPERANDMAP] No. of operands: "
                << INS_OperandCount(ins) << "\n";

      for (UINT32 i = 0; i < INS_OperandCount(ins); ++i) {
        std::cerr << "[DEBUG] [MOVOPERANDMAP] Operand " << i << ": "
                  << escape(ins_operand_to_string(ins, i)) << "\n";
      }
    }

    AddWriteAnalysisCalls(ins, it->second);
  } else {
    AddWriteAnalysisCalls(ins);
  }
}

// Instrumentation routine run for every image loaded.
VOID OnImageLoad(IMG img, VOID *) {
  RTN libc_start_main_rtn = RTN_FindByName(img, "__libc_start_main");
  if (RTN_Valid(libc_start_main_rtn)) {
    RTN_Open(libc_start_main_rtn);
    RTN_InsertCall(libc_start_main_rtn, IPOINT_BEFORE,
                   (AFUNPTR)LibcStartMainBefore, IARG_THREAD_ID,
                   IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END);
    RTN_Close(libc_start_main_rtn);
  }
}

// Callback that is executed before each system call.
VOID OnSyscallEntry(THREADID threadId, CONTEXT *ctx, SYSCALL_STANDARD std,
                    VOID *v) {
  PIN_MutexLock(&sysCallIdMapLock);
  auto sysCallId = sysCallIdMap[threadId]++;
  PIN_MutexUnlock(&sysCallIdMapLock);

  if (syscallsToTrack.empty())
    return;

  ADDRINT sysCallNo = PIN_GetSyscallNumber(ctx, std);
  auto it = std::find_if(syscallsToTrack.cbegin(), syscallsToTrack.cend(),
                         [&](const SyscallEntry &sce) {
                           return (sce.syscall == sysCallNo) &&
                                  (sce.index == sysCallCounters[sysCallNo]);
                         });

  ++sysCallCounters[sysCallNo];

  if (it == syscallsToTrack.cend())
    return;

  // TODO: Make this extensible for different syscalls, i.e. do not hardcode
  // argument index.
  ADDRINT ip = PIN_GetContextReg(ctx, REG_INST_PTR);
  ADDRINT baseAddr = PIN_GetSyscallArgument(ctx, std, 1);
  ADDRINT count = it->number_of_bytes_to_taint;

  PIN_MutexLock(&sysCallInstructionsLock);
  sysCallInstructions.insert(
      std::pair(SyscallInfo(it->index, threadId, sysCallId), ip));
  PIN_MutexUnlock(&sysCallInstructionsLock);

  PIN_MutexLock(&memoryLock);

  // Update memory map for every byte written.
  for (ADDRINT writtenAddr = baseAddr; writtenAddr < baseAddr + count;
       ++writtenAddr) {
    lastMemoryWrite[writtenAddr] = ip;
  }

  PIN_MutexUnlock(&memoryLock);
}

// Callback for when a thread starts.
VOID OnThreadStart(THREADID threadId, CONTEXT *ctx, INT32 flags, VOID *v) {
  PIN_MutexLock(&initialStackPointersLock);
  initialStackPointers[threadId] = PIN_GetContextReg(ctx, REG_STACK_PTR);
  PIN_MutexUnlock(&initialStackPointersLock);
}

// =============================================================================
// Other routines
// =============================================================================

std::string pretty_offset(ADDRINT offset) {
  if (offset != static_cast<ADDRINT>(-1)) {
    std::ostringstream oss;
    oss << offset;
    return oss.str();
  } else {
    return "-1";
  }
}

// This function is called when the application exits
VOID Fini(INT32, VOID *) {
  // Write register dependencies to CSV file.
  registerDependenciesFile
      << "Write_img,Write_off,Register,Read_img,Read_off\n";

  for (const auto &instructionEntry : registerDependencies) {
    ADDRINT ip_read = instructionEntry.first;
    StaticInstructionAddress address_read = staticInstructionAddresses[ip_read];

    for (const auto &registerDependency : instructionEntry.second) {
      REG reg = registerDependency.first;
      ADDRINT ip_write = registerDependency.second;
      StaticInstructionAddress address_write =
          staticInstructionAddresses[ip_write];

      registerDependenciesFile
          << '"' << get_filename(address_write.image_name) << '"' // Write_img
          << ',' << pretty_offset(address_write.image_offset)     // Write_off
          << ',' << REG_StringShort(reg)                          // Register
          << ',' << '"' << get_filename(address_read.image_name)
          << '"'                                             // Read_img
          << ',' << pretty_offset(address_read.image_offset) // Read_off
          << '\n';
    }
  }

  // Write memory dependencies to CSV file.
  memoryDependenciesFile << "Write_img,Write_off,Memory,Read_img,Read_off\n";

  for (const auto &instructionEntry : memoryDependencies) {
    ADDRINT ip_read = instructionEntry.first;
    StaticInstructionAddress address_read = staticInstructionAddresses[ip_read];

    for (const auto &memoryDependency : instructionEntry.second) {
      ADDRINT memLoc = memoryDependency.first;
      ADDRINT ip_write = memoryDependency.second;
      StaticInstructionAddress address_write =
          staticInstructionAddresses[ip_write];

      memoryDependenciesFile
          << '"' << get_filename(address_write.image_name) << '"' // Write_img
          << ',' << pretty_offset(address_write.image_offset)     // Write_off
          << ',' << memLoc                                        // Memory
          << ',' << '"' << get_filename(address_read.image_name)
          << '"'                                             // Read_img
          << ',' << pretty_offset(address_read.image_offset) // Read_off
          << '\n';
    }
  }

  // Write system call instruction to CSV file.
  syscallsFile << "image_name,image_offset,index,thread_id,syscall_id\n";

  for (const auto &sci : sysCallInstructions) {
    const auto &info = sci.first;
    const auto &ip = sci.second;

    StaticInstructionAddress address = staticInstructionAddresses[ip];

    syscallsFile << '"' << get_filename(address.image_name) << '"' // image_name
                 << ',' << pretty_offset(address.image_offset) // image_offset
                 << ',' << info.index                          // index
                 << ',' << info.threadId                       // thread_id
                 << ',' << info.syscallId                      // syscall_id
                 << '\n';
  }

  // Flush and close the output files.
  memoryDependenciesFile.flush();
  memoryDependenciesFile.close();

  registerDependenciesFile.flush();
  registerDependenciesFile.close();

  syscallsFile.flush();
  syscallsFile.close();
}

bool earlyFini(unsigned int, int, LEVEL_VM::CONTEXT *, bool,
               const LEVEL_BASE::EXCEPTION_INFO *, void *) {
  cerr << "signal caught" << endl;
  Fini(0, nullptr);
  PIN_ExitProcess(0);
}

INT32 Usage() {
  PIN_ERROR("This Pin tool collects true data dependencies (RAW): for each "
            "instruction, it records which instructions it depends on.\n"
            "Both dependencies through registers and through memory are "
            "collected.\n" +
            KNOB_BASE::StringKnobSummary() + "\n");
  return -1;
}

int main(int argc, char *argv[]) {
  PIN_InitSymbols();

  // Initialise Pin and SDE.
  sde_pin_init(argc, argv);
  sde_init();

  // If we do not want to start at main() only, set 'main_reached' to true
  // already. This results in the entire executable being analysed.
  if (!KnobStartFromMain.Value())
    main_reached = true;

  std::string csv_prefix = KnobCsvPrefix.Value();

  if (csv_prefix.empty()) {
    csv_prefix = "data_dependencies";
  }

  // Open CSV files.
  memoryDependenciesFile.open(
      (csv_prefix + ".memory_dependencies.csv").c_str());
  registerDependenciesFile.open(
      (csv_prefix + ".register_dependencies.csv").c_str());
  syscallsFile.open((csv_prefix + ".syscall_instructions.csv").c_str());

  // Parse syscall file, if set.
  if (!KnobSyscallFile.Value().empty()) {
    std::ifstream syscallFile(KnobSyscallFile.Value());

    std::string line;

    while (std::getline(syscallFile, line)) {
      std::istringstream lineSs(line);
      std::vector<std::string> parts;
      std::string part;

      while (std::getline(lineSs, part, ',')) {
        parts.emplace_back(part);
      }

      assert((parts.size() == 3) && "Expected format syscall,index,size!");

      auto syscallIt = syscallNos.find(parts[0]);
      assert((syscallIt != syscallNos.end()) && "Unrecognised system call");

      syscallsToTrack.emplace_back(SyscallEntry(
          syscallIt->second, std::stoi(parts[1]), std::stoi(parts[2])));
    }
  }

  // Intercept image loading
  IMG_AddInstrumentFunction(OnImageLoad, nullptr);

  // Register Instruction to be called to instrument instructions
  INS_AddInstrumentFunction(OnInstruction, nullptr);

  // Register callback for system call entry.
  PIN_AddSyscallEntryFunction(OnSyscallEntry, nullptr);

  // Register callback for thread start.
  PIN_AddThreadStartFunction(OnThreadStart, nullptr);

  // Register Fini to be called when the application exits
  PIN_AddFiniFunction(Fini, nullptr);

  // Make sure we can cut off execution
  PIN_UnblockSignal(15, true);
  PIN_InterceptSignal(15, earlyFini, nullptr);

  // Initialize mutexes.
  PIN_MutexInit(&registerLock);
  PIN_MutexInit(&memoryLock);
  PIN_MutexInit(&sysCallInstructionsLock);
  PIN_MutexInit(&sysCallIdMapLock);
  PIN_MutexInit(&staticInstructionAddressesLock);
  PIN_MutexInit(&initialStackPointersLock);

  // Start the program, never returns
  PIN_StartProgram();

  return 0;
}
