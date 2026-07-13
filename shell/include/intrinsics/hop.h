#ifndef HOP_H
#define HOP_H

void init_hop();
int hop_command(int argc, char **argv);
void cleanup_hop();
const char *get_home_dir(); // Getter for home_dir
const char *get_prev_dir(); // Getter for prev_dir

#endif