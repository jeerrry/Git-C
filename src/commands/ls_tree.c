/*
 * ls_tree.c
 *
 * Implements the "git ls-tree --name-only" command — reads a tree
 * object and prints each entry's filename to stdout.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../objects/object.h"

int ls_tree(const char *sha1) {
    GitObject obj;
    if (object_read(sha1, &obj) != 0) return 1;

    unsigned char *pos = obj.body;
    unsigned char *end = obj.body + obj.body_size;

    /* Tree entries are packed as: <mode> <name>\0<20-byte binary SHA>
     * with no separator between entries — the only way to find entry
     * boundaries is by locating \0 and skipping exactly 20 bytes. */
    while (pos < end) {
        unsigned char *space = memchr(pos, ' ', (size_t)(end - pos));
        if (space == NULL) break;

        unsigned char *name_start = space + 1;
        unsigned char *name_end = memchr(name_start, '\0', (size_t)(end - name_start));
        if (name_end == NULL) break;

        printf("%.*s\n", (int)(name_end - name_start), name_start);

        if (name_end + 1 + 20 > end) break;
        pos = name_end + 1 + 20;
    }

    free(obj.raw);
    return 0;
}
