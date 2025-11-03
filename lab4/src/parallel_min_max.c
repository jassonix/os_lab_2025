#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

// Используем -1 для обозначения "не завершен", 0 для "убит", 1 для "успешно завершен"
#define PROCESS_STATUS_LIVE -1
#define PROCESS_STATUS_KILLED 0
#define PROCESS_STATUS_FINISHED 1

static volatile sig_atomic_t timeout_occurred = 0;

void alarm_handler(int signum) {
    (void)signum;
    timeout_occurred = 1;
}

int main(int argc, char **argv) {
    int seed = -1;
    int array_size = -1;
    int pnum = -1;
    bool with_files = false;
    int timeout = 0;

    // --- Обработка аргументов ---
    while (1) {
        static struct option options[] = {
            {"seed", required_argument, 0, 0},
            {"array_size", required_argument, 0, 0},
            {"pnum", required_argument, 0, 0},
            {"by_files", no_argument, 0, 'f'},
            {"timeout", required_argument, 0, 0},
            {0, 0, 0, 0}
        };

        int option_index = 0;
        int c = getopt_long(argc, argv, "f", options, &option_index);

        if (c == -1) break;

        switch (c) {
            case 0:
                if (strcmp(options[option_index].name, "seed") == 0) {
                    seed = atoi(optarg);
                    if (seed <= 0) { fprintf(stderr, "seed must be positive\n"); return 1; }
                } else if (strcmp(options[option_index].name, "array_size") == 0) {
                    array_size = atoi(optarg);
                    if (array_size <= 0) { fprintf(stderr, "array_size must be positive\n"); return 1; }
                } else if (strcmp(options[option_index].name, "pnum") == 0) {
                    pnum = atoi(optarg);
                    if (pnum <= 0) { fprintf(stderr, "pnum must be positive\n"); return 1; }
                } else if (strcmp(options[option_index].name, "timeout") == 0) {
                    timeout = atoi(optarg);
                    if (timeout <= 0) { fprintf(stderr, "timeout must be positive\n"); return 1; }
                }
                break;
            case 'f':
                with_files = true;
                break;
            case '?':
            default:
                fprintf(stderr, "Invalid argument\n");
                return 1;
        }
    }

    if (seed == -1 || array_size== -1 || pnum == -1) {
        fprintf(stderr, "Usage: %s --seed <num> --array_size <num> --pnum <num> [--by_files] [--timeout <sec>]\n", argv[0]);
        return 1;
    }
    
    // Ограничение pnum до array_size для логики деления массива
    if (pnum > array_size) {
        pnum = array_size;
    }

    // --- Инициализация данных ---
    int *array = malloc(sizeof(int) * array_size);
    if (!array) { perror("malloc array"); return 1; }
    GenerateArray(array, array_size, seed);

    pid_t *child_pids = malloc(sizeof(pid_t) * pnum);
    int *process_status = malloc(sizeof(int) * pnum); // Для отслеживания успешного завершения
    if (!child_pids || !process_status) { perror("malloc pids/status"); free(array); free(child_pids); free(process_status); return 1; }
    for(int i = 0; i < pnum; i++) process_status[i] = PROCESS_STATUS_LIVE;

    int *pipe_fds = NULL;
    if (!with_files) {
        pipe_fds = malloc(2 * pnum * sizeof(int));
        if (!pipe_fds) { perror("malloc pipe_fds"); free(array); free(child_pids); free(process_status); return 1; }
        for (int i = 0; i < pnum; i++) {
            if (pipe(pipe_fds + 2 * i) < 0) {
                perror("pipe");
                // Очистка и выход
                free(array); free(child_pids); free(process_status); free(pipe_fds); 
                return 1;
            }
        }
    }

    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    int part_size = array_size / pnum;
    int remainder = array_size % pnum;
    // --- Запуск дочерних процессов ---
    int active_child_processes = 0;
    for (int i = 0; i < pnum; i++) {
        int current_part_size = part_size + (i < remainder ? 1 : 0);
        int start_index = i * part_size + (i < remainder ? i : remainder);
        int end_index = start_index + current_part_size;

        if (current_part_size == 0) continue; // Пропускаем, если pnum > array_size

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            for (int j = 0; j < active_child_processes; j++) {
                kill(child_pids[j], SIGTERM);
            }
            free(array); free(child_pids); free(process_status); if (!with_files) free(pipe_fds);
            return 1;
        }

        if (pid == 0) {
            // Child
            struct MinMax mm = GetMinMax(array, start_index, end_index);
            
            if (!with_files) {
                for (int j = 0; j < pnum; j++) {
                    if (j != i) {
                        close(pipe_fds[2 * j]); // Закрыть все неиспользуемые read ends
                        close(pipe_fds[2 * j + 1]); // Закрыть все неиспользуемые write ends
                    }
                }
                close(pipe_fds[2 * i]); // Закрыть read end родителя
                // Пишем результат
                if (write(pipe_fds[2 * i + 1], &mm.min, sizeof(int)) != sizeof(int) ||
                    write(pipe_fds[2 * i + 1], &mm.max, sizeof(int)) != sizeof(int)) {
                    perror("write");
                    exit(1); // Выход с ошибкой
                }
                close(pipe_fds[2 * i + 1]); // Закрыть write end
            } else {
                char fname[256];
                snprintf(fname, sizeof(fname), "minmax_%d.txt", (int)getpid());
                FILE *f = fopen(fname, "w");
                if (f) {
                    fprintf(f, "%d %d", mm.min, mm.max);
                    fclose(f);
                } else {
                    perror("fopen");
                    exit(1);
                }
            }

            free(array); free(child_pids); free(process_status); if (!with_files) free(pipe_fds);
            exit(0);
        }

        // Parent
        child_pids[i] = pid;
        active_child_processes++;
        if (!with_files) {
            close(pipe_fds[2 * i + 1]); // Закрыть пишущий конец пайпа в родителе
        }
    }
    
    // --- Ожидание и таймаут ---
    if (timeout > 0) {
        signal(SIGALRM, alarm_handler);
        alarm(timeout);
    }

    while (active_child_processes > 0) {
        if (timeout > 0 && timeout_occurred) {
            // Таймаут сработал - Убить всех активных детей
            for (int i = 0; i < pnum; i++) {
                if (process_status[i] == PROCESS_STATUS_LIVE) {
                    kill(child_pids[i], SIGKILL);
                    process_status[i] = PROCESS_STATUS_KILLED;
                    active_child_processes--; // уменьшаем счетчик сразу после kill
                }
            }
            break; // Выход из основного цикла ожидания
        }

        int status;
        pid_t finished = waitpid(-1, &status, WNOHANG);

        if (finished > 0) {
            // active_child_processes--; // Уменьшаем только в случае успешного завершения
            for (int i = 0; i < pnum; i++) {
                if (child_pids[i] == finished) {
                    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                        process_status[i] = PROCESS_STATUS_FINISHED;
                        active_child_processes--;
                    } else if (process_status[i] == PROCESS_STATUS_LIVE) {
                        // Ребенок завершился с ошибкой или сигналом, но не был помечен как убитый
                        process_status[i] = PROCESS_STATUS_KILLED; // Считаем его 'убитым' для пропуска сбора
                        active_child_processes--;
                    }
                    break;
                }
            }
        } else if (finished == 0) {
            usleep(1000); // 1 ms
        } else {
            if (errno != ECHILD) {
                perror("waitpid");
            }
            break; 
        }
    }
    
    // Очистка таймаута
    if (timeout > 0) {
        alarm(0); // Отменяем таймер, если он еще активен
        signal(SIGALRM, SIG_DFL); // Возвращаем стандартную обработку
    }

    // --- Сбор результатов ---
    struct MinMax global = { .min = INT_MAX, .max = INT_MIN };
    bool has_result = false;

    for (int i = 0; i < pnum; i++) {
        if (process_status[i] == PROCESS_STATUS_FINISHED) {
            int min = INT_MAX, max = INT_MIN;

            if (with_files) {
                char fname[256];
                snprintf(fname, sizeof(fname), "minmax_%d.txt", (int)child_pids[i]);
                FILE *f = fopen(fname, "r");
                if (f) {
                    if (fscanf(f, "%d %d", &min, &max) == 2) {
                        has_result = true;
                    }
                    fclose(f);
                    unlink(fname);
                }
            } else {
                ssize_t min_read = read(pipe_fds[2 * i], &min, sizeof(int));
                ssize_t max_read = read(pipe_fds[2 * i], &max, sizeof(int));
                
                if (min_read == sizeof(int) && max_read == sizeof(int)) {
                    has_result = true;
                }
                close(pipe_fds[2 * i]); // Закрыть читающий конец пайпа
            }

            if (has_result) {
                if (min < global.min) global.min = min;
                if (max > global.max) global.max = max;
            }
        } else if (!with_files) {
            // Если процесс был убит или не завершился успешно, закрываем читающий конец пайпа, 
            // если он не был закрыт ранее (он закрывается только в цикле сбора)
             close(pipe_fds[2 * i]);
        }
    }

    // --- Очистка и вывод ---
    free(array);
    free(child_pids);
    free(process_status);
    if (!with_files) {
        free(pipe_fds);
    }

    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0 +
                        (end_time.tv_usec - start_time.tv_usec) / 1000.0;

    printf("Min: %d\n", global.min);
    printf("Max: %d\n", global.max);
    printf("Elapsed time: %fms\n", elapsed_ms);
    fflush(stdout);

    return 0;
}