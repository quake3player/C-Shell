#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

#define MAX_JOBS 64
#define MAX_CMD_LEN 256

typedef enum { RUNNING, STOPPED, TERMINATED } JobStatus;

typedef struct {
    pid_t pid;
    pid_t pgid;
    int job_id;
    char command[MAX_CMD_LEN];
    JobStatus status;
} Job;

void init_jobs(void);
void add_job(pid_t pgid, const char* command, JobStatus initial_status);
void check_jobs(void);
void print_completed_jobs(void);
void cleanup_jobs(void);
void kill_all_jobs(void);
int get_job_list(Job job_snapshot[MAX_JOBS]);

// --- NEW FUNCTIONS ---
Job* find_job_by_id(int job_id);
Job* find_most_recent_job(void);
int remove_job_by_pgid(pid_t pgid);

#endif // JOBS_H