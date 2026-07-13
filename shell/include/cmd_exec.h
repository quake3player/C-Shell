#ifndef CMD_EXEC_H
#define CMD_EXEC_H

void execute_cmd(char *line, int is_background);
int dispatch_command(char *command_segment, int is_background);

#endif // CMD_EXEC_H