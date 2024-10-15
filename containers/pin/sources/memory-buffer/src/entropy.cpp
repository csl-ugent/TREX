#include <math.h>

#include "entropy.h"

float binary_entropy(float p) {
  float q = 1 - p;

  float p_part = (abs(p) < 1e-12 ? 0 : p * log2(p));
  float q_part = (abs(q) < 1e-12 ? 0 : q * log2(q));

  // Return abs so that -0 becomes +0.
  return abs(-p_part - q_part);
}
