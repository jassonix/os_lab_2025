#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

struct thread_args {
  unsigned long long start;
  unsigned long long end;
  unsigned long long mod;
};

static pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned long long global_result = 1;
// общий результат под защитой мьютекса накапливает произведение остатков

static void *thread_compute(void *arg) {
  struct thread_args *data = (struct thread_args *)arg;
  unsigned long long local = 1;
  // локальный результат позволяет избежать частых блокировок

  if (data->mod == 1) {
    local = 0;
  } else {
    for (unsigned long long i = data->start; i <= data->end; ++i) {
      if (i == 0) {
        continue;
      }
      local = (local * (i % data->mod)) % data->mod;
      if (local == 0) {
        break;
      }
    }
  }

  pthread_mutex_lock(&result_mutex);
  if (global_result == 0) {
    /* nothing to do */
  } else if (data->mod == 1) {
    global_result = 0;
  } else {
    global_result = (global_result * (local % data->mod)) % data->mod;
  }
  pthread_mutex_unlock(&result_mutex);
  // после обновления глобального значения поток завершает работу

  return NULL;
}

static void usage(const char *progname) {
  fprintf(stderr,
          "Usage: %s -k <number> --pnum=<threads> --mod=<modulus>\n",
          progname);
}

int main(int argc, char **argv) {
  unsigned long long k = 0;
  unsigned long long mod = 0;
  int pnum = 0;

  while (1) {
    static struct option options[] = {{"k", required_argument, 0, 'k'},
                                      {"pnum", required_argument, 0, 'p'},
                                      {"mod", required_argument, 0, 'm'},
                                      {0, 0, 0, 0}};
    int option_index = 0;
    int c = getopt_long(argc, argv, "k:", options, &option_index);
    if (c == -1)
      break;

    switch (c) {
    case 'k':
      k = strtoull(optarg, NULL, 10);
      break;
    case 'p':
      pnum = atoi(optarg);
      break;
    case 'm':
      mod = strtoull(optarg, NULL, 10);
      break;
    default:
      usage(argv[0]);
      return EXIT_FAILURE;
    }
  }

  if (k == 0) {
    fprintf(stderr, "Parameter -k is required and must be greater than 0.\n");
    return EXIT_FAILURE;
  }

  if (pnum <= 0) {
    fprintf(stderr, "Parameter --pnum must be a positive integer.\n");
    return EXIT_FAILURE;
  }

  if (mod == 0) {
    fprintf(stderr, "Parameter --mod must be greater than 0.\n");
    return EXIT_FAILURE;
  }

  if (mod == 1) {
    printf("0\n");
    return EXIT_SUCCESS;
  }

  pthread_t *threads = calloc(pnum, sizeof(pthread_t));
  struct thread_args *args = calloc(pnum, sizeof(struct thread_args));
  if (threads == NULL || args == NULL) {
    perror("calloc");
    free(threads);
    free(args);
    return EXIT_FAILURE;
  }

  unsigned long long numbers_per_thread = k / pnum;
  unsigned long long remainder = k % pnum;
  unsigned long long current = 1;
  // распределяем диапазоны между потоками так, чтобы первые получали на одну итерацию больше

  for (int i = 0; i < pnum; ++i) {
    unsigned long long start = current;
    unsigned long long length =
        numbers_per_thread + ((unsigned long long)i < remainder ? 1 : 0);
    unsigned long long end = 0;

    if (length > 0) {
      end = start + length - 1;
      current = end + 1;
    } else {
      start = 0;
      end = 0;
    }

    args[i].start = start;
    args[i].end = end;
    args[i].mod = mod;
    // каждый поток получает свой поддиапазон и общий модуль

    if (pthread_create(&threads[i], NULL, thread_compute, &args[i]) != 0) {
      perror("pthread_create");
      pnum = i;
      goto cleanup_join;
    }
  }

cleanup_join:
  for (int i = 0; i < pnum; ++i) {
    pthread_join(threads[i], NULL);
  }

  printf("%llu\n", global_result % mod);

  free(threads);
  free(args);
  return EXIT_SUCCESS;
}
