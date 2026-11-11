#ifndef MYSH_H
#define MYSH_H

#define _DEFAULT_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Constants
#define BUFFER_SIZE 4096
#define MAX_ARGS 256

// Types
typedef enum { INTERACTIVE, BATCH } Mode;
typedef struct {
    char *argv[MAX_ARGS + 1];
    char *infile;
    char *outfile;
    int argc;
    int append;
} Command;

// Shared variables
extern const char *home;

// Parser
int parse_args(char *cmd_buf, Command **cmd_list);
int expand_wildcards(Command *cmd, char *tok);
int check_match(char *name, char *prefix, size_t prefix_len, char *suffix);
void free_cmds(Command *cmds, int n_cmds);
int is_special(char *tok);

// Execution
int exec_builtin(int argc, char **argv);
int find_exec(const char *arg, char **exec_path);
int is_builtin(char **argv);

#endif