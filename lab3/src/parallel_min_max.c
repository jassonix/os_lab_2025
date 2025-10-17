#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>

#include "find_min_max.h"
#include "utils.h"

// Константы для синхронизации через файлы
#define FILE_SYNC_BASE "minmax_result_"
#define FILE_SYNC_EXT ".txt"

// Структура для передачи данных в потоках
struct ThreadData {
    int *array;
    unsigned int begin;
    unsigned int end;
    int index; // Индекс процесса/потока (0 или 1)
    struct MinMax result;
};

// Функция для записи результата в файл
void write_result_to_file(int index, struct MinMax result) {
    char filename[64];
    sprintf(filename, "%s%d%s", FILE_SYNC_BASE, index, FILE_SYNC_EXT);
    FILE *file = fopen(filename, "w");
    if (file) {
        fprintf(file, "%d %d", result.min, result.max);
        fclose(file);
    } else {
        perror("Error opening file for writing");
    }
}

// Функция для чтения результата из файла
int read_result_from_file(int index, struct MinMax *result) {
    char filename[64];
    sprintf(filename, "%s%d%s", FILE_SYNC_BASE, index, FILE_SYNC_EXT);
    FILE *file = fopen(filename, "r");
    if (file) {
        if (fscanf(file, "%d %d", &result->min, &result->max) == 2) {
            fclose(file);
            // Удаляем файл после чтения
            remove(filename); 
            return 0; // Успех
        }
        fclose(file);
        return -1; // Ошибка чтения
    }
    return -1; // Ошибка открытия
}


int main(int argc, char **argv) {
    if (argc != 4) {
        printf("Usage: %s seed arraysize sync_type\n", argv[0]);
        printf("sync_type: pipe or by_files\n");
        return 1;
    }

    int seed = atoi(argv[1]);
    if (seed <= 0) {
        printf("seed is a positive number\n");
        return 1;
    }

    int array_size = atoi(argv[2]);
    if (array_size <= 0) {
        printf("array_size is a positive number\n");
        return 1;
    }

    const char *sync_type = argv[3];
    int use_files = (strcmp(sync_type, "by_files") == 0);

    // Каналы для синхронизации (если не используем файлы)
    int pipe_fds[2][2]; // pipe_fds[i][0] - чтение, pipe_fds[i][1] - запись для i-го процесса
    if (!use_files) {
        if (pipe(pipe_fds[0]) == -1 || pipe(pipe_fds[1]) == -1) {
            perror("pipe failed");
            return 1;
        }
    }

    int *array = malloc(array_size * sizeof(int));
    if (!array) {
        perror("malloc failed");
        return 1;
    }
    GenerateArray(array, array_size, seed);

    unsigned int middle = array_size / 2;
    pid_t pid1, pid2;
    struct MinMax results[2];

    // ----------------------------------------------------
    // Создание и выполнение первого дочернего процесса
    // ----------------------------------------------------
    pid1 = fork();

    if (pid1 < 0) {
        perror("fork 1 failed");
        free(array);
        return 1;
    } 
    
    if (pid1 == 0) {
        // Дочерний процесс 1
        struct MinMax min_max = GetMinMax(array, 0, middle);
        
        if (!use_files) {
            // Закрываем чтение, пишем в канал 1
            close(pipe_fds[0][0]); 
            write(pipe_fds[0][1], &min_max, sizeof(struct MinMax));
            close(pipe_fds[0][1]);
        } else {
            write_result_to_file(0, min_max);
        }
        
        free(array); // Освобождаем память в дочернем процессе
        exit(0); // Завершаем дочерний процесс
    }

    // ----------------------------------------------------
    // Создание и выполнение второго дочернего процесса
    // ----------------------------------------------------
    pid2 = fork();

    if (pid2 < 0) {
        perror("fork 2 failed");
        // Чистим ресурсы первого процесса (если он еще не завершился)
        if (pid1 > 0) waitpid(pid1, NULL, 0); 
        free(array);
        return 1;
    } 
    
    if (pid2 == 0) {
        // Дочерний процесс 2
        struct MinMax min_max = GetMinMax(array, middle, array_size);

        if (!use_files) {
            // Закрываем чтение, пишем в канал 2
            close(pipe_fds[1][0]); 
            write(pipe_fds[1][1], &min_max, sizeof(struct MinMax));
            close(pipe_fds[1][1]);
        } else {
            write_result_to_file(1, min_max);
        }

        free(array); // Освобождаем память в дочернем процессе
        exit(0); // Завершаем дочерний процесс
    }

    // ----------------------------------------------------
    // Родительский процесс: ожидание и сбор результатов
    // ----------------------------------------------------

    // Ожидание завершения дочерних процессов
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    // Сбор результатов
    if (!use_files) {
        // Сбор результатов через pipe
        close(pipe_fds[0][1]); // Закрываем запись 1
        read(pipe_fds[0][0], &results[0], sizeof(struct MinMax));
        close(pipe_fds[0][0]); // Закрываем чтение 1

        close(pipe_fds[1][1]); // Закрываем запись 2
        read(pipe_fds[1][0], &results[1], sizeof(struct MinMax));
        close(pipe_fds[1][0]); // Закрываем чтение 2

    } else {
        // Сбор результатов через файлы
        if (read_result_from_file(0, &results[0]) != 0 || 
            read_result_from_file(1, &results[1]) != 0) {
            
            fprintf(stderr, "Error reading results from files.\n");
            free(array);
            return 1;
        }
    }
    
    // Определение общего минимума и максимума
    struct MinMax final_min_max;
    final_min_max.min = (results[0].min < results[1].min) ? results[0].min : results[1].min;
    final_min_max.max = (results[0].max > results[1].max) ? results[0].max : results[1].max;

    free(array);

    printf("min: %d\n", final_min_max.min);
    printf("max: %d\n", final_min_max.max);

    return 0;
}