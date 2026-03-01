/*
 * object.h
 *
 * Common interface for reading and writing git objects.
 * Encapsulates the shared pipelines so commands stay focused on
 * their own logic rather than reimplementing I/O and hashing.
 */

#ifndef OBJECT_H
#define OBJECT_H

#include <stddef.h>

/* Parsed git object — body points into raw, so only raw needs freeing. */
typedef struct {
    unsigned char *body;       /* object content (past the header + \0) */
    size_t         body_size;  /* byte count of body */
    unsigned char *raw;        /* full decompressed buffer (caller frees) */
} GitObject;

/*
 * Reads, decompresses, and parses a git object by SHA-1 hash.
 *
 * On success, populates *out: body points to the content after the
 * "type size\0" header, body_size is the content length, and raw
 * holds the full decompressed buffer. Caller must free(out->raw).
 *
 * @param sha1  40-character hex SHA-1 identifying the object.
 * @param out   Output struct populated on success.
 * @return      0 on success, 1 on failure.
 */
int object_read(const char *sha1, GitObject *out);

/*
 * Writes a complete git object to .git/objects/.
 *
 * Takes already-formatted object data (e.g. "blob 12\0..." or "tree 95\0..."),
 * computes SHA-1, compresses with zlib, and writes to the object store.
 *
 * @param object_data  Formatted git object (header + \0 + content).
 * @param object_size  Total byte count of object_data.
 * @return             Heap-allocated 40-char hex hash (caller must free), or NULL.
 */
char *object_write(const char *object_data, size_t object_size);

/*
 * Creates a blob object for a file and writes it to the object store.
 *
 * Reads the file, wraps it in "blob <size>\0<content>" format,
 * and calls object_write. Used by both hash-object and write-tree.
 *
 * @param path  Path to the file.
 * @return      Heap-allocated 40-char hex hash (caller must free), or NULL.
 */
char *create_blob(const char *path);

#endif /* OBJECT_H */
