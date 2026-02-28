/*
 * cat_file.c
 *
 * Implements the "git cat-file -p" command — reads a git object
 * from the store and prints its content to stdout.
 */

#include <stdio.h>
#include <stdlib.h>

#include "../objects/object.h"

int cat_file(const char *sha1) {
    GitObject obj;
    if (object_read(sha1, &obj) != 0) return 1;

    printf("%.*s", (int)obj.body_size, obj.body);
    free(obj.raw);
    return 0;
}
