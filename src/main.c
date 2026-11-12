#include "mysh.h"

const char *home = NULL;

static int get_cwd(char *cwd);

int main(int argc, char const *argv[]) {
    int status = 0;
    int exit_flag = 0;
    int in_fd = STDIN_FILENO;
    Mode mode = BATCH;
    InputBuffer input = {0};
    char cmd_buf[BUFFER_SIZE];

    // Iniitalize home path
    home = getenv("HOME");
    if (!home) {
        perror("HOME env:");
        status = 1;
        goto cleanup;
    }

    // Set mode to interactive if ran from terminal and batch if ran from file.
    if (argc == 1 && isatty(STDIN_FILENO)) {
        mode = INTERACTIVE;
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

    if (mode == INTERACTIVE) {
        printf("Welcome to my shell!\n");
    }

    while (1) {
        if (mode == INTERACTIVE) {
            char cwd[PATH_MAX];
            if (get_cwd(cwd) < 0) {
                perror("get_cwd error");
                goto cleanup;
            }
            fprintf(stderr, "%s$ ", cwd);
        }

        int result = read_command(in_fd, cmd_buf, &input);

        if (result < 0) {
            goto cleanup;
        } else if (result == 0) {
            break; // Done reading file.
        }

        Command *cmd_list = NULL;
        int n_cmds = parse_args(cmd_buf, &cmd_list);
        if (n_cmds < 1) {
            fprintf(stderr, "main: no commands parsed\n");
            continue;
        }
        // Execute command
        exec_commands(cmd_list, n_cmds, mode, &exit_flag);
        free_cmds(cmd_list, n_cmds);
        if (exit_flag) {
            break;
        }
        continue;
    }

cleanup:
    if (mode == INTERACTIVE) {
        printf("Exiting my shell.\n");
    }

    close(in_fd);
    return status;
}

static int get_cwd(char *cwd) {
    size_t len = strlen(home);
    getcwd(cwd, PATH_MAX);

    if (cwd == NULL) {
        return -1;
    }

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
