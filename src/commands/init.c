/*
 * init.c
 *
 * Implements the "git init" command — creates the .git/ directory
 * structure and writes the default HEAD reference.
 */

#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "../constants.h"

int init_git(void) {
    if (mkdir(GIT_ROOT_DIR, DIRECTORY_PERMISSION) == -1
        || mkdir(GIT_REFS_DIR, DIRECTORY_PERMISSION) == -1
        || mkdir(GIT_OBJECTS_DIR, DIRECTORY_PERMISSION) == -1) {
        GIT_ERR("Failed to create directories: %s\n", strerror(errno));
        return 1;
    }

    FILE *headFile = fopen(".git/HEAD", "w");
    if (headFile == NULL) {
        GIT_ERR("Failed to create .git/HEAD file: %s\n", strerror(errno));
        return 1;
    }
    fprintf(headFile, "ref: refs/heads/main\n");
    fclose(headFile);

    printf("Initialized git directory\n");

    return 0;
}
