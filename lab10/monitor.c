#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>

volatile sig_atomic_t keep_running = 1;

void sigint_handler(int sig) {
    keep_running = 0;
}

void print_stat_info(const char *filepath) {
    struct stat st;
    if (stat(filepath, &st) == 0) {
        printf("Stat info for %s:\n", filepath);
        printf("  Inode: %ld\n", (long)st.st_ino);
        printf("  Links: %ld\n", (long)st.st_nlink);
        printf("  Size: %ld bytes\n", (long)st.st_size);
        printf("  Permissions: %o\n", st.st_mode & 0777);
        printf("  UID: %d, GID: %d\n", st.st_uid, st.st_gid);
    } else {
        printf("Stat info for %s: Not available (deleted or inaccessible)\n", filepath);
    }
}

void print_all_stats(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir");
        return;
    }
    
    struct dirent *entry;
    char fullpath[PATH_MAX];
    
    printf("\n--- Printing all stats in %s ---\n", path);
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
        print_stat_info(fullpath);
    }
    closedir(dir);
    printf("--- End of stats ---\n\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory_path>\n", argv[0]);
        return 1;
    }
    
    const char *path = argv[1];
    
    // Print stats on startup
    print_all_stats(path);
    
    int fd = inotify_init1(IN_NONBLOCK);
    if (fd < 0) {
        perror("inotify_init1");
        return 1;
    }
    
    int wd = inotify_add_watch(fd, path, IN_ACCESS | IN_CREATE | IN_DELETE | IN_MODIFY | IN_OPEN | IN_ATTRIB);
    if (wd < 0) {
        perror("inotify_add_watch");
        close(fd);
        return 1;
    }
    
    signal(SIGINT, sigint_handler);
    
    char buffer[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    
    printf("Monitoring %s. Press Ctrl-C to stop.\n", path);
    
    while (keep_running) {
        ssize_t len = read(fd, buffer, sizeof(buffer));
        if (len < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(100000); // 100ms sleep to prevent busy waiting
                continue;
            }
            perror("read");
            break;
        }
        
        for (char *ptr = buffer; ptr < buffer + len; ) {
            struct inotify_event *event = (struct inotify_event *)ptr;
            
            if (event->len > 0) {
                char filepath[PATH_MAX];
                snprintf(filepath, sizeof(filepath), "%s/%s", path, event->name);
                
                printf("\nEvent detected on: %s\n", event->name);
                
                if (event->mask & IN_ACCESS)  printf("  - %s is accessed (IN_ACCESS)\n", event->name);
                if (event->mask & IN_CREATE)  printf("  - %s is created (IN_CREATE)\n", event->name);
                if (event->mask & IN_DELETE)  printf("  - %s is deleted (IN_DELETE)\n", event->name);
                if (event->mask & IN_MODIFY)  printf("  - %s is modified (IN_MODIFY)\n", event->name);
                if (event->mask & IN_OPEN)    printf("  - %s is opened (IN_OPEN)\n", event->name);
                if (event->mask & IN_ATTRIB)  printf("  - %s metadata changed (IN_ATTRIB)\n", event->name);
                
                if (event->mask & IN_DELETE) {
                    printf("Stat info for %s: File has been deleted.\n", event->name);
                } else {
                    print_stat_info(filepath);
                }
            }
            
            ptr += sizeof(struct inotify_event) + event->len;
        }
    }
    
    printf("\nSIGINT received. Terminating...\n");
    
    // Print stats before termination
    print_all_stats(path);
    
    inotify_rm_watch(fd, wd);
    close(fd);
    
    return 0;
}