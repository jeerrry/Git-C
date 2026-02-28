/*
 * directory.c
 *
 * Directory existence checking using stat().
 */

#include <stdio.h>
#include <sys/stat.h>

#include "../../constants.h"

/* Returns 0 if directory exists, 1 otherwise.
 * Note: inverted from typical boolean convention. */
int is_directory_present(const char *path) {
    if (path == NULL) {
        GIT_ERR("Provided directory path is NULL\n");
        return 1;
    }

    struct stat statbuf;
    if (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
        return 0;
    }

    return 1;
}
