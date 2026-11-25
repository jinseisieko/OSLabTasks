#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void aboba() {
    printf("aboba\n");
}

int main() {
    // for (int i = 1; i < 65; i++) 
    // {
    //     signal(i, SIG_IGN);
    // } 
    atexit(aboba);
    signal(SIGINT, exit);
    pid_t my_pid = getpid();
    printf("PID моего процесса: %d\n", my_pid);
    while (1)
    {
        /* code */
    }
    
    
    return 0;
}