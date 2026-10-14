#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_MSG 1025
#define DIR_PATH "/tmp/ex1"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <subscriber_id>\n", argv[0]);
        return 1;
    }
    
    int id = atoi(argv[1]);
    if (id <= 0) {
        fprintf(stderr, "Subscriber ID must be > 0\n");
        return 1;
    }

    char fifo_path[256];
    snprintf(fifo_path, sizeof(fifo_path), "%s/s%d", DIR_PATH, id);

    // IMPORTANT: Opening with O_RDONLY blocks until the publisher's child opens it with O_WRONLY.
    // This ensures the connection is established before any data is sent, preventing SIGPIPE.
    int fd = open(fifo_path, O_RDONLY);
    if (fd == -1) {
        perror("open fifo");
        return 1;
    }

    char msg[MAX_MSG];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, msg, MAX_MSG - 1)) > 0) {
        msg[bytes_read] = '\0';
        printf("%s", msg);
        fflush(stdout);
    }

    close(fd);
    return 0;
}