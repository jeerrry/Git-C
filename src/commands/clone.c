/*
 * clone.c
 *
 * Implements the "git clone" command — clones a remote repository
 * via the smart HTTP protocol.
 *
 * Pipeline:
 *   1. Create target directory and init .git/
 *   2. GET refs → extract HEAD SHA
 *   3. POST upload-pack with "want" request → get packfile response
 *   4. Strip side-band framing → raw packfile
 *   5. Parse packfile → write all objects to .git/objects/
 *   6. Read HEAD commit → tree → recursively checkout working directory
 */

#include <sys/stat.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../constants.h"
#include "commands.h"
#include "../objects/object.h"
#include "../utils/string/string.h"
#include "../utils/file/file.h"
#include "../net/http.h"
#include "../net/pktline.h"
#include "../pack/packfile.h"

/*
 * Extracts the tree SHA from a commit object body.
 *
 * Commit body format:
 *   tree <40-char SHA>\n
 *   parent <sha>\n     (optional)
 *   author ...\n
 *   ...
 *
 * The tree SHA is always the first line, characters 5-44.
 */
static int get_tree_sha(const char *commit_sha, char *tree_sha_out) {
    GitObject obj;
    if (object_read(commit_sha, &obj) != 0) return 1;

    /* Verify the body starts with "tree " */
    if (obj.body_size < 45 || memcmp(obj.body, "tree ", 5) != 0) {
        GIT_ERR("clone: malformed commit object %s\n", commit_sha);
        free(obj.raw);
        return 1;
    }

    memcpy(tree_sha_out, obj.body + 5, 40);
    tree_sha_out[40] = '\0';
    free(obj.raw);
    return 0;
}

/*
 * Recursively checks out a tree object into a directory.
 *
 * Walks the tree's binary entries. For each entry:
 *   - Mode "40000" (directory): create subdirectory, recurse
 *   - Other modes (files): read the blob and write it to disk
 *
 * Tree entry format: <mode> <name>\0<20-byte binary SHA>
 */
static int checkout_tree(const char *tree_sha, const char *dir) {
    GitObject obj;
    if (object_read(tree_sha, &obj) != 0) return 1;

    unsigned char *pos = obj.body;
    unsigned char *end = obj.body + obj.body_size;

    while (pos < end) {
        /* Parse mode */
        unsigned char *space = memchr(pos, ' ', (size_t)(end - pos));
        if (space == NULL) break;
        char mode[16];
        size_t mode_len = (size_t)(space - pos);
        memcpy(mode, pos, mode_len);
        mode[mode_len] = '\0';

        /* Parse name */
        unsigned char *name_start = space + 1;
        unsigned char *name_end = memchr(name_start, '\0', (size_t)(end - name_start));
        if (name_end == NULL) break;
        char name[256];
        size_t name_len = (size_t)(name_end - name_start);
        memcpy(name, name_start, name_len);
        name[name_len] = '\0';

        /* 20-byte binary SHA follows the NUL */
        if (name_end + 1 + 20 > end) break;
        unsigned char *sha_bin = name_end + 1;
        char *sha_hex = hex_to_string(sha_bin, 20);
        if (sha_hex == NULL) { free(obj.raw); return 1; }

        /* Build full path: dir/name */
        char path[GIT_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dir, name);

        if (strcmp(mode, "40000") == 0) {
            /* Directory entry: create dir and recurse into subtree */
            if (mkdir(path, DIRECTORY_PERMISSION) == -1) {
                GIT_ERR("clone: failed to create directory %s\n", path);
                free(sha_hex);
                free(obj.raw);
                return 1;
            }
            if (checkout_tree(sha_hex, path) != 0) {
                free(sha_hex);
                free(obj.raw);
                return 1;
            }
        } else {
            /* File entry: read blob and write to disk */
            GitObject blob;
            if (object_read(sha_hex, &blob) != 0) {
                free(sha_hex);
                free(obj.raw);
                return 1;
            }
            if (write_file(path, (const char *)blob.body,
                           blob.body_size, "wb") != 0) {
                GIT_ERR("clone: failed to write file %s\n", path);
                free(blob.raw);
                free(sha_hex);
                free(obj.raw);
                return 1;
            }
            free(blob.raw);
        }

        free(sha_hex);
        pos = sha_bin + 20;
    }

    free(obj.raw);
    return 0;
}

int clone_repo(const char *url, const char *dir) {
    /* Step 1: Create target directory and init .git/ inside it */
    if (mkdir(dir, DIRECTORY_PERMISSION) == -1) {
        GIT_ERR("clone: failed to create directory %s\n", dir);
        return 1;
    }

    /* chdir into the target directory so init_git() creates .git/ there */
    char original_dir[GIT_PATH_MAX];
    if (getcwd(original_dir, sizeof(original_dir)) == NULL) {
        GIT_ERR("clone: getcwd failed\n");
        return 1;
    }
    if (chdir(dir) != 0) {
        GIT_ERR("clone: chdir to %s failed\n", dir);
        return 1;
    }

    /* Initialize .git/ structure */
    if (init_git() != 0) return 1;

    /* Step 2: Discover refs — get HEAD SHA */
    HttpResponse refs_resp;
    if (http_get_refs(url, &refs_resp) != 0) return 1;

    char head_sha[41];
    if (pktline_parse_head(refs_resp.data, refs_resp.size, head_sha) != 0) {
        http_response_free(&refs_resp);
        return 1;
    }
    http_response_free(&refs_resp);

    /* Step 3: Build "want" request and fetch the packfile */
    char *want_body;
    size_t want_len;
    if (pktline_build_want(head_sha, &want_body, &want_len) != 0) return 1;

    fprintf(stderr, "DEBUG HEAD SHA: %s\n", head_sha);
    fprintf(stderr, "DEBUG want body (%zu bytes): ", want_len);
    for (size_t i = 0; i < want_len; i++)
        fprintf(stderr, "%c", (want_body[i] >= 32 && want_body[i] < 127) ? want_body[i] : '.');
    fprintf(stderr, "\n");

    HttpResponse pack_resp;
    if (http_post_pack(url, want_body, want_len, &pack_resp) != 0) {
        free(want_body);
        return 1;
    }
    free(want_body);

    /* DEBUG: dump response info */
    fprintf(stderr, "DEBUG pack_resp: size=%zu, first bytes:", pack_resp.size);
    for (size_t i = 0; i < 80 && i < pack_resp.size; i++)
        fprintf(stderr, " %02x", (unsigned char)pack_resp.data[i]);
    fprintf(stderr, "\n");

    /* Step 4: Strip side-band framing to get raw packfile */
    unsigned char *pack_data;
    size_t pack_len;
    if (pktline_strip_sideband(pack_resp.data, pack_resp.size,
                               &pack_data, &pack_len) != 0) {
        http_response_free(&pack_resp);
        return 1;
    }
    http_response_free(&pack_resp);

    /* Step 5: Parse packfile — writes all objects to .git/objects/ */
    if (packfile_parse(pack_data, pack_len) != 0) {
        free(pack_data);
        return 1;
    }
    free(pack_data);

    /* Step 6: Checkout — commit → tree → working directory */
    char tree_sha[41];
    if (get_tree_sha(head_sha, tree_sha) != 0) return 1;
    if (checkout_tree(tree_sha, ".") != 0) return 1;

    /* Restore original directory */
    if (chdir(original_dir) != 0) {
        GIT_ERR("clone: chdir back to %s failed\n", original_dir);
        return 1;
    }

    return 0;
}
