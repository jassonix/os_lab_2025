CC=gcc
CFLAGS=-I.
PTHREAD_FLAGS=-pthread
AR=ar
ARFLAGS=rcs

# Основная цель - сборка всех программ
all: sequential_min_max parallel_min_max run_sequential zombie_demo process_memory parallel_sum

libpsum.a : sum_lib.o
	$(AR) $(ARFLAGS) $@ $<

# sequential_min_max - последовательная версия
sequential_min_max : utils.o find_min_max.o utils.h find_min_max.h
	$(CC) -o sequential_min_max sequential_min_max.c find_min_max.o utils.o $(CFLAGS)

# parallel_min_max - параллельная версия
parallel_min_max : utils.o find_min_max.o utils.h find_min_max.h
	$(CC) -o parallel_min_max parallel_min_max.c utils.o find_min_max.o $(CFLAGS)

# run_sequential - программа для запуска sequential_min_max в отдельном процессе
run_sequential :
	$(CC) -o run_sequential run_sequential.c $(CFLAGS)

# zombie_demo - демонстрация зомби-процессов
zombie_demo :
	$(CC) -o zombie_demo zombie_demo.c $(CFLAGS)

# process_memory - демонстрация областей памяти процесса
process_memory :
	$(CC) -o process_memory process_memory.c $(CFLAGS)

# parallel_sum - многопоточный расчет суммы
parallel_sum : parallel_sum.c libpsum.a utils.o sum_lib.h
	$(CC) $(PTHREAD_FLAGS) -o parallel_sum parallel_sum.c libpsum.a utils.o $(CFLAGS)

# Статическая библиотека с функцией суммирования
libpsum.a : sum_lib.o
	$(AR) $(ARFLAGS) $@ $<

# Цели для компиляции объектных файлов:

# utils.o - объектный файл утилит
utils.o : utils.c utils.h
	$(CC) -o utils.o -c utils.c $(CFLAGS)

# find_min_max.o - объектный файл функций поиска min/max
find_min_max.o : find_min_max.c find_min_max.h utils.h
	$(CC) -o find_min_max.o -c find_min_max.c $(CFLAGS)

# sum_lib.o - объектный файл функций суммы
sum_lib.o : sum_lib.c sum_lib.h
	$(CC) $(PTHREAD_FLAGS) -o sum_lib.o -c sum_lib.c $(CFLAGS)

# Очистка - удаление всех сгенерированных файлов
clean :
	rm -f utils.o find_min_max.o sum_lib.o libpsum.a sequential_min_max parallel_min_max run_sequential zombie_demo process_memory parallel_sum
