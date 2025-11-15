#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h> 
#include <string.h> 
#include <stddef.h> 
#include "find_min_max.h"

// --- Прототипы функций для I/O ---
int read_array_from_file(const char *filename, int **array, size_t *array_size_out);
int write_result_to_file(const char *filename, struct MinMax result);

// --- Структура для передачи данных в поток ---
struct ThreadData {
  pthread_t thread;
  int thread_id;
  int *array;
  size_t begin;
  size_t end;
  struct MinMax result;
};

// --- Функция-обработчик потока ---
void *find_min_max_thread(void *arg) {
  struct ThreadData *data = (struct ThreadData *)arg;
  data->result = GetMinMax(data->array, data->begin, data->end);
  return NULL;
}

// --- ОСНОВНАЯ ФУНКЦИЯ main ---
int main(int argc, char *argv[]) {
  if (argc < 4) {
    fprintf(stderr, "Использование:\n");
    fprintf(stderr, "  Генерация/Pipe: %s pipe <seed> <размер_массива> <число_потоков>\n", argv[0]);
    fprintf(stderr, "  Файловый ввод/вывод: %s files <входной_файл> <выходной_файл> <число_потоков>\n", argv[0]);
    return 1;
  }

  char *mode = argv[1];
  size_t array_size = 0;
  int *array = NULL;
  int num_threads = 0;
  struct MinMax final_result;
  int result_output_ok = 0; // Флаг успешного вывода результата

  // --- ЛОГИКА РЕЖИМА PIPE (Генерация данных) ---
  if (strcmp(mode, "pipe") == 0) {
    if (argc != 5) {
      fprintf(stderr, "Ошибка: Для режима 'pipe' требуются <seed> <размер> <потоки>.\n");
      return 1;
    }
    
    // Парсинг аргументов с использованием strtoull для size_t
    unsigned int seed = (unsigned int)strtoull(argv[2], NULL, 10);
    array_size = strtoull(argv[3], NULL, 10);
    num_threads = atoi(argv[4]);

    if (array_size == 0 || num_threads <= 0) {
      fprintf(stderr, "Ошибка: Размер и число потоков должны быть > 0.\n");
      return 1;
    }

    // Выделение памяти (Heap Allocation - ФИКС КРУПНЫХ МАССИВОВ)
    printf("[PIPE MODE] Выделение памяти для %zu элементов...\n", array_size);
    array = (int *)malloc(array_size * sizeof(int)); 
    if (array == NULL) {
      fprintf(stderr, "Ошибка: не удалось выделить %zu байт памяти (не хватает RAM?).\n", array_size * sizeof(int));
      return 1;
    }

    // Заполнение массива (Генерация)
    printf("[PIPE MODE] Заполнение массива...\n");
    srand(seed);
    for (size_t i = 0; i < array_size; i++) {
      array[i] = rand(); 
    }

  } 
  
  // --- ЛОГИКА РЕЖИМА FILES (Чтение из файла) ---
  else if (strcmp(mode, "files") == 0) {
    if (argc != 5) {
      fprintf(stderr, "Ошибка: Для режима 'files' требуются <входной_файл> <выходной_файл> <потоки>.\n");
      return 1;
    }

    const char *input_filename = argv[2];
    num_threads = atoi(argv[4]);

    if (num_threads <= 0) {
      fprintf(stderr, "Ошибка: Число потоков должно быть > 0.\n");
      return 1;
    }
    
    printf("[FILES MODE] Чтение данных из файла '%s'...\n", input_filename);
    
    if (read_array_from_file(input_filename, &array, &array_size) != 0) {
        fprintf(stderr, "Ошибка: Не удалось прочитать массив из файла.\n");
        return 1;
    }
    
    // ПРОВЕРКА ОГРАНИЧЕНИЯ ДЛЯ ФАЙЛОВОГО РЕЖИМА ТОЖЕ
    if (array_size > 800000000) {
      fprintf(stderr, "Ошибка: Превышено максимальное количество элементов (800,000,000).\n");
      fprintf(stderr, "Прочитано из файла: %zu элементов.\n", array_size);
      free(array);
      return 1;
    }
  } 
  
  // --- НЕИЗВЕСТНЫЙ РЕЖИМ ---
  else {
    fprintf(stderr, "Ошибка: Неизвестный режим работы '%s'. Используйте 'pipe' или 'files'.\n", mode);
    return 1;
  }
  
  // --- ОБЩАЯ МНОГОПОТОЧНАЯ ЛОГИКА ---
  
  if (array == NULL || array_size == 0) {
      fprintf(stderr, "Ошибка: Массив пуст.\n");
      return 1;
  }
  
  // 4. ЗАПУСК ПОТОКОВ
  struct ThreadData *thread_data = malloc(num_threads * sizeof(struct ThreadData));
  if (thread_data == NULL) {
    fprintf(stderr, "Ошибка: не удалось выделить память для данных потоков.\n");
    free(array);
    return 1;
  }
  
  size_t chunk_size = array_size / num_threads;
  final_result.min = INT_MAX;
  final_result.max = INT_MIN;

  printf("Запуск %d потоков (общий размер: %zu)...\n", num_threads, array_size);

  for (int i = 0; i < num_threads; i++) {
    thread_data[i].thread_id = i;
    thread_data[i].array = array;
    thread_data[i].begin = i * chunk_size;
    // Обеспечиваем, что последний поток обработает все оставшиеся элементы
    thread_data[i].end = (i == num_threads - 1) ? array_size : (i + 1) * chunk_size;

    if (pthread_create(&thread_data[i].thread, NULL, find_min_max_thread, &thread_data[i]) != 0) {
      perror("pthread_create");
      free(array); free(thread_data); return 1;
    }
  }

  // 5. ОЖИДАНИЕ И ОБЪЕДИНЕНИЕ РЕЗУЛЬТАТОВ
  for (int i = 0; i < num_threads; i++) {
    pthread_join(thread_data[i].thread, NULL);
    
    if (thread_data[i].result.min < final_result.min) final_result.min = thread_data[i].result.min;
    if (thread_data[i].result.max > final_result.max) final_result.max = thread_data[i].result.max;
  }
  

  // 6. ВЫВОД РЕЗУЛЬТАТА
  if (strcmp(mode, "pipe") == 0) {
    // Вывод в stdout для режима pipe
    printf("--- Результат (pipe) --- \n");
    printf("Глобальный минимум: %d\n", final_result.min);
    printf("Глобальный максимум: %d\n", final_result.max);
    printf("------------------------\n");
    result_output_ok = 0;
  } else {
    // Вывод в файл для режима files
    const char *output_filename = argv[3];
    printf("[FILES MODE] Запись результата в файл '%s'...\n", output_filename);
    result_output_ok = write_result_to_file(output_filename, final_result);
  }


  // 7. ОЧИСТКА
  free(array);
  free(thread_data);

  return (result_output_ok != 0) ? 1 : 0;
}


