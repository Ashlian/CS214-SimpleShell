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
typedef struct {
    char buf[BUFFER_SIZE];
    size_t pos;
    size_t len;
} InputBuffer;

// Shared variables
extern const char *home;

// Reader
int read_command(int fd, char *cmd_buf, InputBuffer *input);

// Parser
int parse_args(char *cmd_buf, Command **cmd_list);
void free_cmds(Command *cmds, int n_cmds);

// Execution
int exec_builtin(int argc, char **argv);
int find_exec(const char *arg, char **exec_path);
int is_builtin(char **argv);

#endif