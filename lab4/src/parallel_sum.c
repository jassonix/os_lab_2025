#include <getopt.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include "sum_lib.h"
#include "utils.h"

struct SumArgs {
  const int *array;
  size_t begin;
  size_t end;
};

static void PrintUsage(const char *prog_name) {
  printf("Usage: %s --threads_num \"num\" --seed \"num\" --array_size \"num\"\n",
         prog_name);
}

static int ParseArguments(int argc, char **argv, uint32_t *threads_num,
                          uint32_t *seed, uint32_t *array_size) {
  int option_index = 0;
  optind = 1;

  static struct option options[] = {{"threads_num", required_argument, 0, 0},
                                    {"seed", required_argument, 0, 0},
                                    {"array_size", required_argument, 0, 0},
                                    {0, 0, 0, 0}};

  while (1) {
    int c = getopt_long(argc, argv, "", options, &option_index);
    if (c == -1) break;

    if (c == 0) {
      switch (option_index) {
        case 0:
          int parsed_threads = atoi(optarg);
          if (parsed_threads <= 0) {
            printf("threads_num must be a positive number\n");
            return -1;
          }
          *threads_num = (uint32_t)parsed_threads;
          break;
        case 1:
          int parsed_seed = atoi(optarg);
          if (parsed_seed <= 0) {
            printf("seed must be a positive number\n");
            return -1;
          }
          *seed = (uint32_t)parsed_seed;
          break;
        case 2:
          int parsed_size = atoi(optarg);
          if (parsed_size <= 0) {
            printf("array_size must be a positive number\n");
            return -1;
          }
          *array_size = (uint32_t)parsed_size;
          break;
        default:
          break;
      }
    } else if (c == '?') {
      return -1;
    } else {
      printf("Unexpected option\n");
      return -1;
    }
  }

  if (*threads_num == 0 || *seed == 0 || *array_size == 0) {
    return -1;
  }

  if (optind < argc) {
    printf("Unexpected positional arguments\n");
    return -1;
  }

  return 0;
}

static void *ThreadSum(void *args) {
  struct SumArgs *sum_args = (struct SumArgs *)args;
  long long *result = (long long *)malloc(sizeof(long long));
  if (result == NULL) {
    perror("malloc");
    pthread_exit(NULL);
  }
  *result = SumRange(sum_args->array, sum_args->begin, sum_args->end);
  return result;
}

int main(int argc, char **argv) {
  uint32_t threads_num = 0;
  uint32_t array_size = 0;
  uint32_t seed = 0;

  if (ParseArguments(argc, argv, &threads_num, &seed, &array_size) != 0) {
    PrintUsage(argv[0]);
    return 1;
  }

  int *array = (int *)malloc(sizeof(int) * array_size);
  if (array == NULL) {
    perror("malloc");
    return 1;
  }

  GenerateArray(array, array_size, seed);

  pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t) * threads_num);
  if (threads == NULL) {
    perror("malloc");
    free(array);
    return 1;
  }

  struct SumArgs *args = (struct SumArgs *)malloc(sizeof(struct SumArgs) * threads_num);
  if (args == NULL) {
    perror("malloc");
    free(threads);
    free(array);
    return 1;
  }

  size_t base_chunk = array_size / threads_num;
  size_t remainder = array_size % threads_num;
  size_t offset = 0;

  for (uint32_t i = 0; i < threads_num; ++i) {
    size_t chunk_size = base_chunk + (i < remainder ? 1 : 0);
    args[i].array = array;
    args[i].begin = offset;
    args[i].end = offset + chunk_size;
    offset += chunk_size;
  }

  struct timeval start_time = {0};
  struct timeval finish_time = {0};
  gettimeofday(&start_time, NULL);

  for (uint32_t i = 0; i < threads_num; ++i) {
    if (pthread_create(&threads[i], NULL, ThreadSum, (void *)&args[i]) != 0) {
      perror("pthread_create");
      threads_num = i;
      break;
    }
  }

  long long total_sum = 0;
  for (uint32_t i = 0; i < threads_num; ++i) {
    void *result = NULL;
    if (pthread_join(threads[i], &result) != 0) {
      perror("pthread_join");
      continue;
    }
    if (result != NULL) {
      total_sum += *(long long *)result;
      free(result);
    }
  }

  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) +
                        (finish_time.tv_usec - start_time.tv_usec) / 1000000.0;

  printf("Total: %lld\n", total_sum);
  printf("Elapsed time: %f seconds\n", elapsed_time);

  free(args);
  free(threads);
  free(array);
  return 0;
}
