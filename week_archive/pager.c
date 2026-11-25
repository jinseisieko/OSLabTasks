#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define PAGE_TEXT_SIZE 8
#define PAGE_TABLE_DIR "/tmp/ex2"
#define PAGE_TABLE_PATH "/tmp/ex2/pagetable"

struct PTE {
    bool valid;
    int frame;
    bool dirty;
    int referenced;
};

static volatile sig_atomic_t sigusr1_received = 0;

static void handle_sigusr1(int signal_number) {
    (void)signal_number;
    sigusr1_received = 1;
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

static void generate_random_page(char page[PAGE_TEXT_SIZE + 1]) {
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

    for (int i = 0; i < PAGE_TEXT_SIZE; ++i) {
        page[i] = alphabet[rand() % (sizeof(alphabet) - 1)];
    }
    page[PAGE_TEXT_SIZE] = '\0';
}

static void print_page_table(const struct PTE *page_table, int pages) {
    for (int i = 0; i < pages; ++i) {
        printf("Page %d ---> valid=%d, frame=%d, dirty=%d, referenced=%d\n",
               i, page_table[i].valid, page_table[i].frame,
               page_table[i].dirty, page_table[i].referenced);
    }
}

static void print_ram(char ram[][PAGE_TEXT_SIZE + 1], int frames) {
    printf("RAM array\n");
    for (int i = 0; i < frames; ++i) {
        printf("Frame %d ---> %s\n", i, ram[i]);
    }
}

static void print_disk(char disk[][PAGE_TEXT_SIZE + 1], int pages) {
    printf("Disk array\n");
    for (int i = 0; i < pages; ++i) {
        printf("Page %d ----> %s\n", i, disk[i]);
    }
}

int main(int argc, char *argv[]) {
    int pages;
    int frames;
    int fd = -1;
    struct PTE *page_table = MAP_FAILED;
    char(*ram)[PAGE_TEXT_SIZE + 1] = NULL;
    char(*disk)[PAGE_TEXT_SIZE + 1] = NULL;
    size_t table_size;
    int disk_accesses = 0;
    int status = EXIT_FAILURE;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <number_of_pages> <number_of_frames>\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (!parse_positive_int(argv[1], &pages) ||
        !parse_positive_int(argv[2], &frames)) {
        fprintf(stderr, "The number of pages and frames must be positive integers.\n");
        return EXIT_FAILURE;
    }

    if (mkdir(PAGE_TABLE_DIR, 0777) == -1 && errno != EEXIST) {
        perror("mkdir");
        return EXIT_FAILURE;
    }

    fd = open(PAGE_TABLE_PATH, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd == -1) {
        perror("open pagetable");
        return EXIT_FAILURE;
    }

    table_size = sizeof(struct PTE) * (size_t)pages;
    if (ftruncate(fd, (off_t)table_size) == -1) {
        perror("ftruncate");
        goto cleanup;
    }

    page_table = mmap(NULL, table_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (page_table == MAP_FAILED) {
        perror("mmap");
        goto cleanup;
    }

    ram = calloc((size_t)frames, sizeof(*ram));
    disk = calloc((size_t)pages, sizeof(*disk));
    if (ram == NULL || disk == NULL) {
        perror("calloc");
        goto cleanup;
    }

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_sigusr1;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGUSR1, &action, NULL) == -1) {
        perror("sigaction");
        goto cleanup;
    }

    sigset_t blocked_signals;
    sigset_t original_signals;
    sigemptyset(&blocked_signals);
    sigaddset(&blocked_signals, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &blocked_signals, &original_signals) == -1) {
        perror("sigprocmask");
        goto cleanup;
    }

    srand((unsigned int)(time(NULL) ^ getpid()));

    for (int i = 0; i < pages; ++i) {
        page_table[i].valid = false;
        page_table[i].frame = -1;
        page_table[i].dirty = false;
        page_table[i].referenced = 0;

        int duplicate;
        do {
            duplicate = 0;
            generate_random_page(disk[i]);
            for (int j = 0; j < i; ++j) {
                if (strcmp(disk[i], disk[j]) == 0) {
                    duplicate = 1;
                    break;
                }
            }
        } while (duplicate);
    }

    printf("pager process\n");
    printf("-------------------------\n");
    printf("Initialized page table\n");
    print_page_table(page_table, pages);
    printf("-------------------------\n");
    printf("Initialized RAM\n");
    print_ram(ram, frames);
    printf("-------------------------\n");
    printf("Initialized disk\n");
    print_disk(disk, pages);
    printf("-------------------------\n");
    fflush(stdout);

    for (;;) {
        while (!sigusr1_received) {
            sigsuspend(&original_signals);
        }
        sigusr1_received = 0;

        int requested_page = -1;
        pid_t mmu_pid = -1;
        for (int i = 0; i < pages; ++i) {
            if (page_table[i].referenced != 0) {
                requested_page = i;
                mmu_pid = (pid_t)page_table[i].referenced;
                break;
            }
        }

        if (requested_page == -1) {
            status = EXIT_SUCCESS;
            break;
        }

        printf("A disk access request from MMU Process (pid=%d)\n", (int)mmu_pid);
        printf("Page %d is referenced\n", requested_page);

        int allocated_frame = -1;
        for (int i = 0; i < frames; ++i) {
            if (ram[i][0] == '\0') {
                allocated_frame = i;
                break;
            }
        }

        if (allocated_frame != -1) {
            printf("We can allocate it to free frame %d\n", allocated_frame);
        } else {
            printf("We do not have free frames in RAM\n");
            allocated_frame = rand() % frames;

            int victim_page = -1;
            for (int i = 0; i < pages; ++i) {
                if (page_table[i].valid && page_table[i].frame == allocated_frame) {
                    victim_page = i;
                    break;
                }
            }
            if (victim_page == -1) {
                fprintf(stderr, "Could not find the page occupying frame %d.\n",
                        allocated_frame);
                break;
            }

            printf("Chose a random victim page %d\n", victim_page);
            printf("Replace/Evict it with page %d to be allocated to frame %d\n",
                   requested_page, allocated_frame);

            if (page_table[victim_page].dirty) {
                printf("Page %d is dirty; write it to disk\n", victim_page);
                printf("Copy data from RAM (frame=%d) to disk (page=%d)\n",
                       allocated_frame, victim_page);
                memcpy(disk[victim_page], ram[allocated_frame], PAGE_TEXT_SIZE + 1);
                ++disk_accesses;
            }

            page_table[victim_page].valid = false;
            page_table[victim_page].frame = -1;
            page_table[victim_page].dirty = false;
        }

        printf("Copy data from the disk (page=%d) to RAM (frame=%d)\n",
               requested_page, allocated_frame);
        memcpy(ram[allocated_frame], disk[requested_page], PAGE_TEXT_SIZE + 1);

        page_table[requested_page].valid = true;
        page_table[requested_page].frame = allocated_frame;
        page_table[requested_page].dirty = false;
        page_table[requested_page].referenced = 0;
        ++disk_accesses;

        print_ram(ram, frames);
        printf("disk accesses is %d so far\n", disk_accesses);
        printf("Resume MMU process\n");
        printf("-------------------------\n");
        fflush(stdout);

        if (kill(mmu_pid, SIGCONT) == -1) {
            perror("kill SIGCONT");
            break;
        }
    }

    printf("%d disk accesses in total\n", disk_accesses);
    printf("Pager is terminated\n");

cleanup:
    free(ram);
    free(disk);
    if (page_table != MAP_FAILED) {
        munmap(page_table, table_size);
    }
    if (fd != -1) {
        close(fd);
    }
    unlink(PAGE_TABLE_PATH);
    return status;
}
