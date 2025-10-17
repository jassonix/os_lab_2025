#include "find_min_max.h"

#include <limits.h>

struct MinMax GetMinMax(int *array, unsigned int begin, unsigned int end) {
  struct MinMax min_max;
  // Проверка на пустой диапазон
    if (begin >= end) {
        min_max.min = INT_MAX; 
        min_max.max = INT_MIN;
        return min_max;
    }

    // Инициализация min и max первым элементом диапазона
    min_max.min = array[begin];
    min_max.max = array[begin];

    // Итерация по оставшимся элементам диапазона [begin + 1, end)
    for (unsigned int i = begin + 1; i < end; ++i) {
        int current = array[i];

        if (current < min_max.min) {
            min_max.min = current;
        }
        if (current > min_max.max) {
            min_max.max = current;
        }
    }

  return min_max;
}