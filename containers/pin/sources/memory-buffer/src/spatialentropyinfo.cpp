#include <math.h>

#include "entropy.h"
#include "spatialentropyinfo.h"

#include "pin.H"

void SpatialEntropyInfo::record(const unsigned char *start_address,
                                unsigned int size) {
  unsigned char byte;

  unsigned int total_hamming_weight = 0;
  unsigned int byte_counts[256] = {};

  for (const unsigned char *ptr = start_address; ptr < start_address + size;
       ++ptr) {
    PIN_SafeCopy(&byte, ptr, 1);

    // Calculate the total number of 1-bits (= hamming weight) of the data in
    // the buffer.
    total_hamming_weight += hamming_weight(byte);

    // Count the number of occurences of every byte value.
    ++byte_counts[byte];
  }

  // Bit-level entropy
  // -----------------

  float fraction_one_bits =
      static_cast<float>(total_hamming_weight) / (8 * size);

  // Record the average bit value.
  bit_average_aggregator.add_sample(fraction_one_bits);

  // Calculate the Shannon bit-level entropy based on the hamming weight.
  float shannon_bit_entropy = binary_entropy(fraction_one_bits);
  bit_shannon_aggregator.add_sample(shannon_bit_entropy);

  // Byte-level entropy
  // ------------------

  float shannon_byte = 0;
  unsigned int num_different = 0;
  unsigned int num_unique = 0;
  unsigned int sum = 0;

  for (unsigned int i = 0; i < 256; ++i) {
    unsigned int count = byte_counts[i];

    if (count > 0) {
      // fraction that this byte value occurs
      float p = static_cast<float>(count) / static_cast<float>(size);
      shannon_byte -= p * log2(p);

      ++num_different;

      sum += count * i;
    }

    if (count == 1) {
      ++num_unique;
    }
  }

  byte_shannon_aggregator.add_sample(shannon_byte / log2(static_cast<float>(256)));
  byte_shannon_adapted_aggregator.add_sample(shannon_byte / log2(static_cast<float>(std::min(256u, size))));
  byte_num_different_aggregator.add_sample(num_different);
  byte_num_unique_aggregator.add_sample(num_unique);
  byte_average_aggregator.add_sample(sum / static_cast<float>(size));
}
