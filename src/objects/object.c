/*
 * object.c
 *
 * Implements the shared read → decompress → parse-header pipeline
 * for git objects stored in .git/objects/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../constants.h"
#include "../utils/file/file.h"
#include "../utils/compression/compression.h"
#include "object.h"

int object_read(const char *sha1, GitObject *out) {
    long compressed_size;
    char *compressed = read_git_blob_file(sha1, &compressed_size);
    if (compressed == NULL) {
        GIT_ERR("Error reading object %s\n", sha1);
        return 1;
    }

    unsigned long raw_size;
    unsigned char *raw = decompress_data((unsigned char *)compressed, compressed_size, &raw_size);
    free(compressed);
    if (raw == NULL) {
        GIT_ERR("Error decompressing object %s\n", sha1);
        return 1;
    }

    /* Git objects: "<type> <size>\0<content>" — find the null separator */
    unsigned char *header_end = memchr(raw, '\0', raw_size);
    if (header_end == NULL) {
        GIT_ERR("Malformed git object header in %s\n", sha1);
        free(raw);
        return 1;
    }

    out->raw = raw;
    out->body = header_end + 1;
    out->body_size = raw_size - (unsigned long)(out->body - raw);
    return 0;
}
