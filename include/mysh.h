#ifndef MYSH_H
#define MYSH_H

#define _DEFAULT_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define MAX_ARGS 256

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

extern const char *home;

int read_command(int fd, char *cmd_buf, InputBuffer *input);
int parse_args(char *cmd_buf, Command **cmd_list);
void free_cmds(Command *cmds, int n_cmds);
int exec_commands(Command *cmd_list, int n_cmds, Mode mode);

#endif