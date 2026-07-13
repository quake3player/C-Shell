#include "input/prompt.h"
#include "input/input.h"
#include "input/parser.h"
#include "intrinsics/hop.h"
#include "intrinsics/log.h"
#include "jobs/jobs.h"
#include "jobs/execution.h"
#include "exotic/signals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    // Make the shell interactive and take control of the terminal.
    setpgid(0, 0);
    tcsetpgrp(STDIN_FILENO, getpgrp());
    init_signal_handlers();

    init_hop();
    init_log();
    init_jobs();
    
    while (1) {
        show_prompt();
        char *line = read_input();

        if (!line) { // Ctrl+D was pressed (EOF)
            printf("logout\n");
            kill_all_jobs();
            break; 
        }
        
        // Check for completed jobs and print messages before processing the next command
        check_jobs();
        print_completed_jobs();
        
        if (strlen(line) > 0) {
            // Handle log execute command replacement before parsing
            char *processed_line = process_log_execute(line);
            
            if (processed_line == NULL) {
                // Error occurred in process_log_execute (e.g., infinite loop detected)
                // The error is already printed, so just continue.
            } else {
                // A copy is needed for the parser because it's destructive (uses strtok)
                char *line_for_parser = strdup(processed_line);
                if (!line_for_parser) {
                    perror("strdup");
                } else {
                    if (!parse_command(line_for_parser)) {
                        printf("Invalid Syntax!\n");
                    } else {
                        // Log the original, un-expanded command
                        add_to_log(line); 
                        handle_execution_flow(processed_line);
                    }
                    free(line_for_parser);
                }
                free(processed_line);
            }
        }
        free(line);
    }

    cleanup_hop();
    cleanup_log();
    cleanup_jobs();
    return 0;
}