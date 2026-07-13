#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include "intrinsics/hop.h"
#include "intrinsics/reveal.h"

// Helper: qsort comparator
static int cmpstr(const void *a, const void *b) {
    const char *sa = *(const char **)a;
    const char *sb = *(const char **)b;
    return strcmp(sa, sb);  // ASCII lexicographic order
}

int reveal_command(int argc, char **argv) {
    int show_all = 0;   // -a
    int long_list = 0;  // -l
    char *target_dir = NULL;

    int seen_path = 0;  // have we already consumed the (~ | . | .. | - | name) part?

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];

        if (!seen_path && strcmp(arg, "-") == 0) {
            // standalone "-" → prev dir
            seen_path = 1;
            const char *prev = get_prev_dir();
            if (!prev) {
                printf("No such directory!\n");
                return 0;
            }
            target_dir = strdup(prev);
            if (!target_dir) { perror("strdup"); return 1; }

        } else if (!seen_path && arg[0] == '-' && arg[1] != '\0') {
            // flags like -a, -l, -al
            for (int j = 1; arg[j] != '\0'; j++) {
                if (arg[j] == 'a') show_all = 1;
                else if (arg[j] == 'l') long_list = 1;
                else {
                    printf("reveal: Invalid flag -%c\n", arg[j]);
                    return 0;
                }
            }

        } else if (!seen_path) {
            // non-flag path arguments (~, ., .., or name)
            seen_path = 1;

            if (strcmp(arg, "~") == 0) {
                target_dir = strdup(get_home_dir());
            } else if (strcmp(arg, ".") == 0) {
                target_dir = strdup(".");
            } else if (strcmp(arg, "..") == 0) {
                target_dir = strdup("..");
            } else {
                target_dir = strdup(arg);
            }

            if (!target_dir) { perror("strdup"); return 1; }

        } else {
            // Already in path phase, but found another arg → invalid
            printf("reveal: Invalid Syntax!\n");
            return 0;
        }
    }

    if (!target_dir) {
        target_dir = strdup(".");  // default current dir
    }


    // --- Open directory ---
    DIR *dir = opendir(target_dir);
    if (!dir) {
        printf("No such directory!\n");
        free(target_dir);
        return 0;
    }

    // --- Collect entries ---
    struct dirent *entry;
    char **entries = NULL;
    size_t count = 0;

    while ((entry = readdir(dir)) != NULL) {
        // skip hidden unless -a
        if (!show_all && entry->d_name[0] == '.') continue;

        entries = realloc(entries, sizeof(char *) * (count + 1));
        if (!entries) {
            perror("realloc");
            closedir(dir);
            free(target_dir);
            return 1;
        }
        entries[count++] = strdup(entry->d_name);
    }
    closedir(dir);

    // --- Sort ---
    qsort(entries, count, sizeof(char *), cmpstr);

    // --- Print ---
    for (size_t i = 0; i < count; i++) {
        printf("%s", entries[i]);
        if (long_list) {
            printf("\n");
        } else {
            if (i < count - 1) printf(" ");
        }
        free(entries[i]);
    }
    if (!long_list && count > 0) printf("\n");

    free(entries);
    free(target_dir);
    return 0;
}
