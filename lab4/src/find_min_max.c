#include "find_min_max.h"
#include <limits.h>
#include <stddef.h> // Для size_t

// Индексы и цикл теперь используют size_t
struct MinMax GetMinMax(int *array, size_t begin, size_t end) {
  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  if (begin >= end) {
    min_max.min = min_max.max = 0;
    return min_max;
  }

  min_max.min = array[begin];
  min_max.max = array[begin];


  for (size_t i = begin + 1; i < end; i++) {
    if (array[i] < min_max.min) {
      min_max.min = array[i];
    }
    if (array[i] > min_max.max) {
      min_max.max = array[i];
    }
  }

  return min_max;
}