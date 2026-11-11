#include "mysh.h"

const char *home = NULL;

int main(int argc, char const *argv[]) {
    int status = 0;
    int in_fd = STDIN_FILENO;
    size_t cmd_idx = 0;
    ssize_t bytes = -1;
    char read_buf[BUFFER_SIZE];
    char cmd_buf[BUFFER_SIZE];
    // Set mode to interactive if ran from terminal and batch if ran from file.
    Mode mode = isatty(in_fd) ? INTERACTIVE : BATCH;

    // Iniitalize home path
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
                    Command *cmd_list = NULL;
                    int n_cmds = parse_args(cmd_buf, cmd_list);

                    // Check if file is builtin
                    if (n_cmds == 1 && is_builtin(cmd_list[0].argv) && cmd_list[0].infile == NULL &&
                        cmd_list[0].outfile == NULL) {

                        int r = exec_builtin(cmd_list[0].argc, cmd_list[0].argv);
                        if (r < 0) {
                            // Error
                            status = 1;
                            goto cleanup;
                        } else if (r > 0) {
                            // Exit cmd
                            goto cleanup;
                        }

                        continue;
                    }

                    // Run through the cmds - check whether a path is given or not
                    pid_t last_pid = -1;
                    for (int i = 0; i < n_cmds; i++) {
                        Command curr_cmd = cmd_list[i];
                        char *path = NULL;
                        //! IMPORTANT - SEARCH FOR PATH OF FILE IF NO PATH IS GIVEN (/usr/localbin,
                        //! /usr/bin, /bin with access())
                        if (find_exec(curr_cmd.argv[0], &path) != 0) {
                            status = 1;
                            goto cleanup;
                        }

                        pid_t pid = fork();
                        if (pid < 0) {
                            perror("fork");
                            break;
                        }
                        if (pid == 0) {
                            if (mode == BATCH) {
                                int devnull = open("/dev/null", O_RDONLY);
                                if (devnull < 0) {
                                    perror("/dev/null");
                                    exit(EXIT_FAILURE);
                                }
                                dup2(devnull, STDIN_FILENO);
                                close(devnull);
                            }
                            // ! Check if cmd is piped using the position of the cmd in the cmd_list
                            if (n_cmds >= 2) {

                            }
                            // Check if cmd gets redirected using infile and outfile flags
                            else if (curr_cmd.infile || curr_cmd.outfile) {
                                // input redirection
                                if (curr_cmd.infile) {
                                    int input_fd = open(curr_cmd.infile, O_RDONLY);
                                    if (input_fd < 0) {
                                        perror(curr_cmd.infile);
                                        exit(EXIT_FAILURE);
                                    }
                                    dup2(input_fd, STDIN_FILENO);
                                    close(input_fd);
                                }
                                // output redireection
                                if (curr_cmd.outfile) {
                                    int flags =
                                        O_WRONLY | O_CREAT | (curr_cmd.append ? O_APPEND : O_TRUNC);
                                    int output_fd = open(curr_cmd.outfile, flags, 0640);
                                    if (output_fd < 0) {
                                        perror(curr_cmd.outfile);
                                        exit(EXIT_FAILURE);
                                    }
                                    dup2(output_fd, STDOUT_FILENO);
                                    close(output_fd);
                                }
                            }

                            //! CHECK BUILT IN COMMAND TO RUN BEFORE RUNNING EXECV
                            execv(path, curr_cmd.argv);
                            perror(path);
                            exit(EXIT_FAILURE);
                        } else {
                            last_pid = pid;
                        }
                        // ! PARENT CLOSE ALL PIPE ENDS HERE
                    }
                    //! CHECK STATUS OF LAST COMMAND FROM CHILD PROCESS AND PRINT IF WHAT STATUS IT
                    //! WAS EXITED IF IT FAILED AND IF SHELL IS IN INTERACTIVE
                    int last_status;
                    int pid_status;
                    for (int i = 0; i < n_cmds; i++) {
                        pid_t done = wait(&pid_status);
                        if (done == last_pid) {
                            last_status = pid_status;
                        }
                    }

                    cmd_idx = 0;
                    continue;
                }

                cmd_buf[cmd_idx++] = read_buf[i];
            }
        }

        if (bytes < 0) {
            perror("Error reading:");
            status = 1;
            goto cleanup;
        }
    }

cleanup:
    if (mode == INTERACTIVE) {
        printf("Exiting my shell.\n");
    }

    close(in_fd);
    return status;
}

static int get_cwd(char *cwd) {
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
