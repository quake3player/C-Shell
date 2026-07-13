#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "redirect/output_redirect.h"

int handle_output_redirection(const char *filename, int append) {
    if (!filename) return 0;

    int flags = O_WRONLY | O_CREAT;
    flags |= (append ? O_APPEND : O_TRUNC);

    int fd_out = open(filename, flags, 0644);
    if (fd_out < 0) {
        printf("Unable to create file for writing\n");
        return -1;
    }

    if (dup2(fd_out, STDOUT_FILENO) < 0) {
        perror("dup2");
        close(fd_out);
        return -1;
    }

    close(fd_out);
    return 0;
}
