#define _POSIX_C_SOURCE 200809L
#include "exotic/fg.h"
#include "jobs/jobs.h"
#include "exotic/signals.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

int fg_command(int argc, char **argv) {
    if (argc > 2) {
        printf("fg: too many arguments\n");
        return 1;
    }

    Job* job = NULL;
    if (argc == 1) {
        job = find_most_recent_job();
    } else {
        char *endptr;
        long job_id_long = strtol(argv[1], &endptr, 10);
        if (*endptr != '\0' || argv[1] == endptr) {
            printf("fg: invalid job number\n");
            return 1;
        }
        job = find_job_by_id((int)job_id_long);
    }

    if (!job) {
        printf("No such job\n");
        return 1;
    }

    printf("%s\n", job->command);

    // Give the job terminal control
    tcsetpgrp(STDIN_FILENO, job->pgid);
    g_foreground_pgid = job->pgid;

    // If it's stopped, send the continue signal
    if (job->status == STOPPED) {
        if (kill(-job->pgid, SIGCONT) < 0) {
            perror("fg: kill (SIGCONT)");
            // On failure, give terminal control back to the shell
            tcsetpgrp(STDIN_FILENO, getpgrp());
            g_foreground_pgid = 0;
            return 1;
        }
        // Conceptually, the job is now running
        job->status = RUNNING;
    }

    // Wait for the job to change state (terminate or stop)
    int status;
    pid_t result = waitpid(-job->pgid, &status, WUNTRACED);

    // Take terminal control back for the shell
    tcsetpgrp(STDIN_FILENO, getpgrp());
    g_foreground_pgid = 0;

    // Handle the final state of the job after it has run
    if (result > 0) {
        if (WIFSTOPPED(status)) {
            // The job was stopped again. It's still in our list, so just update its status.
            job->status = STOPPED;
            printf("\n[%d] Stopped \t%s\n", job->job_id, job->command);
        } else {
            // The job terminated (exited or signaled). Remove it from the job list.
            remove_job_by_pgid(job->pgid);
        }
    } else if (result < 0 && errno != ECHILD) {
        // An actual error occurred with waitpid
        perror("fg: waitpid");
        // The job state is uncertain, but we should remove it to be safe
        remove_job_by_pgid(job->pgid);
    } else {
        // waitpid returned 0 or -1 with ECHILD, meaning the job is gone.
        // This can happen if it was killed by an external signal.
        remove_job_by_pgid(job->pgid);
    }

    return 0;
}