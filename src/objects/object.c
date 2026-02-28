/*
 * object.c
 *
 * Git object store: read and write pipelines.
 * Read side:  file → decompress → parse header → GitObject
 * Write side: format → SHA-1 → compress → write to .git/objects/
 */

#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>

#include "../constants.h"
#include "../utils/file/file.h"
#include "../utils/compression/compression.h"
#include "../utils/string/string.h"
#include "../utils/directory/directory.h"
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

char *object_write(const char *object_data, size_t object_size) {
    char *str_hash = NULL;
    unsigned char *compressed = NULL;

    unsigned char raw_hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)object_data, object_size, raw_hash);
    str_hash = hex_to_string(raw_hash, SHA_DIGEST_LENGTH);
    if (str_hash == NULL) {
        GIT_ERR("Error converting hash to string\n");
        goto fail;
    }

    char abs_dir[GIT_PATH_MAX];
    char abs_file[GIT_PATH_MAX];
    if (object_path(str_hash, abs_dir, sizeof(abs_dir), abs_file, sizeof(abs_file)) != 0) {
        goto fail;
    }

    if (!directory_exists(abs_dir) && mkdir(abs_dir, DIRECTORY_PERMISSION) == -1) {
        GIT_ERR("Error creating directory %s\n", abs_dir);
        goto fail;
    }

    unsigned long compressed_size;
    compressed = compress_data((unsigned char *)object_data, object_size, &compressed_size);
    if (compressed == NULL) {
        GIT_ERR("Compression failed\n");
        goto fail;
    }

    if (write_file(abs_file, (const char *)compressed, compressed_size, "wb") == 1) {
        GIT_ERR("Error writing object %s\n", str_hash);
        goto fail;
    }

    free(compressed);
    return str_hash;

fail:
    free(str_hash);
    free(compressed);
    return NULL;
}

char *create_blob(const char *path) {
    long file_size;
    char *file_content = read_file(path, &file_size);
    if (file_content == NULL) {
        GIT_ERR("Error reading file %s\n", path);
        return NULL;
    }

    /* snprintf(NULL, 0, ...) safely calculates the header length
     * before allocating, avoiding any fixed-size buffer guesses. */
    int header_len_int = snprintf(NULL, 0, "blob %ld", file_size);
    if (header_len_int < 0) {
        GIT_ERR("Error formatting blob header\n");
        free(file_content);
        return NULL;
    }
    size_t header_len = (size_t)header_len_int;
    size_t total_size = header_len + 1 + file_size;
    char *blob_data = malloc(total_size);
    if (blob_data == NULL) {
        GIT_ERR("Error allocating memory for blob data\n");
        free(file_content);
        return NULL;
    }

    /* Git blob format: "blob <size>\0<content>" */
    snprintf(blob_data, header_len + 1, "blob %ld", file_size);
    blob_data[header_len] = '\0';
    memcpy(blob_data + header_len + 1, file_content, file_size);
    free(file_content);

    char *sha_hex = object_write(blob_data, total_size);
    free(blob_data);
    return sha_hex;
}
