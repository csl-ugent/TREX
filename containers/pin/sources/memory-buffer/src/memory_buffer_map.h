#ifndef MEMORY_BUFFER_MAP_H
#define MEMORY_BUFFER_MAP_H

#include "memory_buffer.h"

// Comparator for MemoryBuffer that compares the end address.
class MemoryBufferEndAddressComparator {
public:
  bool operator()(const MemoryBuffer &lhs, const MemoryBuffer &rhs) const {
    return lhs.end_address < rhs.end_address;
  }
};

// A map from memory buffers to any arbitrary type T.
// Assumes that the memory buffers in the map do not overlap.
// This map supports efficient lookup of the buffer containing a given memory
// address.
template <typename T>
class MemoryBufferMap
    : public std::map<MemoryBuffer, T, MemoryBufferEndAddressComparator> {
public:
  // Returns an iterator to the memory buffer that contains a given address
  // 'addr', i.e. for which (buffer.start_address <= addr) && (addr <
  // buffer.end_address).
  typename std::map<MemoryBuffer, T, MemoryBufferEndAddressComparator>::iterator
  find_buffer_containing_address(ADDRINT addr) {
    // Find first buffer with addr < buffer.end_address.
    typename std::map<MemoryBuffer, T,
                      MemoryBufferEndAddressComparator>::iterator buffer =
        this->upper_bound(MemoryBuffer(addr, addr));

    // If we didn't find any buffer, return.
    if (buffer == this->end())
      return buffer;

    // Otherwise, check if the start_address is OK.
    if (buffer->first.start_address <= addr)
      return buffer;
    else
      return this->end();
  }

  // Returns an iterator to the memory buffer that contains a given address
  // 'addr', i.e. for which (buffer.start_address <= addr) && (addr <
  // buffer.end_address).
  typename std::map<MemoryBuffer, T,
                    MemoryBufferEndAddressComparator>::const_iterator
  find_buffer_containing_address(ADDRINT addr) const {
    // Find first buffer with addr < buffer.end_address.
    typename std::map<MemoryBuffer, T,
                      MemoryBufferEndAddressComparator>::const_iterator buffer =
        this->upper_bound(MemoryBuffer(addr, addr));

    // If we didn't find any buffer, return.
    if (buffer == this->cend())
      return buffer;

    // Otherwise, check if the start_address is OK.
    if (buffer->first.start_address <= addr)
      return buffer;
    else
      return this->cend();
  }
};

#endif
