/*
 * commands.c
 *
 * Implements the core git subcommands: init, cat-file, hash-object,
 * ls-tree, write-tree. Each public function returns 0 on success, 1 on failure.
 *
 * Internal helpers use the goto-cleanup pattern and return heap-allocated
 * results (NULL on failure) so callers can compose them without leaks.
 * write_object_to_store and create_blob factor out the shared pipelines
 * that hash_object and write_tree both need.
 */

#include <dirent.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zconf.h>
#include <zlib.h>
#include <openssl/sha.h>

#include "../constants.h"
#include "../objects/object.h"
#include "../utils/file/file.h"
#include "../utils/compression/compression.h"
#include "../utils/string/string.h"
#include "../utils/directory/directory.h"

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

int cat_file(const char *sha1) {
    GitObject obj;
    if (object_read(sha1, &obj) != 0) return 1;

    printf("%.*s", (int)obj.body_size, obj.body);
    free(obj.raw);
    return 0;
}

/*
 * Writes a complete git object to .git/objects/.
 * Takes already-formatted object data (e.g. "blob 12\0..." or "tree 95\0..."),
 * computes SHA-1, compresses with zlib, and writes to the object store.
 *
 * Both hash_object and write_tree share this pipeline — extracting it
 * avoids duplicating the hash → compress → write sequence.
 *
 * Returns heap-allocated 40-char hex hash (caller must free), or NULL.
 */
static char *write_object_to_store(const char *object_data, size_t object_size) {
    char *str_hash = NULL;
    char *obj_path = NULL;
    char *dir_name = NULL;
    char *file_name = NULL;
    unsigned char *compressed = NULL;

    unsigned char raw_hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char *)object_data, object_size, raw_hash);
    str_hash = hex_to_string(raw_hash, SHA_DIGEST_LENGTH);
    if (str_hash == NULL) {
        GIT_ERR("Error converting hash to string\n");
        goto fail;
    }

    obj_path = get_file_path(str_hash);
    if (obj_path == NULL) {
        GIT_ERR("Error getting object path\n");
        goto fail;
    }
    if (split_file_path(obj_path, &dir_name, &file_name) == 1) {
        GIT_ERR("Error splitting object path\n");
        goto fail;
    }

    char abs_dir[GIT_PATH_MAX];
    char abs_file[GIT_PATH_MAX];
    snprintf(abs_dir, sizeof(abs_dir), "%s/%s", GIT_OBJECTS_DIR, dir_name);

    if (!directory_exists(abs_dir) && mkdir(abs_dir, DIRECTORY_PERMISSION) == -1) {
        GIT_ERR("Error creating directory %s\n", dir_name);
        goto fail;
    }

    snprintf(abs_file, sizeof(abs_file), "%s/%s/%s", GIT_OBJECTS_DIR, dir_name, file_name);

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

    free(obj_path);
    free(dir_name);
    free(file_name);
    free(compressed);
    return str_hash;

fail:
    free(str_hash);
    free(obj_path);
    free(dir_name);
    free(file_name);
    free(compressed);
    return NULL;
}

/*
 * Creates a blob object for a file and returns its SHA.
 * Same pipeline as hash_object but returns the hash instead of printing —
 * write_tree uses this to collect SHAs without polluting stdout.
 *
 * Returns heap-allocated 40-char hex hash (caller must free), or NULL.
 */
static char *create_blob(const char *path) {
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

    char *sha_hex = write_object_to_store(blob_data, total_size);
    free(blob_data);
    return sha_hex;
}

int hash_object(const char *path) {
    char *sha_hex = create_blob(path);
    if (sha_hex == NULL) return 1;

    printf("%s\n", sha_hex);
    free(sha_hex);
    return 0;
}

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

/* Holds one parsed directory entry before we pack it into binary tree format. */
typedef struct {
    char mode[7];       /* "100644" or "40000" + null */
    char *name;         /* heap-allocated filename (caller frees via cleanup) */
    char sha_hex[41];   /* 40-char hex SHA + null */
} TreeEntry;

/* Lexicographic comparator for qsort — git requires sorted tree entries. */
static int compare_tree_entries(const void *a, const void *b) {
    return strcmp(((const TreeEntry *)a)->name, ((const TreeEntry *)b)->name);
}

/*
 * Recursively builds tree objects from a directory's contents.
 * For files: creates a blob via create_blob.
 * For subdirectories: recurses to build a child tree first.
 * Entries are sorted alphabetically — git requires this for
 * deterministic hashing (same directory = same tree SHA).
 *
 * Returns heap-allocated 40-char hex hash (caller must free), or NULL.
 */
