#define _POSIX_C_SOURCE 200809L
#include "jobs/execution.h"
#include "cmd_exec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

void handle_execution_flow(char *line) {
    if (!line || *line == '\0') return;

    char *line_copy = strdup(line);
    if (!line_copy) { perror("strdup"); return; }
    
    char *current_cmd = line_copy;
    char *next_sep;

    while (current_cmd && *current_cmd) {
        next_sep = strpbrk(current_cmd, ";&");
        int is_background = 0;

        if (next_sep) {
            if (*next_sep == '&') is_background = 1;
            *next_sep = '\0';
        } else {
            // Check if the whole line ends in '&' without being a separator
            int len = strlen(current_cmd);
            if (len > 0 && current_cmd[len-1] == '&') {
                is_background = 1;
                current_cmd[len-1] = '\0';
            }
        }
        
        char *trimmed_cmd = current_cmd;
        while (isspace((unsigned char)*trimmed_cmd)) trimmed_cmd++;
        int len = strlen(trimmed_cmd);
        while (len > 0 && isspace((unsigned char)trimmed_cmd[len - 1])) {
            trimmed_cmd[--len] = '\0';
        }

        if (*trimmed_cmd) {
            execute_cmd(trimmed_cmd, is_background);
        }
        
        current_cmd = (next_sep) ? next_sep + 1 : NULL;
    }

    free(line_copy);
}