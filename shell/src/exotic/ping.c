#define _POSIX_C_SOURCE 200809L
#include "exotic/ping.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>   // <-- Add this header for isspace()

int ping_command(int argc, char **argv) {
    if (argc != 3) {
        printf("ping: Invalid Syntax!\n");
        return 1;
    }

    char *pid_str = argv[1];
    char *sig_str = argv[2];
    // printf("%s %s " , argv[1] , argv[2] ); -- debugging

    // ** THE FIX IS HERE **
    // Trim whitespace from the beginning of the PID string.
    while (isspace((unsigned char)*pid_str)) {
        pid_str++;
    }

    // Validate that PID is a number
    char *endptr_pid;
    pid_t pid = strtol(pid_str, &endptr_pid, 10);

    // Trim whitespace from the end of the PID string.
    while (isspace((unsigned char)*endptr_pid)) {
        endptr_pid++;
    }
    
    if (*endptr_pid != '\0') {
        printf("ping: Invalid PID!\n");
        // printf("%s %s %s" , pid_str , sig_str , endptr_pid); - debugging
        return 1;
    }

    // Validate that signal number is a number
    char *endptr_sig;
    long sig_num_long = strtol(sig_str, &endptr_sig, 10);
    if (*endptr_sig != '\0') {
        printf("ping: Invalid signal number!\n");
        return 1;
    }
    
    int signal_number = (int)sig_num_long;
    int actual_signal = signal_number % 32;

    if (kill(pid, actual_signal) == 0) {
        printf("Sent signal %d to process with pid %d\n", signal_number, pid);
    } else {
        if (errno == ESRCH) {
            printf("No such process found\n");
        } else {
            perror("ping");
        }
        return 1;
    }

    return 0;
}