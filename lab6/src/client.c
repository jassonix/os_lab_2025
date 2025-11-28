#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stdint.h>
#include <inttypes.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "common.h"

struct Server {
  char ip[255];
  int port;
};

struct ThreadArgs {
  struct Server server;
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
  uint64_t result;
};

void *ClientRoutine(void *args) {
  struct ThreadArgs *targs = (struct ThreadArgs *)args;
  struct hostent *hostname = gethostbyname(targs->server.ip);
  if (hostname == NULL) {
    fprintf(stderr, "gethostbyname failed with %s\n", targs->server.ip);
    pthread_exit(NULL);
  }

  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_port = htons(targs->server.port);
  server.sin_addr.s_addr = *((unsigned long *)hostname->h_addr);

  int sck = socket(AF_INET, SOCK_STREAM, 0);
  if (sck < 0) {
    fprintf(stderr, "Socket creation failed!\n");
    pthread_exit(NULL);
  }

  if (connect(sck, (struct sockaddr *)&server, sizeof(server)) < 0) {
    fprintf(stderr, "Connection failed to %s:%d\n", targs->server.ip,
            targs->server.port);
    close(sck);
    pthread_exit(NULL);
  }

  char task[sizeof(uint64_t) * 3];
  memcpy(task, &targs->begin, sizeof(uint64_t));
  memcpy(task + sizeof(uint64_t), &targs->end, sizeof(uint64_t));
  memcpy(task + 2 * sizeof(uint64_t), &targs->mod, sizeof(uint64_t));

  if (send(sck, task, sizeof(task), 0) < 0) {
    fprintf(stderr, "Send failed to %s:%d\n", targs->server.ip, targs->server.port);
    close(sck);
    pthread_exit(NULL);
  }

  char response[sizeof(uint64_t)];
  if (recv(sck, response, sizeof(response), 0) < 0) {
    fprintf(stderr, "Receive failed from %s:%d\n", targs->server.ip,
            targs->server.port);
    close(sck);
    pthread_exit(NULL);
  }

  memcpy(&targs->result, response, sizeof(uint64_t));
  close(sck);
  return NULL;
}

int main(int argc, char **argv) {
  uint64_t k = 0;
  uint64_t mod = 0;
  bool k_set = false;
  bool mod_set = false;
  char servers[255] = {'\0'}; // 255 is enough for a reasonably long path

  while (true) {
    static struct option options[] = {{"k", required_argument, 0, 0},
                                      {"mod", required_argument, 0, 0},
                                      {"servers", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1)
      break;

    switch (c) {
    case 0: {
      switch (option_index) {
      case 0:
        if (!ConvertStringToUI64(optarg, &k)) {
          fprintf(stderr, "Invalid k value\n");
          return 1;
        }
        k_set = true;
        break;
      case 1:
        if (!ConvertStringToUI64(optarg, &mod)) {
          fprintf(stderr, "Invalid mod value\n");
          return 1;
        }
        mod_set = true;
        break;
      case 2:
        memcpy(servers, optarg, strlen(optarg));
        break;
      default:
        printf("Index %d is out of options\n", option_index);
      }
    } break;

    case '?':
      printf("Arguments error\n");
      break;
    default:
      fprintf(stderr, "getopt returned character code 0%o?\n", c);
    }
  }

  if (!k_set || !mod_set || !strlen(servers)) {
    fprintf(stderr, "Using: %s --k 1000 --mod 5 --servers /path/to/file\n",
            argv[0]);
    return 1;
  }

  FILE *file = fopen(servers, "r");
  if (file == NULL) {
    fprintf(stderr, "Can't open file %s\n", servers);
    return 1;
  }

  size_t servers_num = 0;
  size_t capacity = 4;
  struct Server *to = malloc(sizeof(struct Server) * capacity);
  char *line = NULL;
  size_t len = 0;

  while (getline(&line, &len, file) != -1) {
    if (strlen(line) == 0)
      continue;
    if (line[strlen(line) - 1] == '\n')
      line[strlen(line) - 1] = '\0';

    if (servers_num == capacity) {
      capacity *= 2;
      to = realloc(to, sizeof(struct Server) * capacity);
    }

    if (sscanf(line, "%254[^:]:%d", to[servers_num].ip, &to[servers_num].port) ==
        2) {
      servers_num++;
    }
  }

  free(line);
  fclose(file);

  if (servers_num == 0) {
    fprintf(stderr, "No servers found in file %s\n", servers);
    free(to);
    return 1;
  }

  pthread_t *threads = malloc(sizeof(pthread_t) * servers_num);
  struct ThreadArgs *targs = malloc(sizeof(struct ThreadArgs) * servers_num);
  bool *thread_created = calloc(servers_num, sizeof(bool));

  uint64_t chunk = k / servers_num;
  uint64_t remainder = k % servers_num;
  uint64_t current = 1;

  for (size_t i = 0; i < servers_num; i++) {
    uint64_t part = chunk + (i < remainder ? 1 : 0);
    targs[i].begin = part ? current : 1;
    targs[i].end = part ? current + part - 1 : 0;
    targs[i].mod = mod;
    targs[i].server = to[i];
    targs[i].result = 1;
    current += part;

    if (part > 0) {
      if (pthread_create(&threads[i], NULL, ClientRoutine, &targs[i])) {
        fprintf(stderr, "Error: pthread_create failed!\n");
        free(to);
        free(threads);
        free(targs);
        free(thread_created);
        return 1;
      }
      thread_created[i] = true;
    }
  }

  uint64_t answer = 1;
  for (size_t i = 0; i < servers_num; i++) {
    if (thread_created[i])
      pthread_join(threads[i], NULL);
    answer = MultModulo(answer, targs[i].result, mod);
  }

  printf("answer: %" PRIu64 "\n", answer);

  free(threads);
  free(targs);
  free(thread_created);
  free(to);

  return 0;
}
