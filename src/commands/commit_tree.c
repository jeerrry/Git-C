/*
 * commit_tree.c
 *
 * Implements the "git commit-tree" command — creates a commit object
 * that links a tree snapshot to its parent commit with metadata.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../constants.h"
#include "../objects/object.h"

int commit_tree(const char *tree_sha, const char *parent_sha, const char *message) {
    /* Build the commit body:
     *   tree <sha>\n
     *   parent <sha>\n
     *   author <name> <email> <timestamp> <tz>\n
     *   committer <name> <email> <timestamp> <tz>\n
     *   \n
     *   <message>\n
     */
    time_t now = time(NULL);

    int body_len_int = snprintf(NULL, 0,
        "tree %s\nparent %s\nauthor Dev <dev@example.com> %ld +0000\n"
        "committer Dev <dev@example.com> %ld +0000\n\n%s\n",
        tree_sha, parent_sha, (long)now, (long)now, message);
    if (body_len_int < 0) {
        GIT_ERR("Error formatting commit body\n");
        return 1;
    }
    size_t body_len = (size_t)body_len_int;

    /* Wrap with "commit <body_len>\0<body>" header */
    int header_len_int = snprintf(NULL, 0, "commit %zu", body_len);
    if (header_len_int < 0) {
        GIT_ERR("Error formatting commit header\n");
        return 1;
    }
    size_t header_len = (size_t)header_len_int;
    size_t total_size = header_len + 1 + body_len;

    char *commit_data = malloc(total_size);
    if (commit_data == NULL) {
        GIT_ERR("Error allocating memory for commit data\n");
        return 1;
    }

    snprintf(commit_data, header_len + 1, "commit %zu", body_len);
    commit_data[header_len] = '\0';

    snprintf(commit_data + header_len + 1, body_len + 1,
        "tree %s\nparent %s\nauthor Dev <dev@example.com> %ld +0000\n"
        "committer Dev <dev@example.com> %ld +0000\n\n%s\n",
        tree_sha, parent_sha, (long)now, (long)now, message);

    char *sha_hex = object_write(commit_data, total_size);
    free(commit_data);
    if (sha_hex == NULL) return 1;

    printf("%s\n", sha_hex);
    free(sha_hex);
    return 0;
}
