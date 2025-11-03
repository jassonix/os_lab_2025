#include "sum_lib.h"

long long SumRange(const int *array, size_t begin, size_t end) {
  if (array == NULL || begin >= end) {
    return 0;
  }

  long long sum = 0;
  for (size_t i = begin; i < end; ++i) {
    sum += array[i];
  }

  return sum;
}