// --- РЕАЛИЗАЦИЯ ФУНКЦИЙ ФАЙЛОВОГО I/O ---

// Читает массив из файла. Предполагается, что файл содержит только int в бинарном формате.
int read_array_from_file(const char *filename, int **array, size_t *array_size_out) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("Error opening input file for reading");
        return -1;
    }
    
    // Определяем размер файла
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    if (file_size < 0) {
        perror("Error determining file size");
        fclose(f);
        return -1;
    }

    *array_size_out = file_size / sizeof(int);
    fseek(f, 0, SEEK_SET);
    
    // Выделяем память
    *array = (int*)malloc(*array_size_out * sizeof(int));
    if (*array == NULL) {
        fprintf(stderr, "Error: Could not allocate memory for array from file.\n");
        fclose(f);
        return -1;
    }
    
    // Читаем весь массив
    size_t items_read = fread(*array, sizeof(int), *array_size_out, f);
    if (items_read != *array_size_out) {
        fprintf(stderr, "Warning: Expected %zu items, read %zu items.\n", *array_size_out, items_read);
    }

    fclose(f);
    printf("Прочитано %zu элементов.\n", *array_size_out);
    return 0;
}

// Записывает результат в файл (текстовый формат).
int write_result_to_file(const char *filename, struct MinMax result) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("Error opening output file for writing");
        return -1;
    }
    
    fprintf(f, "Min: %d\n", result.min);
    fprintf(f, "Max: %d\n", result.max);
    
    fclose(f);
    return 0;
}