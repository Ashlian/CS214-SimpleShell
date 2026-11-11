#include "mysh.h"

int read_command(int fd, char *cmd_buf, InputBuffer *input) {
    size_t cmd_idx = 0;

    while (1) {
        // Read when empty buffer
        if (input->pos >= input->len) {
            ssize_t bytes = read(fd, input->buf, BUFFER_SIZE);

            if (bytes < 0) {
                perror("read");
                return -1;
            }

            if (bytes == 0) {
                if (cmd_idx == 0) {
                    return 0; // Done
                }
                // Process last command
                cmd_buf[cmd_idx] = '\0';
                return 1;
            }

            input->pos = 0;
            input->len = (size_t)bytes;
        }

        char c = input->buf[input->pos++];

        if (c == '\n') {
            cmd_buf[cmd_idx] = '\0';
            return 1;
        }

        if (cmd_idx >= BUFFER_SIZE - 1) {
            fprintf(stderr, "InputBuffer overflow\n");
            return -1;
        }

        cmd_buf[cmd_idx++] = c;
    }
}