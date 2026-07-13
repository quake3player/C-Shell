#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h> 
#include <stdint.h>
#include <ctype.h>
#include "intrinsics/log.h"
#include "cmd_exec.h"

#define MAX_HISTORY_SIZE 15
#define LOG_FILENAME "/.shell_log"

static char *command_history[MAX_HISTORY_SIZE];
static int history_count = 0;
static char log_filepath[1024];

static void setup_log_filepath() {
    const char *home_dir  = NULL;
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf)) != NULL) {
        home_dir = strdup(buf);
        if (!home_dir) {
            perror("strdup");
            exit(1);
        }
    } else {
        perror("getcwd");
        exit(1);
    }

    if (home_dir) {
        snprintf(log_filepath, sizeof(log_filepath), "%s%s", home_dir, LOG_FILENAME);
    } else {
        strcpy(log_filepath, ".shell_log");
    }
}

void init_log() {
    setup_log_filepath();
    FILE *log_file = fopen(log_filepath, "r");
    if (!log_file) return;

    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, log_file) != -1 && history_count < MAX_HISTORY_SIZE) {
        line[strcspn(line, "\n")] = 0; // Strip newline
        command_history[history_count] = strdup(line);
        history_count++;
    }
    free(line);
    fclose(log_file);
}

void add_to_log(const char *command) {
    if (strncmp(command, "log", 3) == 0 && (command[3] == ' ' || command[3] == '\0')) {
        return;
    }
    if (!command || !*command ||
        (history_count > 0 && strcmp(command, command_history[history_count - 1]) == 0)) {
        return;
    }
    if (history_count == MAX_HISTORY_SIZE) {
        free(command_history[0]);
        for (int i = 0; i < MAX_HISTORY_SIZE - 1; i++) {
            command_history[i] = command_history[i + 1];
        }
        history_count--;
    }
    command_history[history_count] = strdup(command);
    history_count++;
}

void cleanup_log() {
    FILE *log_file = fopen(log_filepath, "w");
    if (!log_file) {
        perror("Could not save command history");
        return;
    }
    for (int i = 0; i < history_count; i++) {
        fprintf(log_file, "%s\n", command_history[i]);
        free(command_history[i]);
    }
    fclose(log_file);
}

int log_command(int argc, char **argv) {
    if (argc == 1) { // log
        for (int i = 0; i < history_count; i++) {
            printf("%s\n", command_history[i]);
        }
        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "purge") == 0) { // log purge
        for (int i = 0; i < history_count; i++) {
            free(command_history[i]);
            command_history[i] = NULL;
        }
        history_count = 0;
        remove(log_filepath);
        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "execute") == 0) { // log execute <index>
        int index = atoi(argv[2]);
        if (index < 1 || index > history_count) {
            printf("Invalid index!\n");
            return 1;
        }

        int real_index = history_count - index;
        char *cmd_to_run = command_history[real_index];
        
        execute_cmd(cmd_to_run, 0);
        return 0;
    }

    printf("Invalid log command.\n");
    return 1;
}

char* replace_log_execute(const char* command) {
    const char* pattern = "log execute ";
    char* occurrence = strstr(command, pattern);

    if (!occurrence) {
        return NULL; // No "log execute" found, replacement is done.
    }

    // Isolate the part of the string before "log execute"
    size_t prefix_len = occurrence - command;
    char* prefix = (char*)malloc(prefix_len + 1);
    if (!prefix) {
        perror("malloc");
        return NULL;
    }
    strncpy(prefix, command, prefix_len);
    prefix[prefix_len] = '\0';

    // Parse the index
    char* index_start = occurrence + strlen(pattern);
    char* index_end;
    long index = strtol(index_start, &index_end, 10);

    // Validate the index
    if (index_start == index_end || index < 1 || index > history_count) {
        fprintf(stderr, "Error: Invalid log index.\n");
        free(prefix);
        return NULL; // Stop processing on invalid index
    }

    // Get the command from history (1-based index from most recent)
    const char* history_cmd = command_history[history_count - index];

    // Skip leading whitespace in the rest of the string
    while (isspace((unsigned char)*index_end)) {
        index_end++;
    }
    const char* suffix = index_end;

    // Allocate memory for the new command string
    size_t new_len = strlen(prefix) + strlen(history_cmd) + strlen(suffix) + 1;
    char* new_command = (char*)malloc(new_len);
    if (!new_command) {
        perror("malloc");
        free(prefix);
        return NULL;
    }

    // Construct the new command
    snprintf(new_command, new_len, "%s%s%s", prefix, history_cmd, suffix);

    free(prefix);
    return new_command;
}

char* process_log_execute(const char* command) {
    char* current_command = strdup(command);
    if (!current_command) {
        perror("strdup");
        return NULL;
    }

    char* next_command;
    int replacements = 0;
    const int MAX_REPLACEMENTS = 10; // Safeguard against infinite loops

    while ((next_command = replace_log_execute(current_command)) != NULL) {
        free(current_command);
        current_command = next_command;
        replacements++;
        if (replacements > MAX_REPLACEMENTS) {
            fprintf(stderr, "Error: Exceeded maximum log execute replacements, possible infinite loop\n");
            free(current_command);
            return NULL;
        }
    }

    return current_command;
}