#include "mysh.h"

static const char *home = NULL;

int main(int argc, char const *argv[]) {
    int status = 0;
    int in_fd = STDIN_FILENO;
    size_t cmd_idx;
    ssize_t bytes;
    Mode mode;
    char read_buf[BUFFER_SIZE];
    char cmd_buf[BUFFER_SIZE];

    home = getenv("HOME");
    if (!home) {
        perror("HOME env:");
        status = 1;
        goto cleanup;
    }

    // Read file of commands
    if (argc > 1) {
        in_fd = open(argv[1], O_RDONLY);
        if (in_fd < 0) {
            perror("Error opening file:");
            status = 1;
            goto cleanup;
        }
    }

    // Set mode to interactive if ran from terminal and batch if ran from file.
    mode = isatty(in_fd) ? INTERACTIVE : BATCH;

    if (mode == INTERACTIVE) {
        printf("Welcome to my shell!\n");
    }

    while (1) {
        if (mode == INTERACTIVE) {
            char cwd[PATH_MAX];
            if (get_cwd(cwd) < 0) {
                perror("get_cwd error");
                status = 1;
                goto cleanup;
            }
            fprintf(stderr, "%s$ ", cwd);
        }
        while ((bytes = read(in_fd, read_buf, BUFFER_SIZE)) > 0) {
            for (size_t i = 0; i < bytes; i++) {
                // Execute command
                if (read_buf[i] == '\n') {
                    cmd_buf[cmd_idx] = '\0';
                    char *argv[MAX_ARGS];
                    int argc = parse_args(cmd_buf, argv);
                    if (argc > 0) {
                        int r = exec(argc, argv);
                        if (r < 0) {
                            status = 1;
                            goto cleanup;
                        } else if (r > 0) {
                            goto cleanup;
                        }
                    }
                    cmd_idx = 0;
                    continue;
                }

                cmd_buf[cmd_idx++] = read_buf[i];
            }
        }
    }

    if (bytes < 0) {
        perror("Error reading:");
        status = 1;
        goto cleanup;
    }

cleanup:
    if (mode == INTERACTIVE) {
        printf("Exiting my shell.\n");
    }

    close(in_fd);
    return status;
}

int get_cwd(char *cwd) {
    size_t len = strlen(home);
    getcwd(cwd, PATH_MAX);

    if (cwd == NULL) {
        return -1;
    }

    // Replace home dir with '~'
    if (strncmp(cwd, home, len) == 0 && (cwd[len] == '\0' || cwd[len] == '/')) {
        size_t i = 0;
        while (cwd[len + i] != '\0') {
            cwd[i + 1] = cwd[len + i];
            i++;
        }
        cwd[0] = '~';
        cwd[i + 1] = '\0';
    }

    return 0;
}

int parse_args(char *cmd_buf, char **argv) {
    int argc = 0;
    char *token = strtok(cmd_buf, " ");

    while (token != NULL && argc < MAX_ARGS - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }

    return argc;
}

int exec(int argc, char **argv) {
    if (strcmp(argv[0], "cd") == 0) {
        char *path = home;
        if (argc > 1) {
            path = argv[1];
        }

        if (chdir(path) < 0) {
            perror("chdir failed");
            return -1;
        }
    } else if (strcmp(argv[0], "pwd") == 0) {
        char cwd[PATH_MAX];
        getcwd(cwd, PATH_MAX);

        if (cwd == NULL) {
            perror("pwd failed");
            return -1;
        }

        fprintf(stderr, "%s", cwd);
    } else if (strcmp(argv[0], "which") == 0) {
        // to be implemented...
    } else if (strcmp(argv[0], "exit") == 0) {
        return 1;
    } else {
        perror("Invalid command");
        return -1;
    }
    return 0;
}