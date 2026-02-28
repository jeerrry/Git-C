/*
 * write_tree.c
 *
 * Implements the "git write-tree" command — recursively scans the
 * working directory, creates blob/tree objects, and prints the
 * root tree's SHA-1 hash to stdout.
 */

#include <dirent.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../constants.h"
#include "../objects/object.h"
#include "../utils/string/string.h"

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

    result_sha = object_write(tree_data, total_size);
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
