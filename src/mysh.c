#include "mysh.h"

int main(int argc, char const *argv[]) {
    int status = 0;
    int in_fd = STDIN_FILENO;
    ssize_t bytes;
    Mode mode;
    char read_buf[BUFFER_SIZE];
    char line_buf[BUFFER_SIZE];
    char cwd[PATH_MAX];
    if (get_cwd(cwd) != 0) {
        perror("get_cwd error");
        status = 1;
        goto cleanup;
    }

    // Read file of commands
    if (argc > 1) {
        in_fd = open(argv[1], O_RDONLY);
        if (in_fd < 0) {
            perror("Error opening file:");
            status = 1;
            goto cleanup;
        }
    }

    // Set mode to interactive if ran from terminal and batch if ran from file.
    mode = isatty(in_fd) ? INTERACTIVE : BATCH;

    if (mode == INTERACTIVE) {
        printf("Welcome to my shell!\n");
    }

    while (1) {
        if (mode == INTERACTIVE) {
            fprintf(stderr, "%s$ ", cwd);
        }

        while ((bytes = read(in_fd, read_buf, BUFFER_SIZE)) > 0) {
            for (size_t i = 0; i < bytes; i++) {
                char c = read_buf[i];
            }
        }
    }

    if (bytes < 0) {
        perror("Error reading:");
        status = 1;
        goto cleanup;
    }

cleanup:
    if (mode == INTERACTIVE) {
        printf("Exiting my shell.\n");
    }

    close(in_fd);
    return status;
}

int get_cwd(char *cwd) {
    const char *home = getenv("HOME");
    getcwd(cwd, PATH_MAX);

    if (!home || !cwd) {
        return 1;
    }

    size_t len = strlen(home);

    // Replace home dir with '~'
    if (strncmp(cwd, home, len) == 0 && (cwd[len] == '\0' || cwd[len] == '/')) {
        size_t i = 0;
        while (cwd[len + i] != '\0') {
            cwd[i + 1] = cwd[len + i];
            i++;
        }
        cwd[0] = '~';
        cwd[i + 1] = '\0';
    }

    return 0;
}