static char *write_tree_recursive(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        GIT_ERR("Error opening directory %s: %s\n", dir_path, strerror(errno));
        return NULL;
    }

    TreeEntry *entries = NULL;
    size_t entry_count = 0;
    size_t entry_capacity = 0;
    char *result_sha = NULL;

    struct dirent *dentry;
    while ((dentry = readdir(dir)) != NULL) {
        if (strcmp(dentry->d_name, ".") == 0 || strcmp(dentry->d_name, "..") == 0)
            continue;
        /* .git is git's internal storage — never include it in tree objects */
        if (strcmp(dentry->d_name, ".git") == 0)
            continue;

        char full_path[GIT_PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, dentry->d_name);

        struct stat st;
        if (stat(full_path, &st) == -1) {
            GIT_ERR("Error stat %s: %s\n", full_path, strerror(errno));
            goto cleanup;
        }

        /* Grow entries array using doubling strategy to amortize realloc cost */
        if (entry_count >= entry_capacity) {
            entry_capacity = entry_capacity == 0 ? 8 : entry_capacity * 2;
            TreeEntry *new_entries = realloc(entries, entry_capacity * sizeof(TreeEntry));
            if (new_entries == NULL) {
                GIT_ERR("Error allocating memory for tree entries\n");
                goto cleanup;
            }
            entries = new_entries;
        }

        TreeEntry *te = &entries[entry_count];
        te->name = strdup(dentry->d_name);
        if (te->name == NULL) {
            GIT_ERR("Error duplicating entry name\n");
            goto cleanup;
        }

        char *sha = NULL;
        if (S_ISREG(st.st_mode)) {
            strcpy(te->mode, "100644");
            sha = create_blob(full_path);
        } else if (S_ISDIR(st.st_mode)) {
            strcpy(te->mode, "40000");
            sha = write_tree_recursive(full_path);
        } else {
            /* Skip symlinks, pipes, sockets, etc. */
            free(te->name);
            continue;
        }

        if (sha == NULL) {
            free(te->name);
            goto cleanup;
        }
        memcpy(te->sha_hex, sha, 40);
        te->sha_hex[40] = '\0';
        free(sha);

        entry_count++;
    }

    closedir(dir);
    dir = NULL;

    /* Git requires tree entries sorted by name for deterministic hashing */
    if (entry_count > 1) {
        qsort(entries, entry_count, sizeof(TreeEntry), compare_tree_entries);
    }

    /* Calculate body size: each entry is mode + space + name + null + 20 SHA bytes */
    size_t body_size = 0;
    for (size_t i = 0; i < entry_count; i++) {
        body_size += strlen(entries[i].mode) + 1 + strlen(entries[i].name) + 1 + 20;
    }

    /* Build "tree <body_size>\0<packed entries>" */
    int header_len_int = snprintf(NULL, 0, "tree %zu", body_size);
    if (header_len_int < 0) {
        GIT_ERR("Error formatting tree header\n");
        goto cleanup;
    }
    size_t header_len = (size_t)header_len_int;
    size_t total_size = header_len + 1 + body_size;
    char *tree_data = malloc(total_size);
    if (tree_data == NULL) {
        GIT_ERR("Error allocating memory for tree data\n");
        goto cleanup;
    }

    snprintf(tree_data, header_len + 1, "tree %zu", body_size);
    tree_data[header_len] = '\0';

    /* Pack entries into binary format: <mode> <name>\0<20 raw SHA bytes> */
    char *write_pos = tree_data + header_len + 1;
    for (size_t i = 0; i < entry_count; i++) {
        size_t mode_len = strlen(entries[i].mode);
        size_t name_len = strlen(entries[i].name);

        memcpy(write_pos, entries[i].mode, mode_len);
        write_pos += mode_len;
        *write_pos++ = ' ';

        memcpy(write_pos, entries[i].name, name_len);
        write_pos += name_len;
        *write_pos++ = '\0';

        /* Convert 40-char hex SHA to 20 raw bytes for binary tree format */
        size_t sha_byte_len;
        unsigned char *sha_bytes = hex_string_to_bytes(entries[i].sha_hex, &sha_byte_len);
        if (sha_bytes == NULL) {
            free(tree_data);
            goto cleanup;
        }
        memcpy(write_pos, sha_bytes, 20);
        write_pos += 20;
        free(sha_bytes);
    }

    result_sha = write_object_to_store(tree_data, total_size);
    free(tree_data);

cleanup:
    if (dir != NULL) closedir(dir);
    for (size_t i = 0; i < entry_count; i++) {
        free(entries[i].name);
    }
    free(entries);
    return result_sha;
}

int write_tree(void) {
    char *sha_hex = write_tree_recursive(".");
    if (sha_hex == NULL) return 1;

    printf("%s\n", sha_hex);
    free(sha_hex);
    return 0;
}
