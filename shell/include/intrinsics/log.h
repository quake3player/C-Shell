#ifndef LOG_H
#define LOG_H

void init_log();
void add_to_log(const char *cmd);
int log_command(int argc, char **argv);
void cleanup_log();
char* process_log_execute(const char* line);

#endif