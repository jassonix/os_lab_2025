/********************************************************
 * An example source module to accompany...
 *
 * "Using POSIX Threads: Programming with Pthreads"
 *     by Brad nichols, Dick Buttlar, Jackie Farrell
 *     O'Reilly & Associates, Inc.
 *  Modified by A.Kostin
 ********************************************************
 * mutex.c
 *
 * Simple multi-threaded example with a mutex lock.
 */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

void *do_one_thing(void *);
void *do_another_thing(void *);
void do_wrap_up(int);
int common = 0; /* A shared variable for two threads */
// общая переменная используется обоими потоками и без синхронизации приводит к гонкам
int r1 = 0, r2 = 0, r3 = 0;
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

int main() {
  pthread_t thread1, thread2;

  if (pthread_create(&thread1, NULL, do_one_thing, (void *)&common) != 0) {
    perror("pthread_create");
    exit(1);
  }

  if (pthread_create(&thread2, NULL, do_another_thing, (void *)&common) != 0) {
    perror("pthread_create");
    exit(1);
  }

  if (pthread_join(thread1, NULL) != 0) {
    perror("pthread_join");
    exit(1);
  }

  if (pthread_join(thread2, NULL) != 0) {
    perror("pthread_join");
    exit(1);
  }

  do_wrap_up(common);

  return 0;
}

void *do_one_thing(void *arg) {
  int *pnum_times = (int *)arg;
  int i;
  unsigned long k;
  int work;
  for (i = 0; i < 50; i++) {
#ifdef USE_MUTEX
    pthread_mutex_lock(&mut);
#endif
    // критическая секция защищается мьютексом при наличии макроса use_mutex
    printf("doing one thing\n");
    work = *pnum_times;
    printf("counter = %d\n", work);
    work++; /* increment, but not write */
    for (k = 0; k < 500000; k++)
      ;                 /* long cycle */
    *pnum_times = work; /* write back */
#ifdef USE_MUTEX
    pthread_mutex_unlock(&mut);
#endif
  }
  return NULL;
}

void *do_another_thing(void *arg) {
  int *pnum_times = (int *)arg;
  int i;
  unsigned long k;
  int work;
  for (i = 0; i < 50; i++) {
#ifdef USE_MUTEX
    pthread_mutex_lock(&mut);
#endif
    // здесь второй поток выполняет те же действия, что и первый, создавая состязание без блокировки
    printf("doing another thing\n");
    work = *pnum_times;
    printf("counter = %d\n", work);
    work++; /* increment, but not write */
    for (k = 0; k < 500000; k++)
      ;                 /* long cycle */
    *pnum_times = work; /* write back */
#ifdef USE_MUTEX
    pthread_mutex_unlock(&mut);
#endif
  }
  return NULL;
}

/* при сборке без макроса use_mutex оба потока обновляют общий счётчик без
 * синхронизации, что приводит к потерянным инкрементам и непредсказуемому
 * результату; компиляция с -duse_mutex защищает критические секции мьютексом
 * и гарантирует получение стабильного значения 100 независимо от порядка
 * планирования потоков */

void do_wrap_up(int counter) {
  printf("All done, counter = %d\n", counter);
}
