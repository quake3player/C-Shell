#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "redirect/input_redirect.h"

int handle_input_redirection(const char *filename) {
    if (!filename) return 0;

    int fd_in = open(filename, O_RDONLY);
    if (fd_in < 0) {
        printf("No such file or directory\n");
        return -1;
    }

    if (dup2(fd_in, STDIN_FILENO) < 0) {
        perror("dup2");
        close(fd_in);
        return -1;
    }

    close(fd_in);
    return 0;
}
