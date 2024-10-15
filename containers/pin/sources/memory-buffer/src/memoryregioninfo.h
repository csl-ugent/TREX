#ifndef MEMORYINFO_H
#define MEMORYINFO_H

#include <set>

#include "spatialentropyinfo.h"
#include "temporalentropyinfo.h"

#include "pin.H"

// Struct used to keep information for contiguous memory buffers or memory
// regions.
class MemoryRegionInfo {
public:
  // Creates a new MemoryRegion for a contiguous memory region consisting of
  // 'size' bytes.
  MemoryRegionInfo(unsigned int size, ADDRINT allocation_addr = 0)
      : num_reads(0), num_writes(0), spatial_entropy_info(),
        temporal_entropy_info(size), allocation_address(allocation_addr) {}

  // Total number of reads from any address inside this memory region.
  unsigned int num_reads;

  // Total number of writes to any address inside this memory region.
  unsigned int num_writes;

  // Information needed to calculate the spatial entropy of this memory region.
  SpatialEntropyInfo spatial_entropy_info;

  // Information needed to calculate the temporal entropy of this memory region.
  TemporalEntropyInfo temporal_entropy_info;

  // The set of static instructions (i.e. values of IP) that read from this
  // memory region.
  std::set<ADDRINT> read_static_instructions;

  // The set of static instructions (i.e. values of IP) that write to this
  // memory region.
  std::set<ADDRINT> write_static_instructions;

  // The address of the instruction that allocated this buffer.
  ADDRINT allocation_address;

  // Backtrace of the allocation (i.e. malloc()).
  // NOTE: Only used for debugging!
  std::string DEBUG_allocation_backtrace;

  // User-provided information for this memory buffer/region.
  // NOTE: Only used for debugging!
  std::string DEBUG_annotation;
};

#endif
