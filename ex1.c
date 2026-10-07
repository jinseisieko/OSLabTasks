#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

int main() {
    pid_t pid1, pid2;
    clock_t start, end;
    double time_ms;

    pid1 = fork();

    if (pid1 < 0) {
        perror("Fork 1 failed");
        exit(1);
    } else if (pid1 == 0) {
        start = clock();

        for (volatile long i = 0; i < 200000000L; i++);

        end = clock();
        time_ms = (double)(end - start) * 1000.0 / CLOCKS_PER_SEC;

        printf("Child_1{PID: %d,  Parent PID: %d,  Execution time: %.3f ms}\n",
               getpid(), getppid(), time_ms);
        exit(0);
    }

    pid2 = fork();

    if (pid2 < 0) {
        perror("Fork 2 failed");
        exit(1);
    } else if (pid2 == 0) {
        start = clock();

        for (volatile long i = 0; i < 200000000L; i++);

        end = clock();
        time_ms = (double)(end - start) * 1000.0 / CLOCKS_PER_SEC;

        printf("Child_2{PID: %d,  Parent PID: %d,  Execution time: %.3f ms}\n",
               getpid(), getppid(), time_ms);
        exit(0);
    }

    start = clock();

    for (volatile long i = 0; i < 200000000L; i++);

    end = clock();
    time_ms = (double)(end - start) * 1000.0 / CLOCKS_PER_SEC;

    printf("Parent{PID: %d,  Parent PID: %d,  Execution time: %.3f ms}\n",
           getpid(), getppid(), time_ms);

    wait(NULL);
    wait(NULL);

    return 0;
}