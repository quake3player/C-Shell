#ifndef OUTPUT_REDIRECT_H
#define OUTPUT_REDIRECT_H

// Redirects stdout to file
// append = 0 → overwrite (>), append = 1 → append (>>)
// Returns 0 on success, -1 on failure (and prints error)
int handle_output_redirection(const char *filename, int append);

#endif
