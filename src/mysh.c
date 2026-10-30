#include "mysh.h"

typedef enum { INTERACTIVE, BATCH } Mode;

int main(int argc, char const *argv[]) {
    int status = 0;
    int in_fd = STDIN_FILENO;
    // Read file of commands
    if (argc > 1) {
        in_fd = open(argv[1], O_RDONLY);
        if (in_fd < 0) {
            perror("Error openng file:");
            status = 1;
            goto cleanup;
        }
    }

    char read_buf[BUFFER_SIZE];

    ssize_t bytes;

    while ((bytes = read(in_fd, read_buf, BUFFER_SIZE)) > 0) {
        /* code */
    }

    if (bytes < 0) {
        perror("Error reading:");
        status = 1;
        goto cleanup;
    }

cleanup:
    close(in_fd);
    return status;
}