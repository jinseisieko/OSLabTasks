#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>

const char* watched_path = NULL;

void find_all_hlinks(const char* source) {
    struct stat source_st;
    if (stat(source, &source_st) != 0) {
        perror("stat source");
        return;
    }
    
    printf("Finding all hard links to %s (inode: %ld, dev: %ld) in %s:\n", 
        source, (long)source_st.st_ino, (long)source_st.st_dev, watched_path);
    
    DIR *dir = opendir(watched_path);
    if (!dir) {
        perror("opendir");
        return;
    }
    
    struct dirent *entry;
    char fullpath[PATH_MAX];
    char abs_path[PATH_MAX];
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        
        snprintf(fullpath, sizeof(fullpath), "%s/%s", watched_path, entry->d_name);
        struct stat st;
        if (lstat(fullpath, &st) == 0) {
            if (st.st_ino == source_st.st_ino && st.st_dev == source_st.st_dev) {
                if (realpath(fullpath, abs_path) != NULL) {
                    printf("  Inode: %ld, Absolute Path: %s\n", (long)st.st_ino, abs_path);
                    if (S_ISREG(st.st_mode)) {
                        FILE *f = fopen(fullpath, "r");
                        if (f) {
                            char content[256];
                            if (fgets(content, sizeof(content), f)) {
                                printf("  Content: %s", content);
                            }
                            fclose(f);
                        }
                    }
                }
            }
        }
    }
    closedir(dir);
}

void unlink_all(const char* source) {
    struct stat source_st;
    if (stat(source, &source_st) != 0) {
        perror("stat source");
        return;
    }
    
    char links[100][PATH_MAX];
    int count = 0;
    
    DIR *dir = opendir(watched_path);
    if (!dir) {
        perror("opendir");
        return;
    }
    
    struct dirent *entry;
    char fullpath[PATH_MAX];
    char abs_path[PATH_MAX];
    
    while ((entry = readdir(dir)) != NULL && count < 100) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        
        snprintf(fullpath, sizeof(fullpath), "%s/%s", watched_path, entry->d_name);
        struct stat st;
        if (lstat(fullpath, &st) == 0) {
            if (st.st_ino == source_st.st_ino && st.st_dev == source_st.st_dev) {
                if (realpath(fullpath, abs_path) != NULL) {
                    strncpy(links[count], abs_path, PATH_MAX);
                    count++;
                }
            }
        }
    }
    closedir(dir);
    
    if (count <= 1) {
        printf("No duplicates to remove. Only %d link exists in the watched directory.\n", count);
        if (count == 1) printf("Kept hard link: %s\n", links[0]);
        return;
    }
    
    for (int i = 1; i < count; i++) {
        if (unlink(links[i]) == 0) {
            printf("Removed duplicate hard link: %s\n", links[i]);
        } else {
            perror("unlink");
        }
    }
    
    printf("Last remaining hard link: %s\n", links[0]);
    
    struct stat kept_st;
    if (stat(links[0], &kept_st) == 0) {
        printf("Stat info of kept hard link (%s):\n", links[0]);
        printf("  Inode: %ld\n", (long)kept_st.st_ino);
        printf("  Number of hard links: %ld\n", (long)kept_st.st_nlink);
    }
}

void create_sym_link(const char* source, const char* link) {
    char link_path[PATH_MAX];
    snprintf(link_path, sizeof(link_path), "%s/%s", watched_path, link);
    
    if (symlink(source, link_path) == 0) {
        printf("Created symbolic link: %s -> %s\n", link_path, source);
    } else {
        perror("symlink");
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <directory_path>\n", argv[0]);
        return 1;
    }
    
    watched_path = argv[1];
    char file1[PATH_MAX], file11[PATH_MAX], file12[PATH_MAX];
    
    snprintf(file1, sizeof(file1), "%s/myfile1.txt", watched_path);
    snprintf(file11, sizeof(file11), "%s/myfile11.txt", watched_path);
    snprintf(file12, sizeof(file12), "%s/myfile12.txt", watched_path);
    
    // 1. Create myfile1.txt
    FILE *f = fopen(file1, "w");
    if (f) {
        fprintf(f, "Hello world.");
        fclose(f);
        printf("Created %s\n", file1);
    }
    
    // 2. Create two hard links
    link(file1, file11);
    link(file1, file12);
    printf("Created hard links %s and %s\n", file11, file12);
    
    // 3. Find all hard links and print info
    find_all_hlinks(file1);
    
    // 4. Move myfile1.txt to /tmp
    char tmp_file[PATH_MAX] = "/tmp/myfile1.txt";
    rename(file1, tmp_file);
    printf("Moved %s to %s\n", file1, tmp_file);
    
    // 5. Modify myfile11.txt
    f = fopen(file11, "a");
    if (f) {
        fprintf(f, " Modified content.");
        fclose(f);
        printf("Modified %s\n", file11);
    }
    
    // 6. Create symbolic link
    create_sym_link(tmp_file, "myfile13.txt");
    
    // 7. Modify /tmp/myfile1.txt
    f = fopen(tmp_file, "a");
    if (f) {
        fprintf(f, " Modified in /tmp.");
        fclose(f);
        printf("Modified %s\n", tmp_file);
    }
    
    // 8. Remove duplicates of hard links to myfile11.txt in watched directory
    unlink_all(file11);
    
    return 0;
}
