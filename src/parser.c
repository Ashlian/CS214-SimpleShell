#include "mysh.h"

/*
 * Converts whole lines into a list of tokens
 * Input: shell buffer
 * Output: number of commands and the full command list
 */
int parse_args(char *cmd_buf, Command *cmd_list) {
    // tokenize with whitespace
    size_t max = strlen(cmd_buf) / 2 + 1;
    char **tokens = malloc((max + 1) * sizeof(char *));
    if (!tokens) {
        perror("token_init failed");
        free(tokens);
        return -1;
    }
    int n_tokens = 0;
    for (char *t = strtok(cmd_buf, " \t\n"); t != NULL; t = strtok(NULL, " \t\n")) {
        tokens[n_tokens++] = t;
    }
    if (n_tokens == 0) {
        free(tokens);
        return 0;
    }
    tokens[n_tokens] = NULL;
    // initialize command list
    int n_cmds = 1;
    for (int i = 0; i < n_tokens; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            n_cmds++;
        }
    }
    cmd_list = calloc(n_cmds, sizeof(Command));

    int c = 0;
    for (int i = 0; i < n_tokens - 1; i++) {
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
        } else if (strcmp(tok, ">") == 0 || strcmp(tok, ">>") == 0) {
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
                free_cmds(cmd_list, n_cmds);
                return -1;
            }
        }
        // everything else
        else {
            if (cmd_list[c].argc >= MAX_ARGS) {
                goto too_many_args;
            }
            cmd_list[c].argv[cmd_list[c].argc++] = strdup(tok);
        }
    }

    if (cmd_list[c].argc == 0) {
        goto syntax_error;
    }
    return n_cmds;

syntax_error:
    fprintf(stderr, "mysh: syntax error\n");
    free(tokens);
    free_cmds(cmd_list, n_cmds);
    return -1;

too_many_args:
    fprintf(stderr, "mysh: too many arugments\n");
    free(tokens);
    free_cmds(cmd_list, n_cmds);
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
    } else if (strncmp(name, prefix, prefix_len) != 0) {
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

    if (slash == NULL) {
        dirpath = strdup(".");
        pattern = tok;
        dir_prefix_len = 0;
    } else {
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

            if (prefix_len == 0 && name[0] == '.')
                continue;
            if (!check_match(name, pattern, prefix_len, suffix))
                continue;

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
            } else {
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
        } else {
            cmd->argv[cmd->argc++] = copy;
        }
    }

    status = 0;

cleanup:
    if (dir)
        closedir(dir);
    free(dirpath);
    return status;
}

void free_cmds(Command *cmd_list, int n_cmds) {
    if (cmd_list == NULL) {
        return;
    }

    for (int c = 0; c < n_cmds; c++) {
        for (int a = 0; a < cmd_list[c].argc; a++) {
            free(cmd_list[c].argv[a]);
        }
    }
    free(cmd_list);
}

int is_special(char *tok) {
    return ((strcmp(tok, "|") == 0) || (strcmp(tok, "<") == 0) || (strcmp(tok, ">") == 0) ||
            (strcmp(tok, ">>") == 0));
}

/*
 * Checks if a command is built in (needed for redirection vs. basic)
 * Input: argument list
 * Output: 1 if true, 0 if false
 */
int is_builtin(char **argv) {
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