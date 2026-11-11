#include "mysh.h"

static const char *SEARCH_DIRS[] = {"/usr/local/bin", "/usr/bin", "/bin"};
static const size_t NUM_SEARCH_DIRS = sizeof(SEARCH_DIRS) / sizeof(SEARCH_DIRS[0]);

/*
 * Runs built in commands
 * Input: # of args and args list
 * Output: 0 if complete, -1 if error
 */
int exec_builtin(int argc, char **argv) {
    if (strcmp(argv[0], "cd") == 0) {
        const char *path = home;
        if (argc > 1) {
            path = argv[1];
        }

        if (chdir(path) < 0) {
            perror("chdir failed");
            return -1;
        }
    } else if (strcmp(argv[0], "pwd") == 0) {
        char cwd[PATH_MAX];

        if (getcwd(cwd, PATH_MAX) == NULL) {
            perror("pwd failed");
            return -1;
        }

        printf("%s\n", cwd);
    } else if (strcmp(argv[0], "which") == 0) {
        char *exec_path = NULL;
        size_t num_exec = 0;

        for (size_t i = 1; i < argc; i++) {
            if (find_exec(argv[i], &exec_path) == 0) {
                printf("%s\n", exec_path);
                num_exec++;
            }
        }

        free(exec_path);

        if (num_exec != argc - 1) {
            fprintf(stderr, "No executables");
            return -1;
        }
    } else if (strcmp(argv[0], "exit") == 0) {
        return 1;
    } else {
        fprintf(stderr, "Invalid command");
        return -1;
    }

    return 0;
}

int find_exec(const char *arg, char **exec_path) {
    if (strchr(arg, '/') != NULL) {
        if (access(arg, X_OK) == 0) {
            char *temp = realloc(*exec_path, strlen(arg) + 1);
            if (temp == NULL) {
                return -1;
            }

            *exec_path = temp;
            strcpy(*exec_path, arg);
            return 0;
        }
        fprintf(stderr, "find_exec: path is not executable");
        return -1;
    }

    for (size_t i = 0; i < NUM_SEARCH_DIRS; i++) {
        size_t path_len = strlen(SEARCH_DIRS[i]) + strlen(arg) + 2;

        char *temp = realloc(*exec_path, path_len);
        if (temp == NULL) {
            return -1;
        }

        *exec_path = temp;

        strcpy(*exec_path, SEARCH_DIRS[i]);
        strcat(*exec_path, "/");
        strcat(*exec_path, arg);

        if (access(*exec_path, X_OK) == 0) {
            return 0;
        }
    }

    fprintf(stderr, "find_exec: can not find executable");
    return -1;
}

/*
 * Checks if a command is built in (needed for redirection vs. basic)
 * Input: argument list
 * Output: 1 if true, 0 if false
 */
int is_builtin(char **argv) {
    if (strcmp(argv[0], "cd") == 0) {
        return 1;
    } else if (strcmp(argv[0], "pwd") == 0) {
        return 1;
    } else if (strcmp(argv[0], "which") == 0) {
        return 1;
    } else if (strcmp(argv[0], "exit") == 0) {
        return 1;
    } else {
        return 0;
    }
}