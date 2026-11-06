#define _DEFAULT_SOURCE
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define MAX_ARGS 256

typedef enum { INTERACTIVE, BATCH } Mode;
typedef struct{
    char *argv[MAX_ARGS + 1];
    char *infile;
    char *outfile;
    int argc;
    int append; 
} Command;

int get_cwd(char *cwd);
int parse_args(char *cmd_buf, Command *cmd_list);

int is_builtin(char **argv);
int is_special(char *tok);
int exec_builtin(int argc, char **argv);


