#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static pthread_mutex_t mutex_a = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_b = PTHREAD_MUTEX_INITIALIZER;
// специально меняем порядок захвата мьютексов, чтобы получить взаимную блокировку

static void *lock_ab(void *arg) {
  (void)arg;
  printf("Thread 1 locking mutex A\n");
  pthread_mutex_lock(&mutex_a);
  sleep(1);
  printf("Thread 1 waiting for mutex B\n");
  pthread_mutex_lock(&mutex_b);
  printf("Thread 1 acquired both mutexes\n");
  pthread_mutex_unlock(&mutex_b);
  pthread_mutex_unlock(&mutex_a);
  return NULL;
}

static void *lock_ba(void *arg) {
  (void)arg;
  printf("Thread 2 locking mutex B\n");
  pthread_mutex_lock(&mutex_b);
  sleep(1);
  printf("Thread 2 waiting for mutex A\n");
  pthread_mutex_lock(&mutex_a);
  printf("Thread 2 acquired both mutexes\n");
  pthread_mutex_unlock(&mutex_a);
  pthread_mutex_unlock(&mutex_b);
  return NULL;
}

int main(void) {
  pthread_t t1, t2;

  if (pthread_create(&t1, NULL, lock_ab, NULL) != 0) {
    perror("pthread_create");
    return EXIT_FAILURE;
  }

  if (pthread_create(&t2, NULL, lock_ba, NULL) != 0) {
    perror("pthread_create");
    return EXIT_FAILURE;
  }

  pthread_join(t1, NULL);
  pthread_join(t2, NULL);

  return EXIT_SUCCESS;
}
