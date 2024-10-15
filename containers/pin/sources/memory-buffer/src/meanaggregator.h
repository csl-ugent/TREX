#ifndef MEANAGGREGATOR_H
#define MEANAGGREGATOR_H

template <typename T> class MeanAggregator {
public:
  MeanAggregator() : num_samples(0), sum_samples(T(0)) {}

  void add_sample(T sample) {
    sum_samples += sample;
    ++num_samples;
  }

  T get_mean() const { return sum_samples / static_cast<T>(num_samples); }

private:
  // Total number of samples.
  unsigned int num_samples;

  // Sum of all samples.
  T sum_samples;
};

#endif /* end of include guard: MEANAGGREGATOR_H */
