#define _POSIX_C_SOURCE 200809L
#include "exotic/activities.h"
#include "jobs/jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// qsort comparator to sort jobs lexicographically by command name
static int compare_jobs(const void *a, const void *b) {
    Job *job_a = (Job *)a;
    Job *job_b = (Job *)b;
    return strcmp(job_a->command, job_b->command);
}

int activities_command(int argc, char **argv) {
    (void)argc; // Unused
    (void)argv; // Unused

    // Get a snapshot of the current jobs
    Job job_snapshot[MAX_JOBS];
    int count = get_job_list(job_snapshot);

    if (count == 0) {
        return 0; // Nothing to print
    }

    // Sort the snapshot
    qsort(job_snapshot, count, sizeof(Job), compare_jobs);

    // Print sorted jobs
    for (int i = 0; i < count; i++) {
        const char *status_str = "Unknown";
        if (job_snapshot[i].status == RUNNING) {
            status_str = "Running";
        } else if (job_snapshot[i].status == STOPPED) {
            status_str = "Stopped";
        }
        printf("[%d] : %s - %s\n", job_snapshot[i].pid, job_snapshot[i].command, status_str);
    }

    return 0;
}