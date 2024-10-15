#ifndef TEMPORALENTROPYINFO_H
#define TEMPORALENTROPYINFO_H

#include <vector>

#include "meanaggregator.h"

// Struct used to keep counters for temporal entropy calculations per bit.
struct BitCounter {
  // Creates a new BitCounter.
  BitCounter() : count_0(0), count_1(0) {}

  // Number of times this bit was 0.
  unsigned int count_0;

  // Number of times this bit was 1.
  unsigned int count_1;
};

// Contains information needed to calculate the temporal entropy info of
// contiguous regions of memory.
class TemporalEntropyInfo {
public:
  // Creates a new TemporalEntropyInfo for a contiguous memory region consisting
  // of 'size' bytes.
  TemporalEntropyInfo(unsigned int size)
      : bit_counters(8 * size, BitCounter()), buf_size(size) {}

  // Returns the temporal entropy, averaged over space (i.e. over different bits
  // in the buffer), and calculated using Shannon's bit entropy metric.
  float get_average_bit_shannon_entropy() const;

  // Update the temporal entropy info with the contents of the memory region,
  // starting at the specified address and of a given size.
  void record(const unsigned char *start_address, unsigned int size);

private:
  // Counters for each bit how many times it was 0 or 1. Bytes are ordered
  // according to increasing memory address, bits within a byte are ordered
  // least-significant bit first.
  std::vector<BitCounter> bit_counters;

  // The size of the buffer in bytes. This is set by the ctor.
  unsigned int buf_size;
};

#endif
