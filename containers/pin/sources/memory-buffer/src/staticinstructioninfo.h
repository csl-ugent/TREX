#ifndef STATICINSTRUCTIONINFO_H
#define STATICINSTRUCTIONINFO_H

#include <set>
#include <string>
#include <vector>

#include "staticinstructionaddress.h"

// Struct used to keep information per static instruction (i.e. per value of
// RIP).
class StaticInstructionInfo {
public:
  // Create a new StaticInstructionInfo with a given disassembly representation
  // and instruction address.
  StaticInstructionInfo(const std::string &disassembly, ADDRINT addr)
      : address(addr) {
    // Split the disassembly in opcode (first word) and operands (rest of
    // string).
    const auto first_space = disassembly.find(' ');

    opcode = disassembly.substr(0, first_space);
    operands = disassembly.substr(first_space + 1);

    // Zero out the counter arrays.
    for (std::size_t i = 0; i < 256; ++i) {
        byte_counts_read[i] = 0;
        byte_counts_written[i] = 0;
    }
  }

  // Opcode of the instruction in Intel syntax (e.g. ret, add, xor, ...).
  std::string opcode;

  // Operands of the instruction in Intel syntax (e.g. rax, dword ptr [rcx],
  // ...).
  std::string operands;

  // Address of this instruction.
  StaticInstructionAddress address;

  // Vector of values read by the instruction. Every element is itself a vector,
  // representing the different bytes of the read value.
  std::set<std::vector<unsigned char>> read_values;

  // Vector of values written by the instruction. Every element is itself a
  // vector, representing the different bytes of the read value.
  std::set<std::vector<unsigned char>> written_values;

  // Counters for the number of times a given byte value was read/written.
  unsigned int byte_counts_read[256];
  unsigned int byte_counts_written[256];
};

#endif
