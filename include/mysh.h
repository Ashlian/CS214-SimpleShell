#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define MAX_ARGS 128

typedef enum { INTERACTIVE, BATCH } Mode;

int get_cwd(char *cwd);
int parse_args(char *cmd_buf, char **argv);

int exec(int argc, char **argv);
