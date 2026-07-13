#define _POSIX_C_SOURCE 200809L
#include "jobs/jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

static Job job_list[MAX_JOBS];
static int next_job_id = 1;
static char *completed_jobs_buffer[MAX_JOBS];
static int completed_count = 0;

void init_jobs() {
    for (int i = 0; i < MAX_JOBS; i++) {
        job_list[i].pgid = 0;
    }
}

void add_job(pid_t pgid, const char* command, JobStatus initial_status) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_list[i].pgid == 0) {
            job_list[i].pgid = pgid;
            job_list[i].pid = pgid;
            job_list[i].job_id = next_job_id++;
            job_list[i].status = initial_status;
            strncpy(job_list[i].command, command, MAX_CMD_LEN - 1);
            job_list[i].command[MAX_CMD_LEN - 1] = '\0';
            if (initial_status == RUNNING) {
                 printf("[%d] %d\n", job_list[i].job_id, job_list[i].pid);
            } else if (initial_status == STOPPED) {
                 printf("\n[%d] Stopped \t%s\n", job_list[i].job_id, job_list[i].command);
            }
            return;
        }
    }
    fprintf(stderr, "Shell error: Maximum jobs reached.\n");
}

// *** THIS FUNCTION HAS BEEN REWRITTEN FOR THE FIX ***
void check_jobs() {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_list[i].pgid == 0) {
            continue; // Skip inactive job slots
        }

        int status;
        pid_t child_pid;

        // Repeatedly check the job's process group, reaping any children that have
        // terminated. This handles pipelines where multiple processes finish.
        while ((child_pid = waitpid(-job_list[i].pgid, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
            if (WIFSTOPPED(status)) {
                job_list[i].status = STOPPED;
            } else if (WIFCONTINUED(status)) {
                job_list[i].status = RUNNING;
            } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
                // Process has terminated
                char buffer[MAX_CMD_LEN + 50];
                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    snprintf(buffer, sizeof(buffer), "%s with pid %d exited normally", job_list[i].command, job_list[i].pid);
                } else {
                    snprintf(buffer, sizeof(buffer), "%s with pid %d exited abnormally", job_list[i].command, job_list[i].pid);
                }
                if (completed_count < MAX_JOBS) {
                    completed_jobs_buffer[completed_count++] = strdup(buffer);
                }
                job_list[i].pgid = 0; // Mark the slot as free.
                break; // Exit the while loop since this job is done
            }
        }

        // After the loop, if waitpid failed with ECHILD, it means all processes
        // in the group have been reaped and the job is truly done.
        if (child_pid < 0 && errno == ECHILD && job_list[i].pgid != 0) {
            // The job is done but we didn't catch the exit status above
            char buffer[MAX_CMD_LEN + 50];
            snprintf(buffer, sizeof(buffer), "%s with pid %d exited normally", job_list[i].command, job_list[i].pid);
            if (completed_count < MAX_JOBS) {
                completed_jobs_buffer[completed_count++] = strdup(buffer);
            }
            job_list[i].pgid = 0; // Mark the slot as free.
        }
    }
}

void print_completed_jobs() {
    for (int i = 0; i < completed_count; i++) {
        printf("%s\n", completed_jobs_buffer[i]);
        free(completed_jobs_buffer[i]);
    }
    completed_count = 0;
}

void kill_all_jobs() {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_list[i].pgid != 0) {
            kill(-job_list[i].pgid, SIGKILL);
        }
    }
}

int get_job_list(Job job_snapshot[MAX_JOBS]) {
    int count = 0;
    for (int i = 0; i < MAX_JOBS && count < MAX_JOBS; i++) {
        if (job_list[i].pgid != 0) {
            memcpy(&job_snapshot[count], &job_list[i], sizeof(Job));
            count++;
        }
    }
    return count;
}

void cleanup_jobs() {
    check_jobs();
    print_completed_jobs();
}

Job* find_job_by_id(int job_id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_list[i].pgid != 0 && job_list[i].job_id == job_id) {
            return &job_list[i];
        }
    }
    return NULL;
}

Job* find_most_recent_job(void) {
    int max_job_id = -1;
    Job* most_recent_job = NULL;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_list[i].pgid != 0 && job_list[i].job_id > max_job_id) {
            max_job_id = job_list[i].job_id;
            most_recent_job = &job_list[i];
        }
    }
    return most_recent_job;
}

int remove_job_by_pgid(pid_t pgid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_list[i].pgid == pgid) {
            job_list[i].pgid = 0;
            return 1;
        }
    }
    return 0;
}