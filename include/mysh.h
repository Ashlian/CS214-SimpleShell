#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

typedef enum { INTERACTIVE, BATCH } Mode;

int get_cwd(char *cwd, const char *home);

int exec(char *cmd_buf);