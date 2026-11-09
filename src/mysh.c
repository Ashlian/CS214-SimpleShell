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
                    Command *cmd_list;
                    int ncmds = parse_args(cmd_buf, cmd_list);
                    if (ncmds == 1 && is_builtin(cmd_list[0].argv) && cmd_list[0].infile == NULL && cmd_list[0].outfile == NULL) {
                        int r = exec(argc, argv);
                        if (r < 0) {
                            status = 1;
                            goto cleanup;
                        } else if (r > 0) {
                            goto cleanup;
                        }
                        continue; 
                    }

                    // Run through the cmds - check whether a path is given or not
                    pid_t last_pid;
                    for(int i = 0; i < ncmds; i++) {
                        Command curr_cmd = cmd_list[i];
                        char *path; 
                        //! IMPORTANT - SEARCH FOR PATH OF FILE IF NO PATH IS GIVEN (/usr/localbin, /usr/bin, /bin with access())

                        pid_t pid;
                        if ((pid = fork()) == 0) {
                            last_pid = pid;
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
                            if (ncmds >= 2) {

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
                                    int flags = O_WRONLY | O_CREAT | (curr_cmd.append ? O_APPEND : O_TRUNC);
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
                        }
                        // ! PARENT CLOSE ALL PIPE ENDS HERE
                    }
                    //! CHECK STATUS OF LAST COMMAND FROM CHILD PROCESS AND PRINT IF WHAT STATUS IT WAS EXITED IF IT FAILED AND IF SHELL IS IN INTERACTIVE
                    int last_status;
                    int pid_status;
                    for (int i = 0; i < ncmds; i++) {
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

/*
 * Converts whole lines into a list of tokens
 * Input: shell buffer
 * Output: number of commands and the full command list
 */
int parse_args(char *cmd_buf, Command *cmd_list) {
    // tokenize with whitespace
    size_t max = strlen(cmd_buf) / 2 + 1;
    char **tokens = malloc((max+1) * sizeof(char *));
    if (!tokens) {
        perror("token_init failed");
        free(tokens);
        return -1;

    }
    int ntokens = 0;
    for (char *t = strtok(cmd_buf, " \t\n"); t != NULL, t = strtok(NULL, " \t\n");) {
        tokens[ntokens++] = t;
    }
    if (ntokens == 0) { 
        free(tokens);
        return 0;
    }
    tokens[ntokens] = NULL;
    // initialize command list
    int ncmds = 1;
    for (int i = 0; i < ntokens; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            ncmds++;
        }
    }
    cmd_list = calloc(ncmds, sizeof(Command));

    int c = 0;
    for (int i = 0; i < ntokens - 1; i++) {
        char *tok = tokens[i];
        char *star = strchr(tok, '*');
        // check for pipe "|"
        if (strcmp(tok, "|") == 0) {
            if (cmd_list[c].argc == 0) {
                // syntax error handler
                goto syntax_error;
            }
            c++;
            continue;
        }
        // check for redirection "<", ">", ">>"
        else if (strcmp(tok, "<") == 0) {
            if (tokens[i + 1] == NULL || is_special(tokens[i + 1])) {
                // syntax error handler
                goto syntax_error;
            }
            cmd_list[c].infile = tokens[i + 1];
            i++;
            continue;
        }
        else if (strcmp(tok, ">") == 0 || strcmp(tok, ">>") == 0) {
            if (tokens[i + 1] == NULL || is_special(tokens[i + 1])) {
                // syntax error handler
                goto syntax_error;
            }
            cmd_list[c].outfile = tokens[i + 1];
            cmd_list[c].append = (strcmp(tok, ">>") == 0);
            i++;
            continue;
        }
        // check for '*' wildcard
        else if (star != NULL) {
            if (expand_wildcards(&cmd_list[c], tok) < 0) {
                free(tokens);
                free_cmds(cmd_list, ncmds);
                return -1;
            }
        }
        // everything else 
        else {
            if(cmd_list[c].argc >= MAX_ARGS) {
                goto too_many_args;
            }
            cmd_list[c].argv[cmd_list[c].argc++] = strdup(tok);
        }
    }
    
    if (cmd_list[c].argc == 0) {
        // syntax error handler
        goto syntax_error;
    }
    return ncmds;

syntax_error:
    fprintf(stderr, "mysh: syntax error\n");
    free(tokens);
    free_cmds(cmd_list, ncmds);
    return -1;

too_many_args:
    fprintf(stderr, "mysh: too many arugments\n");
    free(tokens);
    free_cmds(cmd_list, ncmds);
    return -1;
}

/*
 * Returns 1 if name matches the pattern, 0 if not
 */
int check_match(char *name, char *prefix, size_t prefix_len, char *suffix) {
    size_t name_len = strlen(name);
    size_t suffix_len = strlen(suffix);
    if (name_len < prefix_len + suffix_len) {
        return 0;
    }
    else if (strncmp(name, prefix, prefix_len) != 0){
        return 0;
    }
    return (strcmp(name + name_len - suffix_len, suffix) == 0);
}

int expand_wildcards(Command *cmd, char *tok) {
    int status = -1;
    int matches_found = 0;
    char *dirpath = NULL;
    DIR *dir = NULL;

    // Split tok into pieces (directory, before *, after *)
    char *slash = strrchr(tok, '/');
    char *pattern;
    size_t dir_prefix_len;

    if(slash == NULL) {
        dirpath = strdup(".");
        pattern = tok;
        dir_prefix_len = 0;
    }
    else {
        size_t dlen = (slash == tok) ? 1 : (size_t)(slash - tok);
        dirpath = malloc(dlen + 1);
        if (dirpath) {
            memcpy(dirpath, tok, dlen);
            dirpath[dlen] = '\0';
        }
        pattern = slash + 1;
        dir_prefix_len = slash - tok + 1;
    }

    if (dirpath == NULL) {
        perror("malloc");
        goto cleanup;
    }

    char *star = strchr(pattern, '*');
    size_t prefix_len = star - pattern;
    char *suffix = star + 1;

    // Open directory and check directory to find matches
    dir = opendir(dirpath);
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            char *name = entry->d_name;

            if (prefix_len == 0 && name[0] == '.') continue;
            if (!check_match(name, pattern, prefix_len, suffix)) continue;

            size_t len = dir_prefix_len + strlen(name) + 1;
            char *full_arg = malloc(len);
            if (!full_arg) {
                perror("malloc");
                goto cleanup;
            }
            memcpy(full_arg, tok, dir_prefix_len);
            strcpy(full_arg + dir_prefix_len, name);

            if (cmd->argc >= MAX_ARGS) {
                free(full_arg); 
                goto cleanup;
            }
            else {
                cmd->argv[cmd->argc++] = full_arg;
            }
            matches_found++;
        }
    }

    // No match means add token as is
    if (matches_found == 0) {
        char *copy = strdup(tok);
        if (copy == NULL) { 
            perror("strdup"); 
            goto cleanup; 
        }
        if (cmd->argc >= MAX_ARGS) {
            free(copy);
            goto cleanup;
        } 
        else {
            cmd->argv[cmd->argc++] = copy;
        }
    }

    status = 0;

cleanup:
    if (dir) closedir(dir);
    free(dirpath);
    return status;
}

void free_cmds(Command *cmd_list, int ncmds) {
    if (cmd_list == NULL) {
        return;
    }

    for (int c = 0; c < ncmds; c++) {
        for(int a = 0; a < cmd_list[c].argc; a++) {
            free(cmd_list[c].argv[a]);
        }
    }
    free(cmd_list);
}

/*
 * Checks if a command is built in (needed for redirection vs. basic)
 * Input: argument list
 * Output: 1 if true, 0 if false
 */
int is_builtin(char **argv){
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

/*
 * Runs built in commands
 * Input: # of args and args list
 * Output: 0 if complete, -1 if error
 */
int exec_builtin(int argc, char **argv) {
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

int is_special(char *tok) {
    return ((strcmp(tok, "|") == 0) || (strcmp(tok, "<") == 0) || (strcmp(tok, ">") == 0) || (strcmp(tok, ">>") == 0));
}