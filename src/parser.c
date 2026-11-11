#include "mysh.h"

/*
 * Converts whole lines into a list of tokens
 * Input: shell buffer
 * Output: number of commands and the full command list
 */
int parse_args(char *cmd_buf, Command **cmd_list) {
    // Clear command list
    *cmd_list = NULL;

    // Trim comments before tokenization
    char *comment = strchr(cmd_buf, '#');
    if (comment != NULL) {
        *comment = '\0';
    }

    // Tokenize with whitespace
    size_t max_tokens = strlen(cmd_buf) / 2 + 1;
    int n_tokens = 0;
    char **tokens = malloc((max_tokens + 1) * sizeof(char *));
    if (!tokens) {
        perror("token_init");
        free(tokens);
        return -1;
    }

    char *token = strtok(cmd_buf, " \t\n");
    while (token != NULL) {
        tokens[n_tokens++] = token;
        token = strtok(NULL, " \t\n");
    }

    if (n_tokens == 0) {
        free(tokens);
        return 0;
    }
    tokens[n_tokens] = NULL;

    // Initialize command list
    int n_cmds = 1;
    for (int i = 0; i < n_tokens; i++) {
        if (strcmp(tokens[i], "|") == 0) {
            n_cmds++;
        }
    }

    Command *cmds = calloc(n_cmds, sizeof(Command));
    if (!cmds) {
        free(tokens);
        return -1;
    }

    int c = 0;
    for (int i = 0; i < n_tokens; i++) {
        char *tok = tokens[i];
        char *star = strchr(tok, '*');
        // Check for pipe "|"
        if (strcmp(tok, "|") == 0) {
            if (cmds[c].argc == 0) {
                fprintf(stderr, "mysh: syntax error\n");
                goto error;
            }
            c++;
            continue;
        } else if (strcmp(tok, "<") == 0) {
            // Check for redirection "<", ">", ">>"
            if (tokens[i + 1] == NULL || is_special(tokens[i + 1])) {
                fprintf(stderr, "mysh: syntax error\n");
                goto error;
            }
            cmds[c].infile = strdup(tokens[i + 1]);
            if (cmds[c].infile == NULL) {
                perror("strdup");
                goto error;
            }
            i++;
            continue;
        } else if (strcmp(tok, ">") == 0 || strcmp(tok, ">>") == 0) {
            if (tokens[i + 1] == NULL || is_special(tokens[i + 1])) {
                fprintf(stderr, "mysh: syntax error\n");
                goto error;
            }
            cmds[c].outfile = strdup(tokens[i + 1]);
            if (cmds[c].infile == NULL) {
                perror("strdup");
                goto error;
            }
            cmds[c].append = (strcmp(tok, ">>") == 0);
            i++;
            continue;
        } else if (star != NULL) {
            // Check for '*' wildcard
            if (expand_wildcards(&cmds[c], tok) < 0) {
                goto error;
            }
        } else {
            if (cmds[c].argc >= MAX_ARGS) {
                fprintf(stderr, "mysh: too many arugments\n");
                goto error;
            }
            char *copy = strdup(tok);
            if (copy == NULL) {
                perror("strdup");
                goto error;
            }
            cmds[c].argv[cmds[c].argc++] = copy;
        }
    }

    if (cmds[c].argc == 0) {
        fprintf(stderr, "mysh: syntax error\n");
        goto error;
    }

    free(tokens);
    *cmd_list = cmds;
    return n_cmds;

error:
    free(tokens);
    free_cmds(cmds, n_cmds);
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

void free_cmds(Command *cmds, int n_cmds) {
    if (cmds == NULL) {
        return;
    }

    for (int c = 0; c < n_cmds; c++) {
        for (int a = 0; a < cmds[c].argc; a++) {
            free(cmds[c].argv[a]);
        }

        free(cmds[c].infile);
        free(cmds[c].outfile);
    }
    free(cmds);
}

int is_special(char *tok) {
    return ((strcmp(tok, "|") == 0) || (strcmp(tok, "<") == 0) || (strcmp(tok, ">") == 0) ||
            (strcmp(tok, ">>") == 0));
}
