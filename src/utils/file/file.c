/*
 * file.c
 *
 * File I/O utilities for reading, writing, and path manipulation.
 * Handles the git object store path convention where SHA-1 hashes
 * are split into a 2-char directory prefix and 38-char filename.
 */

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
    #include <sys/stat.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../constants.h"
#include "../string/string.h"

char *get_file_path(const char *sha1_hex) {
    const size_t sha_length = strlen(sha1_hex);

    /* Reject invalid SHA-1 strings early to prevent UB from
     * short strings and path traversal from crafted input. */
    if (sha_length != 40) {
        GIT_ERR("Invalid SHA-1 length: expected 40, got %zu\n", sha_length);
        return NULL;
    }

    /* Git shards objects by the first two hex digits to avoid
     * filesystem limits on directory entry counts (e.g., ext4 ~10M).
     * Result format: "ab/cdef0123..." from "abcdef0123..." */
    char *file_path = malloc(sha_length + 2); /* +1 for '/' inserted, +1 for '\0' */
    if (file_path == NULL) {
        GIT_ERR("malloc failed\n");
        return NULL;
    }

    memcpy(file_path, sha1_hex, 2);
    file_path[2] = '/';
    memcpy(file_path + 3, sha1_hex + 2, sha_length - 2 + 1); /* +1 for '\0' */

    return file_path;
}

int split_file_path(const char *file_path, char **directory, char **file_name) {
    /* Pre-initialize so callers can safely free() on any error path */
    *directory = NULL;
    *file_name = NULL;

    const char *last_slash = strrchr(file_path, '/');
    if (last_slash == NULL) {
        GIT_ERR("File path is invalid\n");
        return 1;
    }

    size_t directory_len = (size_t)(last_slash - file_path);
    size_t file_name_len = strlen(last_slash + 1);

    *directory = malloc(directory_len + 1);
    if (*directory == NULL) {
        GIT_ERR("Failed to allocate memory for directory\n");
        return 1;
    }

    *file_name = malloc(file_name_len + 1);
    if (*file_name == NULL) {
        GIT_ERR("Failed to allocate memory for file name\n");
        free(*directory);
        *directory = NULL;
        return 1;
    }

    memcpy(*directory, file_path, directory_len);
    (*directory)[directory_len] = '\0';

    memcpy(*file_name, last_slash + 1, file_name_len);
    (*file_name)[file_name_len] = '\0';

    return 0;
}

char *read_file(const char *file_absolute_path, long *file_size) {
    FILE *target_file = fopen(file_absolute_path, "rb");
    if (target_file == NULL) {
        GIT_ERR("Error opening file %s\n", file_absolute_path);
        return NULL;
    }

    if (fseek(target_file, 0, SEEK_END) != 0) {
        GIT_ERR("Error seeking end of file %s\n", file_absolute_path);
        fclose(target_file);
        return NULL;
    }

    const long target_file_size = ftell(target_file);
    if (target_file_size < 0) {
        GIT_ERR("Error getting file size for %s\n", file_absolute_path);
        fclose(target_file);
        return NULL;
    }

    char *file_content = malloc(target_file_size + 1);
    if (file_content == NULL) {
        GIT_ERR("Error allocating memory for file content\n");
        fclose(target_file);
        return NULL;
    }

    if (fseek(target_file, 0, SEEK_SET) != 0) {
        GIT_ERR("Error seeking to beginning of file %s\n", file_absolute_path);
        fclose(target_file);
        free(file_content);
        return NULL;
    }

    size_t file_bytes_read = fread(file_content, sizeof(char), target_file_size, target_file);
    if (file_bytes_read != (size_t)target_file_size) {
        GIT_ERR("Error reading file %s\n", file_absolute_path);
        fclose(target_file);
        free(file_content);
        return NULL;
    }

    *file_size = target_file_size;
    file_content[target_file_size] = '\0';
    fclose(target_file);

    return file_content;
}

char *read_git_blob_file(const char *sha1_string, long *compressed_size) {
    char *file_relative_path = get_file_path(sha1_string);
    if (file_relative_path == NULL) {
        return NULL;
    }

    /* Fixed-size buffer instead of VLA — C23 makes VLAs optional,
     * and user-controlled sha1 length could cause stack overflow. */
    char file_absolute_path[GIT_PATH_MAX];
    snprintf(file_absolute_path, sizeof(file_absolute_path), "%s/%s", GIT_OBJECTS_DIR, file_relative_path);
    free(file_relative_path);

    long file_size = 0;
    char *file_content = read_file(file_absolute_path, &file_size);
    if (file_content == NULL) {
        GIT_ERR("Error reading file %s\n", file_absolute_path);
        return NULL;
    }

    *compressed_size = file_size;

    return file_content;
}

int write_file(const char *file_absolute_path, const char *data, size_t byte_count, const char *mode) {
    FILE *target_file = fopen(file_absolute_path, mode);
    if (target_file == NULL) {
        GIT_ERR("Error opening file %s\n", file_absolute_path);
        return 1;
    }

    size_t written = fwrite(data, sizeof(char), byte_count, target_file);
    if (written != byte_count) {
        GIT_ERR("Error writing file %s: wrote %zu of %zu bytes\n", file_absolute_path, written, byte_count);
        fclose(target_file);
        return 1;
    }

    if (fclose(target_file) != 0) {
        GIT_ERR("Error closing file %s\n", file_absolute_path);
        return 1;
    }

    return 0;
}
