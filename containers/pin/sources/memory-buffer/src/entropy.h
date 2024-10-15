#ifndef ENTROPY_H
#define ENTROPY_H

#include <bitset>
#include <climits>
#include <numeric>

#include "pin.H"

// Calculates the hamming weight, i.e. the number of 1 bits, of a value.
template <typename T> unsigned int hamming_weight(T value) {
  return std::bitset<sizeof(T) * CHAR_BIT>(value).count();
}

// Calculates the binary entropy function evaluated at p. p must be between 0
// and 1.
float binary_entropy(float p);

#endif
