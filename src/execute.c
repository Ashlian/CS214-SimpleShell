#include "mysh.h"

static const char *SEARCH_DIRS[] = {"/usr/local/bin", "/usr/bin", "/bin"};
static const size_t NUM_SEARCH_DIRS = sizeof(SEARCH_DIRS) / sizeof(SEARCH_DIRS[0]);

static int is_builtin(char **argv);
static int find_exec(const char *arg, char **exec_path);
static int setup_redirection(const Command *cmd);
static int exec_parent_builtin(const Command *cmd);
static int exec_builtin(int argc, char *const argv[]);

int exec_commands(Command *cmd_list, int n_cmds, Mode mode, int *exit_flag) {
    Command *first_cmd = &cmd_list[0];

    // Detect exit in pipeline from parent
    for (int i = 0; i < n_cmds; i++) {
        if (strcmp(cmd_list[i].argv[0], "exit") == 0) {
            *exit_flag = 1;
            break;
        }
    }

    // Run standalone built-in commands
    if (n_cmds == 1 && is_builtin(first_cmd->argv)) {
        return exec_parent_builtin(first_cmd);
    }

    fflush(stdout);

    int prev_read = -1;
    int n_started = 0;
    size_t last_status = 0;
    pid_t pids[n_cmds];

    for (size_t i = 0; i < n_cmds; i++) {
        Command *cmd = &cmd_list[i];
        int pipefd[2] = {-1, -1};

        // Create pipe unless last command
        if (i < n_cmds - 1) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                break;
            }
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");

            if (pipefd[0] >= 0)
                close(pipefd[0]);
            if (pipefd[1] >= 0)
                close(pipefd[1]);

            break;
        }

        // Child Process
        if (pid == 0) {
            // Batch mode defaults to /dev/null stdin
            if (i == 0 && (mode == BATCH || cmd->outfile || n_cmds > 1)) {
                int devnull = open("/dev/null", O_RDONLY);
                if (devnull < 0) {
                    perror("/dev/null");
                    _exit(EXIT_FAILURE);
                }

                if (dup2(devnull, STDIN_FILENO) < 0) {
                    perror("dup2");
                    _exit(EXIT_FAILURE);
                }

                close(devnull);
            }
            if (n_cmds > 1) {
                // Read from previous pipe
                if (prev_read >= 0) {
                    if (dup2(prev_read, STDIN_FILENO) < 0) {
                        perror("dup2");
                        _exit(EXIT_FAILURE);
                    }
                }
                // Write to the next pipe
                if (pipefd[1] >= 0) {
                    if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
                        perror("dup2");
                        _exit(EXIT_FAILURE);
                    }
                }
            } else {
                if (setup_redirection(cmd) < 0) {
                    _exit(EXIT_FAILURE);
                }
            }

            // Close descriptors
            if (prev_read >= 0)
                close(prev_read);
            if (pipefd[0] >= 0)
                close(pipefd[0]);
            if (pipefd[1] >= 0)
                close(pipefd[1]);

            // Excute built-in
            if (is_builtin(cmd->argv)) {
                int result = exec_builtin(cmd->argc, cmd->argv);
                fflush(stdout);
                _exit(result >= 0 ? EXIT_SUCCESS : EXIT_FAILURE);
            }

            // Find execute external command
            char *path = NULL;

            if (find_exec(cmd->argv[0], &path) < 0) {
                free(path);
                _exit(EXIT_FAILURE);
            }

            execv(path, cmd->argv);
            perror("execv");
            free(path);
            _exit(EXIT_FAILURE);
        }

        // Parent Process
        pids[n_started++] = pid;

        if (prev_read >= 0) {
            close(prev_read);
        }
        if (pipefd[1] >= 0) {
            close(pipefd[1]);
        }

        prev_read = pipefd[0];
    }

    if (prev_read >= 0) {
        close(prev_read);
    }

    // Wait for child processes
    int wait_errors = 0;

    for (size_t i = 0; i < n_started; i++) {
        int status = 0;

        if (waitpid(pids[i], &status, 0) < 0) {
            perror("waitpid");
            wait_errors++;
            continue;
        }

        if (i == n_cmds - 1) {
            last_status = status;
        }
    }

    // Check whether pipeline setup failed
    if (n_started != n_cmds || wait_errors > 0) {
        return -1;
    }

    // Terminal by exit
    if (WIFEXITED(last_status)) {
        int code = WEXITSTATUS(last_status);
        if (code != 0 && mode == INTERACTIVE) {
            fprintf(stderr, "Exited with status %d\n", code);
        }

        return code == 0 ? 0 : -1;
    }

    // Terminated by signal
    if (WIFSIGNALED(last_status)) {
        if (mode == INTERACTIVE) {
            psignal(WTERMSIG(last_status), "Terminated");
        }
    }

    return -1;
}

