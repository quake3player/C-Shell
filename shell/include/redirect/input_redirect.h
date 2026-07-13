#ifndef INPUT_REDIRECT_H
#define INPUT_REDIRECT_H

// Redirects stdin from file
// Returns 0 on success, -1 on failure (and prints error)
int handle_input_redirection(const char *filename);

#endif
