#ifndef SPATIALENTROPYINFO_H
#define SPATIALENTROPYINFO_H

#include "meanaggregator.h"

// Contains information needed to calculate the spatial entropy of contiguous
// regions of memory.
class SpatialEntropyInfo {
public:
  // Creates a new SpatialEntropyInfo.
  SpatialEntropyInfo() {}

  // Returns the average spatial entropy according to Shannon's bit entropy
  // metric.
  float get_average_bit_shannon_entropy() const {
    return bit_shannon_aggregator.get_mean();
  }

  // Returns the average spatial entropy according to Shannon's byte entropy
  // metric.
  float get_average_byte_shannon_entropy() const {
    return byte_shannon_aggregator.get_mean();
  }

  // Returns the average spatial entropy according to Shannon's byte entropy
  // metric, with adapted scaling factor for small buffers.
  float get_average_byte_shannon_adapted_entropy() const {
    return byte_shannon_adapted_aggregator.get_mean();
  }

  // Returns the average spatial entropy according to the number of different
  // bytes.
  float get_average_byte_num_different_entropy() const {
    return byte_num_different_aggregator.get_mean();
  }

  // Returns the average spatial entropy according to the number of unique
  // bytes.
  float get_average_byte_num_unique_entropy() const {
    return byte_num_unique_aggregator.get_mean();
  }

  // Returns the average spatial entropy according to the average bit value
  // metric.
  float get_average_bit_average_entropy() const {
    return bit_average_aggregator.get_mean();
  }

  // Returns the average spatial entropy according to the average byte value
  // metric.
  float get_average_byte_average_entropy() const {
    return byte_average_aggregator.get_mean();
  }

  // Add a sample of the spatial entropy of the memory region, starting at the
  // specified address and of a given size.
  void record(const unsigned char *start_address, unsigned int size);

private:
  // Aggregators used to calculate the average spatial entropy.

  // Aggregator for the spatial entropy for different metrics.
  MeanAggregator<float> bit_shannon_aggregator;
  MeanAggregator<float> byte_shannon_aggregator;
  MeanAggregator<float> byte_shannon_adapted_aggregator;
  MeanAggregator<float> byte_num_different_aggregator;
  MeanAggregator<float> byte_num_unique_aggregator;
  MeanAggregator<float> bit_average_aggregator;
  MeanAggregator<float> byte_average_aggregator;
};

#endif
