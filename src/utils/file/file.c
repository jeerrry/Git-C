/*
 * file.c
 *
 * File I/O utilities for reading, writing, and git object path resolution.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../constants.h"

int object_path(const char *sha_hex, char *abs_dir, size_t dir_size,
                char *abs_file, size_t file_size) {
    if (sha_hex == NULL || abs_file == NULL) return 1;
    if (strlen(sha_hex) != 40) {
        GIT_ERR("Invalid SHA-1 length: expected 40, got %zu\n", strlen(sha_hex));
        return 1;
    }

    if (abs_dir != NULL) {
        snprintf(abs_dir, dir_size, "%s/%.2s", GIT_OBJECTS_DIR, sha_hex);
    }
    snprintf(abs_file, file_size, "%s/%.2s/%s", GIT_OBJECTS_DIR, sha_hex, sha_hex + 2);

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
    char abs_file[GIT_PATH_MAX];
    if (object_path(sha1_string, NULL, 0, abs_file, sizeof(abs_file)) != 0) {
        return NULL;
    }

    long file_size = 0;
    char *file_content = read_file(abs_file, &file_size);
    if (file_content == NULL) {
        GIT_ERR("Error reading file %s\n", abs_file);
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
