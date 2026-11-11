#include "mysh.h"

static const char *SEARCH_DIRS[] = {"/usr/local/bin", "/usr/bin", "/bin"};
static const size_t NUM_SEARCH_DIRS = sizeof(SEARCH_DIRS) / sizeof(SEARCH_DIRS[0]);

static int is_builtin(char **argv);
static int find_exec(const char *arg, char **exec_path);
static int exec_builtin(int argc, char **argv);

int exec_commands(Command *cmd_list, int n_cmds, Mode mode) {
    Command *cmd = &cmd_list[0];

    // Run standalone built-in commands
    if (n_cmds == 1 && is_builtin(cmd->argv) && cmd->infile == NULL && cmd->outfile == NULL) {
        return exec_builtin(cmd->argc, cmd->argv);
    }

    // Find executable
    char *path = NULL;
    //! IMPORTANT - SEARCH FOR PATH OF FILE IF NO PATH IS GIVEN (/usr/localbin, /usr/bin, /bin with
    //! access())
    if (!is_builtin(cmd->argv) && find_exec(cmd->argv[0], &path) < 0) {
        free(path);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        free(path);
        return -1;
    }

    if (pid == 0) {
        // Batch mode defaults to /dev/null stdin
        if (mode == BATCH) {
            int devnull = open("/dev/null", O_RDONLY);
            if (devnull < 0) {
                perror("/dev/null");
                exit(EXIT_FAILURE);
            }

            if (dup2(devnull, STDIN_FILENO) < 0) {
                perror("dup2");
                _exit(EXIT_FAILURE);
            }

            close(devnull);
        }
        // ! Check if cmd is piped using the position of the cmd in the cmd_list
        if (n_cmds >= 2) {

        }
        // Check if cmd gets redirected using infile and outfile flags
        else if (cmd->infile || cmd->outfile) {
            // input redirection
            if (cmd->infile) {
                int input_fd = open(cmd->infile, O_RDONLY);
                if (input_fd < 0) {
                    perror(cmd->infile);
                    exit(EXIT_FAILURE);
                }
                dup2(input_fd, STDIN_FILENO);
                close(input_fd);
            }
            // output redireection
            if (cmd->outfile) {
                int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
                int output_fd = open(cmd->outfile, flags, 0640);
                if (output_fd < 0) {
                    perror(cmd->outfile);
                    exit(EXIT_FAILURE);
                }
                dup2(output_fd, STDOUT_FILENO);
                close(output_fd);
            }
        }

        //! CHECK BUILT IN COMMAND TO RUN BEFORE RUNNING EXECV
        if (is_builtin(cmd->argv)) {
            int result = exec_builtin(cmd->argc, cmd->argv);
            exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
        }
        execv(path, cmd->argv);
        perror("execv");
        exit(EXIT_FAILURE);
    }

    // Parent Process
    free(path);

    //! CHECK STATUS OF LAST COMMAND FROM CHILD PROCESS AND PRINT IF WHAT STATUS IT
    //! WAS EXITED IF IT FAILED AND IF SHELL IS IN INTERACTIVE
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }

    // Terminal by exit
    if (WIFEXITED(status)) {
        int code = WEXITSTATUS(status);
        if (code != 0 && mode == INTERACTIVE) {
            fprintf(stderr, "Exited with status %d\n", code);
        }

        return code == 0 ? 0 : -1;
    }

    // Terminated by signal
    if (WIFSIGNALED(status)) {
        if (mode == INTERACTIVE) {
            psignal(WTERMSIG(status), "Terminated");
        }
    }

    return -1;
}

/*
 * Runs built in commands
 * Input: # of args and args list
 * Output: 0 if complete, -1 if error
 */
static int exec_builtin(int argc, char **argv) {
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

        if (num_exec != argc - 1) {
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
        fprintf(stderr, "find_exec: path is not executable");
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

    fprintf(stderr, "find_exec: can not find executable");
    return -1;
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