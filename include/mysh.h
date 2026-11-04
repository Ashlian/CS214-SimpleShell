#define _DEFAULT_SOURCE
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define MAX_ARGS 128

typedef enum { INTERACTIVE, BATCH } Mode;
typedef struct{
    char **argv;
    char *infile;
    char *outfile;
    int argc;
    int append; 
} Command;

int get_cwd(char *cwd);
int parse_args(char *cmd_buf, char **argv);
int wildcard_setup(char **argv);
int cmdlist_setup(Command* cmd_list, char **argv);

int is_builtin(char **argv);
int exec_builtin(int argc, char **argv);
