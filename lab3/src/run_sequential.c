#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s seed arraysize\n", argv[0]);
        printf("This program will run sequential_min_max with the given arguments in a separate process.\n");
        return 1;
    }

    // Аргументы для sequential_min_max:
    // argv[0] - имя программы-обертки
    // argv[1] - seed
    // argv[2] - arraysize
    const char *prog_name = "./sequential_min_max"; // Имя исполняемого файла

    pid_t pid = fork();

    if (pid < 0) {
        // Ошибка при создании процесса
        perror("fork failed");
        return 1;
    } else if (pid == 0) {
        // Дочерний процесс
        printf("Child process (PID: %d) is launching %s...\n", getpid(), prog_name);

        // execlp заменяет текущий образ процесса на sequential_min_max.
        // Первый аргумент - имя программы (с поиском в PATH).
        // Далее идут аргументы для запускаемой программы (включая ее имя), 
        // завершаясь NULL.
        execlp(prog_name, prog_name, argv[1], argv[2], (char *)NULL);

        // Если execlp вернула управление, значит произошла ошибка (программа не найдена/не запущена)
        perror("execlp failed to launch sequential_min_max");
        exit(1); 
    } else {
        // Родительский процесс
        int status;
        printf("Parent process (PID: %d) waiting for child (PID: %d) to finish...\n", getpid(), pid);
        
        // Ожидание завершения дочернего процесса
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            return 1;
        }

        if (WIFEXITED(status)) {
            printf("Child process finished with exit code %d.\n", WEXITSTATUS(status));
            return WEXITSTATUS(status);
        } else {
            printf("Child process terminated abnormally.\n");
            return 1;
        }
    }

    return 0;
}