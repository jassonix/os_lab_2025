#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    pid_t pids[10];
    int i;

    /* цикл порождает дочерние процессы и сохраняет их pid в массиве */
    for (i = 9; i >= 0; --i) {
        pids[i] = fork();
        if (pids[i] == 0) {
            printf("Child%d\n", i);
            sleep(i + 1);
            _exit(0);
        }
    }

    /* родительский процесс последовательно дожидается завершения каждого ребёнка */
    for (i = 9; i >= 0; --i) {
        printf("parent%d\n", i);
        waitpid(pids[i], NULL, 0);
    }

    return 0;
}
