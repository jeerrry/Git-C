/*
 * commands.c
 *
 * Implements the core git subcommands: init, cat-file, hash-object.
 * Each function returns 0 on success, 1 on failure.
 *
 * hash_object uses the goto-cleanup pattern to ensure all heap
 * allocations are freed through a single exit path, preventing
 * leaks on any error condition.
 */

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
    #include <sys/stat.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zconf.h>
#include <zlib.h>
#include <openssl/sha.h>

#include "../constants.h"
#include "../utils/file/file.h"
#include "../utils/compression/compression.h"
#include "../utils/string/string.h"
#include "../utils/directory/directory.h"

int int_git(void) {
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

int cat_file(const char *sha1) {
    unsigned long uncompressed_size;
    long compressed_size;

    char *compressed_file = read_git_blob_file(sha1, &compressed_size);
    if (compressed_file == NULL) {
        GIT_ERR("Error reading object %s\n", sha1);
        return 1;
    }

    unsigned char *decompressed_file = decompress_data((unsigned char *)compressed_file, compressed_size, &uncompressed_size);
    if (decompressed_file == NULL) {
        GIT_ERR("Error decompressing object %s\n", sha1);
        free(compressed_file);
        return 1;
    }

    /* Git objects are formatted as "<type> <size>\0<content>".
     * Find the null separator to skip the header and print the body. */
    unsigned char *header_end = memchr(decompressed_file, '\0', uncompressed_size);
    if (header_end == NULL) {
        GIT_ERR("Malformed git object header in %s\n", sha1);
        free(compressed_file);
        free(decompressed_file);
        return 1;
    }

    unsigned char *body_content = header_end + 1;
    const unsigned long body_size = uncompressed_size - (body_content - decompressed_file);
    printf("%.*s", (int) body_size, body_content);

    free(compressed_file);
    free(decompressed_file);

    return 0;
}

int hash_object(const char *path) {
    int ret = 1;
    long file_size;
    char *file_content = NULL;
    char *file_data_to_compress = NULL;
    char *str_hash_output = NULL;
    char *file_path = NULL;
    char *dir_name = NULL;
    char *file_name = NULL;
    unsigned char *compressed_data = NULL;

    file_content = read_file(path, &file_size);
    if (file_content == NULL) {
        GIT_ERR("Error reading file %s\n", path);
        goto cleanup;
    }

    /* Git blob format: "blob <size>\0<content>"
     * The null byte after the header is part of the format — it separates
     * the header from the content and is included in the SHA-1 hash.
     * snprintf(NULL, 0, ...) safely calculates the header length first. */
    size_t header_length = (size_t)snprintf(NULL, 0, "blob %ld", file_size);
    size_t total_size = header_length + 1 + file_size; /* +1 for the null separator */
    file_data_to_compress = malloc(total_size);
    if (file_data_to_compress == NULL) {
        GIT_ERR("Error allocating memory for blob data\n");
        goto cleanup;
    }
    snprintf(file_data_to_compress, header_length + 1, "blob %ld", file_size);
    file_data_to_compress[header_length] = '\0';
    memcpy(file_data_to_compress + header_length + 1, file_content, file_size);

    unsigned char hex_hash_output[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)file_data_to_compress, total_size, hex_hash_output);
    str_hash_output = hex_to_string(hex_hash_output, SHA_DIGEST_LENGTH);
    if (str_hash_output == NULL) {
        GIT_ERR("Error converting hash to string\n");
        goto cleanup;
    }

    printf("%s\n", str_hash_output);

    char absolute_dir_path[GIT_PATH_MAX];
    char absolute_file_path[GIT_PATH_MAX];
    file_path = get_file_path(str_hash_output);
    if (file_path == NULL) {
        GIT_ERR("Error getting file path\n");
        goto cleanup;
    }
    if (split_file_path(file_path, &dir_name, &file_name) == 1) {
        GIT_ERR("Error splitting file path\n");
        goto cleanup;
    }

    snprintf(absolute_dir_path, sizeof(absolute_dir_path), "%s/%s", GIT_OBJECTS_DIR, dir_name);

    if (is_directory_present(absolute_dir_path) == 1 && mkdir(absolute_dir_path, DIRECTORY_PERMISSION) == -1) {
        GIT_ERR("Error creating directory %s\n", dir_name);
        goto cleanup;
    }

    snprintf(absolute_file_path, sizeof(absolute_file_path), "%s/%s/%s", GIT_OBJECTS_DIR, dir_name, file_name);

    unsigned long compressed_size;
    compressed_data = compress_data((unsigned char *)file_data_to_compress, total_size, &compressed_size);
    if (compressed_data == NULL) {
        GIT_ERR("Compression failed\n");
        goto cleanup;
    }

    if (write_file(absolute_file_path, (const char *)compressed_data, compressed_size, "wb") == 1) {
        GIT_ERR("Error writing %s\n", file_name);
        goto cleanup;
    }

    ret = 0;

cleanup:
    free(file_content);
    free(file_data_to_compress);
    free(str_hash_output);
    free(file_path);
    free(dir_name);
    free(file_name);
    free(compressed_data);

    return ret;
}
