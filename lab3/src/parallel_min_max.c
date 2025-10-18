#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include "utils.h"
#include "find_min_max.h"

#define MAX_PROCESSES 100

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <array_size> <seed> <num_processes> <pipe|files>\n", argv[0]);
        return 1;
    }
    
    // Парсинг аргументов
    unsigned int array_size = atoi(argv[1]);
    unsigned int seed = atoi(argv[2]);
    int num_processes = atoi(argv[3]);
    int use_pipe = (strcmp(argv[4], "pipe") == 0);
    
    if (num_processes <= 0 || num_processes > MAX_PROCESSES) {
        printf("Number of processes must be between 1 and %d\n", MAX_PROCESSES);
        return 1;
    }
    
    // Выделение памяти для массива
    int *array = (int*)malloc(sizeof(int) * array_size);
    if (array == NULL) {
        printf("Memory allocation failed\n");
        return 1;
    }
    
    // Генерация массива
    GenerateArray(array, array_size, seed);
    
    // Создание pipe'ов если используется pipe
    int pipe_fds[MAX_PROCESSES][2];
    if (use_pipe) {
        for (int i = 0; i < num_processes; i++) {
            if (pipe(pipe_fds[i]) == -1) {
                perror("pipe");
                free(array);
                return 1;
            }
        }
    }
    
    // Замер времени начала
    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    
    // Создание дочерних процессов
    for (int i = 0; i < num_processes; i++) {
        pid_t pid = fork();
        
        if (pid == 0) {
            // Дочерний процесс
            if (use_pipe) {
                close(pipe_fds[i][0]); // Закрываем чтение в дочернем процессе
            }
            
            // Вычисляем диапазон для текущего процесса
            int chunk_size = array_size / num_processes;
            int begin = i * chunk_size;
            int end = (i == num_processes - 1) ? array_size : (i + 1) * chunk_size;
            
            // Находим min и max в своем диапазоне
            struct MinMax local_min_max = GetMinMax(array, begin, end);
            
            if (use_pipe) {
                // Передача через pipe
                write(pipe_fds[i][1], &local_min_max, sizeof(struct MinMax));
                close(pipe_fds[i][1]);
            } else {
                // Передача через файлы
                char filename[50];
                snprintf(filename, sizeof(filename), "min_max_%d.txt", i);
                
                FILE *file = fopen(filename, "w");
                if (file) {
                    fprintf(file, "%d %d", local_min_max.min, local_min_max.max);
                    fclose(file);
                }
            }
            
            free(array);
            exit(0);
        } else if (pid < 0) {
            printf("Fork failed\n");
            free(array);
            return 1;
        }
    }
    
    // Родительский процесс
    if (use_pipe) {
        for (int i = 0; i < num_processes; i++) {
            close(pipe_fds[i][1]); // Закрываем запись в родительском процессе
        }
    }
    
    // Ожидание завершения всех дочерних процессов
    for (int i = 0; i < num_processes; i++) {
        wait(NULL);
    }
    
    // Сбор результатов
    struct MinMax partial_results[MAX_PROCESSES];
    
    if (use_pipe) {
        // Чтение из pipe
        for (int i = 0; i < num_processes; i++) {
            read(pipe_fds[i][0], &partial_results[i], sizeof(struct MinMax));
            close(pipe_fds[i][0]);
        }
    } else {
        // Чтение из файлов
        for (int i = 0; i < num_processes; i++) {
            char filename[50];
            snprintf(filename, sizeof(filename), "min_max_%d.txt", i);
            
            FILE *file = fopen(filename, "r");
            if (file) {
                fscanf(file, "%d %d", &partial_results[i].min, &partial_results[i].max);
                fclose(file);
                remove(filename); // Удаляем временный файл
            }
        }
    }
    
    // Объединение результатов
    struct MinMax final_result;
    final_result.min = partial_results[0].min;
    final_result.max = partial_results[0].max;
    
    for (int i = 1; i < num_processes; i++) {
        if (partial_results[i].min < final_result.min) {
            final_result.min = partial_results[i].min;
        }
        if (partial_results[i].max > final_result.max) {
            final_result.max = partial_results[i].max;
        }
    }
    
    // Замер времени окончания
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    
    // Вычисление времени выполнения
    double execution_time = (end_time.tv_sec - start_time.tv_sec) + 
                           (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    
    // Вывод результатов
    printf("Min: %d\n", final_result.min);
    printf("Max: %d\n", final_result.max);
    printf("Execution time: %.6f seconds\n", execution_time);
    
    // Очистка
    free(array);
    
    return 0;
}