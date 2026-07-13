/*
we use this POSIX to get MAX_PATH or getcwd() 
unistd gives us access to unix-system func calls like chdir etc
errno - better error handling , has sytem codes for failed syscalls
limits - PATH_MAX etc
*/


#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h> 
#include "intrinsics/hop.h"

static char *home_dir = NULL;
static char *prev_dir = NULL;

void init_hop() {
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
    prev_dir = NULL;
}

// In src/hop.c

static void change_dir(const char *path) {
    char current_dir_buf[PATH_MAX];

    // First, get the CWD so we can save it if chdir succeeds.
    if (getcwd(current_dir_buf, sizeof(current_dir_buf)) == NULL) {
        perror("getcwd");
        return; // Don't proceed if we can't get the current path
    }

    if (chdir(path) == 0) {
        // SUCCESS: The directory was changed. Now we can update prev_dir.
        free(prev_dir);
        prev_dir = strdup(current_dir_buf);
        if (!prev_dir) {
            perror("strdup");
            exit(1);
        }
    } else {
        printf("No such directory!\n");
    }
}

int hop_command(int argc, char **argv) {
    if (argc == 1) {
        change_dir(home_dir);
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "~") == 0) {
            change_dir(home_dir);
        } else if (strcmp(arg, ".") == 0) {
            
        } else if (strcmp(arg, "..") == 0) {
            change_dir("..");
        } else if (strcmp(arg, "-") == 0) {
            if (prev_dir != NULL) {
                change_dir(prev_dir);
            }
        } else {
            change_dir(arg);
        }
    }
    return 0;
}

void cleanup_hop() {
    free(home_dir);
    free(prev_dir);
    home_dir = prev_dir = NULL;
}

const char *get_home_dir() {
    return home_dir;
}

const char *get_prev_dir() {
    return prev_dir;
}

/*
at any time we only want to store prev_dir because we already have the current dir from getcwd
*/