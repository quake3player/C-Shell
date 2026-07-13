#define _POSIX_C_SOURCE 200809L
#include "exotic/bg.h"
#include "jobs/jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

int bg_command(int argc, char **argv) {
    if (argc != 2) {
        printf("bg: job number required\n");
        return 1;
    }

    int job_id = atoi(argv[1]);
    Job* job = find_job_by_id(job_id);

    if (!job) {
        printf("No such job\n");
        return 1;
    }

    if (job->status == RUNNING) {
        printf("bg: job already running\n");
        return 1;
    }

    // Continue the stopped job in the background
    if (kill(-job->pgid, SIGCONT) < 0) {
        perror("bg: kill (SIGCONT)");
        return 1;
    }

    // Update the job's status and print message
    job->status = RUNNING;
    printf("[%d] %s &\n", job->job_id, job->command);

    return 0;
}