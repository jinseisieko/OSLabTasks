#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_MSG 1025
#define DIR_PATH "/tmp/ex1"

int main(int argc, char *argv[]) {
    int n = atoi(argv[1]);

    // Create directory for FIFOs
    if (mkdir(DIR_PATH, 0777) == -1 && errno != EEXIST) {
        perror("mkdir");
        return 1;
    }

    int pipes[n][2];
    pid_t pids[n];

    for (int i = 0; i < n; i++) {
        char fifo_path[256];
        snprintf(fifo_path, sizeof(fifo_path), "%s/s%d", DIR_PATH, i + 1);
        
        // Create named pipe (remove old one if it exists)
        unlink(fifo_path);
        if (mkfifo(fifo_path, 0666) == -1) {
            perror("mkfifo");
            return 1;
        }

        // Create unnamed pipe for parent-to-child communication
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            return 1;
        }

        pids[i] = fork();
        if (pids[i] == -1) {
            perror("fork");
            return 1;
        }

        if (pids[i] == 0) {
            // Child process (Publisher to specific subscriber)
            close(pipes[i][1]); // Close write end of unnamed pipe
            
            // IMPORTANT: Opening with O_WRONLY blocks until a subscriber opens it with O_RDONLY.
            // This built-in synchronization prevents SIGPIPE errors.
            int fd = open(fifo_path, O_WRONLY);
            if (fd == -1) {
                perror("open fifo");
                exit(1);
            }
            
            char msg[MAX_MSG];
            ssize_t bytes_read;
            while ((bytes_read = read(pipes[i][0], msg, MAX_MSG)) > 0) {
                write(fd, msg, bytes_read);
            }
            close(fd);
            close(pipes[i][0]);
            exit(0);
        } else {
            // Parent process
            close(pipes[i][0]); // Close read end of unnamed pipe
        }
    }

    // Parent reads from stdin and broadcasts to all children
    char msg[MAX_MSG];
    while (fgets(msg, MAX_MSG, stdin) != NULL) {
        size_t len = strlen(msg);
        for (int i = 0; i < n; i++) {
            write(pipes[i][1], msg, len);
        }
    }

    // Close all write ends to signal EOF to children
    for (int i = 0; i < n; i++) {
        close(pipes[i][1]);
    }

    // Wait for all children to finish
    for (int i = 0; i < n; i++) {
        wait(NULL);
    }

    // Clean up FIFOs
    for (int i = 0; i < n; i++) {
        char fifo_path[256];
        snprintf(fifo_path, sizeof(fifo_path), "%s/s%d", DIR_PATH, i + 1);
        unlink(fifo_path);
    }

    return 0;
}