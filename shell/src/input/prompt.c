#define _POSIX_C_SOURCE 200809L
#include "input/prompt.h"
#include "intrinsics/hop.h" // *** CHANGED: Include hop.h to get the shared home_dir
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <string.h>
#include <limits.h>

void show_prompt() {
    // Username
    struct passwd *pw = getpwuid(getuid());
    const char *username = pw ? pw->pw_name : "unknown";

    // Hostname
    char hostname[HOST_NAME_MAX + 1];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "unknown");
    }

    // Current working directory
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        strcpy(cwd, "?");
    }
    
    // *** FIX: Get the canonical home directory from the hop module to ensure consistency. ***
    const char *home_dir = get_home_dir();
    const char *display_path = cwd;

    if (home_dir) {
        size_t home_len = strlen(home_dir);
        // *** FIX: Stricter check to ensure it's a real subdirectory, not a partial name match. ***
        if (strncmp(cwd, home_dir, home_len) == 0 && (cwd[home_len] == '/' || cwd[home_len] == '\0')) {
            static char path_buf[PATH_MAX];
            snprintf(path_buf, sizeof(path_buf), "~%s", cwd + home_len);
            display_path = path_buf;
        }
    }

    printf("<%s@%s:%s> ", username, hostname, display_path);
    fflush(stdout);
}