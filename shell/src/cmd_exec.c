#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "cmd_exec.h"
#include "intrinsics/hop.h"
#include "intrinsics/reveal.h"
#include "intrinsics/log.h"
#include "exotic/activities.h"
#include "exotic/ping.h"
#include "exotic/signals.h"
#include "exotic/fg.h"
#include "exotic/bg.h"
#include "jobs/jobs.h"
#include "redirect/input_redirect.h"
#include "redirect/output_redirect.h"
#include "redirect/pipe.h"

// This is the function that launches a single, non-piped command.
int dispatch_command(char *command_segment, int is_background) {
    char *args[64];
    int argc = 0;
    char *input_file = NULL;
    char *output_file = NULL;
    int append_mode = 0;
    
    // Create a pristine copy for the job list and a mutable copy for strtok
    char *full_command_for_job = strdup(command_segment);
    char *command_copy = strdup(command_segment);

    char *tok = strtok(command_copy, " \t\n");
    if (!tok) {
        free(command_copy);
        free(full_command_for_job);
        return 0;
    }

    char *cmd = tok;
    args[argc++] = cmd;

    while ((tok = strtok(NULL, " \t\n"))) {
        if (strcmp(tok, "<") == 0) {
            input_file = strtok(NULL, " \t\n"); // Always use the last input redirection
        } else if (strcmp(tok, ">") == 0) {
            output_file = strtok(NULL, " \t\n"); // Always use the last output redirection
            append_mode = 0;
        } else if (strcmp(tok, ">>") == 0) {
            output_file = strtok(NULL, " \t\n"); // Always use the last output redirection
            append_mode = 1;
        } else {
            args[argc++] = tok;
        }
    }
    args[argc] = NULL;

    int is_intrinsic = (strcmp(cmd, "hop") == 0) || (strcmp(cmd, "reveal") == 0) ||
                       (strcmp(cmd, "log") == 0) || (strcmp(cmd, "activities") == 0) ||
                       (strcmp(cmd, "ping") == 0) || (strcmp(cmd, "fg") == 0) ||
                       (strcmp(cmd, "bg") == 0);

    if (is_intrinsic) {
        int original_stdin = -1, original_stdout = -1;
        int result = 0;

        if (input_file) {
            original_stdin = dup(STDIN_FILENO);
            if (handle_input_redirection(input_file) < 0) {
                if (original_stdin != -1) close(original_stdin);
                free(command_copy);
                free(full_command_for_job);
                return -1;
            }
        }
        if (output_file) {
            original_stdout = dup(STDOUT_FILENO);
            if (handle_output_redirection(output_file, append_mode) < 0) {
                if (original_stdout != -1) close(original_stdout);
                if (original_stdin != -1) {
                    dup2(original_stdin, STDIN_FILENO);
                    close(original_stdin);
                }
                free(command_copy);
                free(full_command_for_job);
                return -1;
            }
        }

        if (strcmp(cmd, "hop") == 0) result = hop_command(argc, args);
        else if (strcmp(cmd, "reveal") == 0) result = reveal_command(argc, args);
        else if (strcmp(cmd, "log") == 0) result = log_command(argc, args);
        else if (strcmp(cmd, "activities") == 0) result = activities_command(argc, args);
        else if (strcmp(cmd, "ping") == 0) result = ping_command(argc, args);
        else if (strcmp(cmd, "fg") == 0) result = fg_command(argc, args);
        else if (strcmp(cmd, "bg") == 0) result = bg_command(argc, args);

        if (original_stdin != -1) {
            dup2(original_stdin, STDIN_FILENO);
            close(original_stdin);
        }
        if (original_stdout != -1) {
            dup2(original_stdout, STDOUT_FILENO);
            close(original_stdout);
        }

        free(command_copy);
        free(full_command_for_job);
        return result;
    }

    // --- External command logic ---
    pid_t pid = fork();
    if (pid == 0) { // Child process
        setpgid(0, 0);
        if (!is_background) { tcsetpgrp(STDIN_FILENO, getpgrp()); }
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        if (input_file && handle_input_redirection(input_file) < 0) exit(1);
        if (output_file && handle_output_redirection(output_file, append_mode) < 0) exit(1);
        execvp(cmd, args);
        printf("Command not found!\n");
        exit(127);
    } else if (pid > 0) { // Parent process
        if (is_background) {
            // For background jobs, append " &" to the command for proper logging
            char *bg_command = malloc(strlen(full_command_for_job) + 3);
            sprintf(bg_command, "%s &", full_command_for_job);
            add_job(pid, bg_command, RUNNING);
            free(bg_command);
        } else {
            g_foreground_pgid = pid;
            int status;
            waitpid(pid, &status, WUNTRACED);
            if (WIFSTOPPED(status)) {
                add_job(pid, full_command_for_job, STOPPED);
            }
            tcsetpgrp(STDIN_FILENO, getpgrp());
            g_foreground_pgid = 0;
        }
    } else {
        perror("fork");
    }

    free(command_copy);
    free(full_command_for_job);
    return 0;
}

void execute_cmd(char *line, int is_background) {
    if (strchr(line, '|')) {
        execute_pipeline(line, is_background);
    } else {
        dispatch_command(line, is_background);
    }
}