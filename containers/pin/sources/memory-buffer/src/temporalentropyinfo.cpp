#include "entropy.h"
#include "temporalentropyinfo.h"

#include "pin.H"

float TemporalEntropyInfo::get_average_bit_shannon_entropy() const {
  // Create an aggregator to calculate the mean temporal entropy.
  MeanAggregator<float> aggregator;

  // Iterate over every bit, and calculate the binary entropy of the bit's
  // distribution.
  for (const auto &bit_counter : bit_counters) {
    const float c_0 = static_cast<float>(bit_counter.count_0);
    const float c_1 = static_cast<float>(bit_counter.count_1);

    const float fraction_one_bits = c_1 / (c_0 + c_1);
    aggregator.add_sample(binary_entropy(fraction_one_bits));
  }

  return aggregator.get_mean();
}

void TemporalEntropyInfo::record(const unsigned char *start_address,
                                 unsigned int size) {

  // Check that we are getting the entire buffer.
  if (size != this->buf_size) {
    PIN_ExitProcess(300);
  }

  // Iterate over every byte in the buffer.
  for (std::size_t byte = 0; byte < size; ++byte) {
    unsigned char byte_value;
    PIN_SafeCopy(&byte_value, start_address + byte, 1);

    // Iterate over every bit in that byte, least significant bit first.
    for (std::size_t bit = 0; bit < 8; ++bit) {
      // Get the index corresponding to the current bit.
      const auto index = byte * 8 + bit;

      // Get the BitCounter for the current bit.
      BitCounter &bit_counter = bit_counters[index];

      // Increment counters depending on the value of the bit.
      const auto bit_value = (byte_value >> bit) & 1;

      bit_counter.count_0 += !bit_value;
      bit_counter.count_1 += bit_value;
    }
  }
}
