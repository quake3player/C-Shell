#define _POSIX_C_SOURCE 200809L
#include "exotic/signals.h"
#include "jobs/jobs.h"
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

// Global variable to track the foreground process group.
// 'volatile' is important because it's accessed by signal handlers.
volatile pid_t g_foreground_pgid = 0;

// Handler for SIGINT (Ctrl+C)
void handle_sigint(int sig) {
    (void)sig;
    if (g_foreground_pgid > 0) {
        // There is a foreground process, send the signal to its entire group.
        kill(-g_foreground_pgid, SIGINT);
    }
    // Print a newline to make the prompt appear cleanly, but the shell itself doesn't exit.
    printf("\n");
}

// Handler for SIGTSTP (Ctrl+Z)
void handle_sigtstp(int sig) {
    (void)sig;
    if (g_foreground_pgid > 0) {
        kill(-g_foreground_pgid, SIGTSTP);
    } else {
        // No foreground job, so just print a newline for a clean prompt
        printf("\n");
    }
}

void init_signal_handlers(void) {
    struct sigaction sa_int = {0};
    sa_int.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa_int, NULL);

    struct sigaction sa_tstp = {0};
    sa_tstp.sa_handler = handle_sigtstp;
    sigaction(SIGTSTP, &sa_tstp, NULL);

    // Ignore signals that are meant for interactive shells.
    // This prevents the shell from being stopped or interrupted by terminal control signals.
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
}