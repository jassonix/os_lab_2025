#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("usage: %s <seed> <array_size>\n", argv[0]);
        printf("example: %s 42 1000\n", argv[0]);
        return 1;
    }

    pid_t pid = fork();
    
    if (pid < 0) {
        // ошибка при создании процесса
        perror("fork failed");
        return 1;
    }
    else if (pid == 0) {
        // дочерний процесс
        printf("process (PID: %d) start\n", getpid());
        
        // запускаем sequential_min_max с переданными аргументами
        char *args[] = {"./sequential_min_max", argv[1], argv[2], NULL};
        
        execvp(args[0], args);
        
        // если execvp вернул управление, значит произошла ошибка
        perror("failed");
        exit(1);
    }
    else {
        // родительский процесс
        printf("(PID: %d) created process (PID: %d)\n", getpid(), pid);
        printf("waiting to complete\n");
        
        int status;
        waitpid(pid, &status, 0);  // ждем завершения дочернего процесса
        
        if (WIFEXITED(status)) {
            printf("process exit, status: %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("process terminated: %d\n", WTERMSIG(status));
        }
        
        printf("parent process completed.\n");
    }
    
    return 0;
}