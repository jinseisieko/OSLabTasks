#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_MSG 1025 // 1024 bytes + 1 for null terminator

int main() {
    int pipefd[2];
    
    // Create unnamed pipe
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // Subscriber (child process)
        close(pipefd[1]); // Close unused write end
        
        char msg[MAX_MSG];
        ssize_t bytes_read;
        // Read from pipe and print to stdout
        while ((bytes_read = read(pipefd[0], msg, MAX_MSG - 1)) > 0) {
            msg[bytes_read] = '\0';
            printf("%s", msg);
            fflush(stdout);
        }
        close(pipefd[0]);
        exit(0);
    } else {
        // Publisher (parent process)
        close(pipefd[0]); // Close unused read end
        
        char msg[MAX_MSG];
        // Read from stdin and write to pipe
        while (fgets(msg, MAX_MSG, stdin) != NULL) {
            write(pipefd[1], msg, strlen(msg));
        }
        close(pipefd[1]);
        wait(NULL); // Wait for child to finish
    }

    return 0;
}