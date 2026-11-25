#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define PAGE_TABLE_PATH "/tmp/ex2/pagetable"

struct PTE {
    bool valid;
    int frame;
    bool dirty;
    int referenced;
};

struct access {
    char mode;
    int page;
};

static volatile sig_atomic_t sigcont_received = 0;

static void handle_sigcont(int signal_number) {
    (void)signal_number;
    sigcont_received = 1;
}

static int parse_positive_int(const char *text, int *value) {
    char *end;
    long parsed;

    if (text == NULL || *text == '\0') {
        return 0;
    }

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno == ERANGE || *end != '\0' || parsed <= 0 || parsed > INT_MAX) {
        return 0;
    }

    *value = (int)parsed;
    return 1;
}

static int parse_reference_string(const char *text, int pages,
                                  struct access **accesses, size_t *count) {
    const char *cursor = text;
    size_t capacity = strlen(text) + 1;
    struct access *parsed = malloc(capacity * sizeof(*parsed));

    if (parsed == NULL) {
        perror("malloc");
        return 0;
    }

    *count = 0;
    while (*cursor != '\0') {
        while (*cursor != '\0' &&
               (isspace((unsigned char)*cursor) || *cursor == ',' || *cursor == ';')) {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }

        char mode = *cursor++;
        if (mode != 'R' && mode != 'W') {
            fprintf(stderr, "Invalid access mode '%c'; use R or W.\n", mode);
            free(parsed);
            return 0;
        }

        errno = 0;
        char *end;
        long page = strtol(cursor, &end, 10);
        if (cursor == end || errno == ERANGE || page < 0 || page >= pages) {
            fprintf(stderr, "Invalid page number in reference string near '%s'.\n",
                    cursor);
            free(parsed);
            return 0;
        }

        parsed[*count].mode = mode;
        parsed[*count].page = (int)page;
        ++*count;
        cursor = end;
    }

    *accesses = parsed;
    return 1;
}

static void print_page_table(const struct PTE *page_table, int pages) {
    printf("Page table\n");
    for (int i = 0; i < pages; ++i) {
        printf("Page %d ---> valid=%d, frame=%d, dirty=%d, referenced=%d\n",
               i, page_table[i].valid, page_table[i].frame,
               page_table[i].dirty, page_table[i].referenced);
    }
}

int main(int argc, char *argv[]) {
    int pages;
    int pager_pid;
    struct access *accesses = NULL;
    size_t access_count = 0;
    size_t table_size;
    struct PTE *page_table = MAP_FAILED;
    int fd = -1;
    int status = EXIT_FAILURE;

    if (argc != 4) {
        fprintf(stderr,
                "Usage: %s <number_of_pages> <reference_string> <pager_pid>\n",
                argv[0]);
        return EXIT_FAILURE;
    }
    if (!parse_positive_int(argv[1], &pages) ||
        !parse_positive_int(argv[3], &pager_pid)) {
        fprintf(stderr, "The number of pages and pager PID must be positive integers.\n");
        return EXIT_FAILURE;
    }
    if (!parse_reference_string(argv[2], pages, &accesses, &access_count)) {
        return EXIT_FAILURE;
    }

    fd = open(PAGE_TABLE_PATH, O_RDWR);
    if (fd == -1) {
        perror("open pagetable");
        goto cleanup;
    }

    table_size = sizeof(struct PTE) * (size_t)pages;
    page_table = mmap(NULL, table_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (page_table == MAP_FAILED) {
        perror("mmap");
        goto cleanup;
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_sigcont;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGCONT, &action, NULL) == -1) {
        perror("sigaction");
        goto cleanup;
    }

    sigset_t blocked_signals;
    sigset_t original_signals;
    sigemptyset(&blocked_signals);
    sigaddset(&blocked_signals, SIGCONT);
    if (sigprocmask(SIG_BLOCK, &blocked_signals, &original_signals) == -1) {
        perror("sigprocmask");
        goto cleanup;
    }

    printf("mmu process\n");
    printf("----------------------\n");
    printf("Initialized page table\n");
    print_page_table(page_table, pages);

    for (size_t i = 0; i < access_count; ++i) {
        struct access *current = &accesses[i];
        struct PTE *entry = &page_table[current->page];

        printf("-------------------------\n");
        printf("%s Request for page %d\n",
               current->mode == 'R' ? "Read" : "Write", current->page);

        if (!entry->valid) {
            printf("It is not a valid page --> page fault\n");
            printf("Ask pager to load it from disk (SIGUSR1 signal) and wait\n");

            sigcont_received = 0;
            entry->referenced = (int)getpid();
            if (kill((pid_t)pager_pid, SIGUSR1) == -1) {
                perror("kill SIGUSR1");
                goto cleanup;
            }
            while (!sigcont_received) {
                sigsuspend(&original_signals);
            }
            printf("MMU resumed by SIGCONT signal from pager\n");
        } else {
            printf("It is a valid page\n");
        }

        if (current->mode == 'W') {
            printf("It is a write request then set the dirty field\n");
            entry->dirty = true;
        }

        print_page_table(page_table, pages);
        fflush(stdout);
    }

    printf("Done all requests.\n");
    printf("MMU sends SIGUSR1 to the pager.\n");
    fflush(stdout);

    if (munmap(page_table, table_size) == -1) {
        perror("munmap");
        page_table = MAP_FAILED;
        goto cleanup;
    }
    page_table = MAP_FAILED;
    close(fd);
    fd = -1;

    if (kill((pid_t)pager_pid, SIGUSR1) == -1) {
        perror("kill SIGUSR1");
        goto cleanup;
    }

    printf("MMU terminates.\n");
    status = EXIT_SUCCESS;

cleanup:
    if (page_table != MAP_FAILED) {
        munmap(page_table, table_size);
    }
    if (fd != -1) {
        close(fd);
    }
    free(accesses);
    return status;
}
