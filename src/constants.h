/*
 * constants.h
 *
 * Compile-time constants shared across the git implementation.
 * GIT_PATH_MAX exists alongside the system PATH_MAX guard because
 * some platforms don't define PATH_MAX (POSIX leaves it optional).
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdio.h>

/* Git directory structure paths */
#define GIT_ROOT_DIR ".git"
#define GIT_REFS_DIR ".git/refs"
#define GIT_OBJECTS_DIR ".git/objects"

/* Buffer and path limits */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define GIT_PATH_MAX 4096
#define FILE_BUFFER_SIZE 4096
#define DIRECTORY_PERMISSION 0755

/* All diagnostic output goes through this macro.
 * Keeps error output on stderr consistently across the codebase. */
#define GIT_ERR(...) fprintf(stderr, __VA_ARGS__)

#endif /* CONSTANTS_H */
