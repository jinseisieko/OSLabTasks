#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

extern char **environ;

#define MAX_INPUT 256
#define MAX_ARGS 64


char *find_command_path(const char *cmd) {
    if (strchr(cmd, '/') != NULL) {
        char *path = strdup(cmd);
        return path;
    }

    const char *path_env = getenv("PATH");
    if (path_env == NULL) return NULL;

    char *path_copy = strdup(path_env);
    char *dir = strtok(path_copy, ":");

    while (dir != NULL) {
        char full_path[MAX_INPUT];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);

        if (access(full_path, X_OK) == 0) {
            free(path_copy);
            return strdup(full_path);
        }
        dir = strtok(NULL, ":");
    }

    free(path_copy);
    return NULL;
}

int parse_input(char *input, char **args) {
    int argc = 0;
    char *token = strtok(input, " \t\n");

    while (token != NULL && argc < MAX_ARGS - 1) {
        args[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[argc] = NULL;
    return argc;
}

int main() {
    char input[MAX_INPUT];
    char *args[MAX_ARGS];
    char prompt[] = "myshell> ";


    while (1) {
        printf("%s", prompt);
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\n");
            break;  // EOF (Ctrl+D)
        }

        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) == 0) continue;

        int argc = parse_input(input, args);
        if (argc == 0) continue;

        if (strcmp(args[0], "exit") == 0) {
            printf("Выход из shell.\n");
            break;
        }

        if (strcmp(args[0], "cd") == 0) {
            if (argc < 2) {
                chdir(getenv("HOME"));
            } else {
                if (chdir(args[1]) != 0) {
                    perror("cd");
                }
            }
            continue;
        }


        int background = 0;
        int cmd_start = 0;

        if (strcmp(args[0], "bg") == 0) {
            background = 1;
            cmd_start = 1;
            if (args[cmd_start] == NULL) {
                printf("Using: bg <command> [args]\n");
                continue;
            }
        }

        char *cmd_path = find_command_path(args[cmd_start]);
        if (cmd_path == NULL) {
            printf("myshell: command not found: %s\n", args[cmd_start]);
            continue;
        }

        char *exec_args[MAX_ARGS];
        int exec_argc = 0;
        for (int i = cmd_start; args[i] != NULL; i++) {
            exec_args[exec_argc++] = args[i];
        }
        exec_args[exec_argc] = NULL;

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork failed");
            free(cmd_path);
            continue;
        }

        if (pid == 0) {

            execve(cmd_path, exec_args, environ);

            perror("execve failed");
            free(cmd_path);
            exit(1);
        }

        if (background) {
            printf("[+] Process %d is running on backgroud: %s\n", pid, args[cmd_start]);
        } else {
            int status;
            waitpid(pid, &status, 0);

            if (WIFEXITED(status)) {
            } else if (WIFSIGNALED(status)) {
                printf("Process %d done with %d\n", pid, WTERMSIG(status));
            }
        }

        free(cmd_path);
    }

    return 0;
}