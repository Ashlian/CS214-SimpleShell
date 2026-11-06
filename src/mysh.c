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
                    // char *argv[MAX_ARGS];
                    Command *cmd_list;
                    int argc = parse_args(cmd_buf, cmd_list);
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
            }
            c++;
            continue;
        }
        // check for redirection "<", ">", ">>"
        else if (strcmp(tok, "<") == 0) {
            if (tokens[i + 1] == NULL || is_special(tokens[i + 1])) {
                // syntax error handler
            }
            cmd_list[c].infile = tokens[i + 1];
            i++;
            continue;
        }
        else if (strcmp(tok, ">") == 0 || strcmp(tok, ">>") == 0) {
            if (tokens[i + 1] == NULL || is_special(tokens[i + 1])) {
                // syntax error handler
            }
            cmd_list[c].outfile = tokens[i + 1];
            cmd_list[c].append = (strcmp(tok, ">>") == 0);
            i++;
            continue;
        }
        // check for '*' wildcard
        else if (star != NULL) {

        }
        // everything else 
        else {
            if(cmd_list[c].argc >= MAX_ARGS) {
                fprintf(stderr, "mysh: too many arugments\n");
                // syntax error handler
            }
            cmd_list[c].argv[cmd_list[c].argc++] = tok;
        }
    }
    
    if (cmd_list[c].argc == 0) {
        // syntax error handler
    }
    return ncmds;
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