static int exec_parent_builtin(const Command *cmd) {
    int saved_stdin = -1;
    int saved_stdout = -1;
    int status = -1;

    if (cmd->infile) {
        saved_stdin = dup(STDIN_FILENO);
        if (saved_stdin < 0) {
            perror("dup");
            goto cleanup;
        }
    }

    if (cmd->outfile) {
        fflush(stdout);
        saved_stdout = dup(STDOUT_FILENO);
        if (saved_stdout < 0) {
            perror("dup");
            goto cleanup;
        }
    }

    if (setup_redirection(cmd) < 0) {
        goto cleanup;
    }

    status = exec_builtin(cmd->argc, cmd->argv);

cleanup:
    if (saved_stdout >= 0) {
        fflush(stdout);
    }

    // Restore stdin
    if (saved_stdin >= 0) {
        if (dup2(saved_stdin, STDIN_FILENO) < 0) {
            perror("dup2");
            status = -1;
        }
        close(saved_stdin);
    }

    // Restore stdout
    if (saved_stdout >= 0) {
        if (dup2(saved_stdout, STDOUT_FILENO) < 0) {
            perror("dup2");
            status = -1;
        }
        close(saved_stdout);
    }

    return status;
}

/*
 * Runs built in commands
 * Input: # of args and args list
 * Output: 0 if complete, -1 if error
 */
static int exec_builtin(int argc, char *const argv[]) {
    if (strcmp(argv[0], "cd") == 0) {
        const char *path = home;
        if (argc == 2) {
            path = argv[1];
        } else if (argc > 2) {
            fprintf(stderr, "cd: too many arguments\n");
            return -1;
        }

        if (chdir(path) < 0) {
            fprintf(stderr, "chdir failed\n");
            return -1;
        }
    } else if (strcmp(argv[0], "pwd") == 0) {
        char cwd[PATH_MAX];

        if (getcwd(cwd, PATH_MAX) == NULL) {
            fprintf(stderr, "pwd failed\n");
            return -1;
        }

        printf("%s\n", cwd);
    } else if (strcmp(argv[0], "which") == 0) {
        char *exec_path = NULL;
        size_t num_exec = 0;

        for (size_t i = 1; i < argc; i++) {
            if (find_exec(argv[i], &exec_path) == 0) {
                printf("%s\n", exec_path);
                num_exec++;
            }
        }

        free(exec_path);

        if (argc == 1 || num_exec != argc - 1) {
            fprintf(stderr, "No executables\n");
            return -1;
        }
    } else if (strcmp(argv[0], "exit") == 0) {
        return 1;
    } else {
        fprintf(stderr, "Invalid command\n");
        return -1;
    }

    return 0;
}

static int find_exec(const char *arg, char **exec_path) {
    if (strchr(arg, '/') != NULL) {
        if (access(arg, X_OK) == 0) {
            char *temp = realloc(*exec_path, strlen(arg) + 1);
            if (temp == NULL) {
                return -1;
            }

            *exec_path = temp;
            strcpy(*exec_path, arg);
            return 0;
        }
        fprintf(stderr, "find_exec: path is not executable\n");
        return -1;
    }

    for (size_t i = 0; i < NUM_SEARCH_DIRS; i++) {
        size_t path_len = strlen(SEARCH_DIRS[i]) + strlen(arg) + 2;

        char *temp = realloc(*exec_path, path_len);
        if (temp == NULL) {
            return -1;
        }

        *exec_path = temp;

        strcpy(*exec_path, SEARCH_DIRS[i]);
        strcat(*exec_path, "/");
        strcat(*exec_path, arg);

        if (access(*exec_path, X_OK) == 0) {
            return 0;
        }
    }

    fprintf(stderr, "find_exec: can not find executable\n");
    return -1;
}

/*
 * Sets up input and output redirection.
 * Returns 0 on success, -1 on failure.
 */
static int setup_redirection(const Command *cmd) {
    if (cmd->infile) {
        int fd = open(cmd->infile, O_RDONLY);
        if (fd < 0) {
            perror(cmd->infile);
            return -1;
        }

        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2");
            close(fd);
            return -1;
        }
        close(fd);
    }

    if (cmd->outfile) {
        int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);

        int fd = open(cmd->outfile, flags, 0640);
        if (fd < 0) {
            perror(cmd->outfile);
            return -1;
        }

        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2");
            close(fd);
            return -1;
        }
        close(fd);
    }

    return 0;
}

/*
 * Checks if a command is built in (needed for redirection vs. basic)
 * Input: argument list
 * Output: 1 if true, 0 if false
 */
static int is_builtin(char **argv) {
    if (strcmp(argv[0], "cd") == 0) {
        return 1;
    } else if (strcmp(argv[0], "pwd") == 0) {
        return 1;
    } else if (strcmp(argv[0], "which") == 0) {
        return 1;
    } else if (strcmp(argv[0], "exit") == 0) {
        return 1;
    } else {
        return 0;
    }
}