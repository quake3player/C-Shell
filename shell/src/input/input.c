#define _POSIX_C_SOURCE 200809L
#include "input/input.h"

#include <stdio.h>
#include <stdlib.h>

char *read_input() {
    char *line = NULL;
    size_t len = 0;
    ssize_t nread = getline(&line, &len, stdin);

    if (nread == -1) {
        free(line);
        return NULL; // EOF or error
    }

    // Strip newline
    if (nread > 0 && line[nread - 1] == '\n') {
        line[nread - 1] = '\0';
    }

    return line; // Caller must free
}
