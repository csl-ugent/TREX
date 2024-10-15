#ifndef MEMORY_BUFFER_H
#define MEMORY_BUFFER_H

#include <iostream>
#include <map>
#include <sstream>

#include "pin.H"

// Represents buffer in memory, with a start and end address.
struct MemoryBuffer {
  MemoryBuffer(ADDRINT start_address, ADDRINT end_address)
      : start_address(start_address), end_address(end_address) {}

  // Start address of buffer, inclusive.
  ADDRINT start_address;

  // End address of buffer, exclusive.
  ADDRINT end_address;
};

std::ostream &operator<<(std::ostream &os, const MemoryBuffer &buf) {
  std::ostringstream oss;
  oss << "Buffer " << std::hex << std::showbase << buf.start_address << " --> "
     << buf.end_address << std::dec;
  os << oss.str();
  return os;
}

#endif
