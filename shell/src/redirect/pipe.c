#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "redirect/pipe.h"
#include "cmd_exec.h"
#include "exotic/signals.h"
#include "jobs/jobs.h"

#define MAX_CMDS 16

static int split_pipeline(char *line, char *commands[MAX_CMDS]) {
    int count = 0;
    char *line_ptr = line;
    while ((commands[count] = strtok_r(line_ptr, "|", &line_ptr)) && count < MAX_CMDS) {
        count++;
    }
    return count;
}

void execute_pipeline(char *line, int is_background) {
    // *** THE FIX IS HERE: Create a copy of the line for job logging ***
    char *full_command_for_job = strdup(line);
    if (!full_command_for_job) {
        perror("strdup");
        return;
    }
    
    char *commands[MAX_CMDS];
    int ncmds = split_pipeline(line, commands);
    if (ncmds < 1) {
        free(full_command_for_job);
        return;
    }

    pid_t pids[MAX_CMDS];
    pid_t pgid = 0;
    int pipefds[2];
    int in_fd = STDIN_FILENO;

    for (int i = 0; i < ncmds; i++) {
        if (i < ncmds - 1) {
            pipe(pipefds);
        }

        pid_t pid = fork();

        if (pid == 0) { // Child
            if (pgid == 0) pgid = getpid();
            setpgid(0, pgid);

            if (in_fd != STDIN_FILENO) {
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }
            if (i < ncmds - 1) {
                dup2(pipefds[1], STDOUT_FILENO);
                close(pipefds[0]);
                close(pipefds[1]);
            }
            
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            
            // Create a mutable copy for dispatch_command
            char *cmd_copy = strdup(commands[i]);
            dispatch_command(cmd_copy, is_background);
            free(cmd_copy);
            exit(127);
        } else { // Parent
            if (pgid == 0) pgid = pid;
            setpgid(pid, pgid);
            pids[i] = pid;

            if (in_fd != STDIN_FILENO) close(in_fd);
            if (i < ncmds - 1) {
                in_fd = pipefds[0];
                close(pipefds[1]);
            }
        }
    }
    
    if (!is_background) {
        g_foreground_pgid = pgid;
        tcsetpgrp(STDIN_FILENO, pgid);
        
        int status;
        int last_pid = pids[ncmds - 1];
        waitpid(last_pid, &status, WUNTRACED);
        
        for (int i = 0; i < ncmds - 1; i++) {
            waitpid(pids[i], NULL, WNOHANG);
        }

        if (WIFSTOPPED(status)) {
            // *** THE FIX IS HERE: Use the pristine copy of the command ***
            add_job(pgid, full_command_for_job, STOPPED);
        }

        tcsetpgrp(STDIN_FILENO, getpgrp());
        g_foreground_pgid = 0;

    } else {
        // *** THE FIX IS HERE: Use the pristine copy of the command ***
        char *bg_command = malloc(strlen(full_command_for_job) + 3);
        sprintf(bg_command, "%s &", full_command_for_job);
        add_job(pgid, bg_command, RUNNING);
        free(bg_command);
    }
    
    // *** THE FIX IS HERE: Free the allocated copy ***
    free(full_command_for_job);
}
