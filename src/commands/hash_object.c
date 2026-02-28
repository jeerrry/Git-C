/*
 * hash_object.c
 *
 * Implements the "git hash-object -w" command — creates a blob
 * object from a file and prints its SHA-1 hash to stdout.
 */

#include <stdio.h>
#include <stdlib.h>

#include "../objects/object.h"

int hash_object(const char *path) {
    char *sha_hex = create_blob(path);
    if (sha_hex == NULL) return 1;

    printf("%s\n", sha_hex);
    free(sha_hex);
    return 0;
}
