/*
 * object.h
 *
 * Common interface for reading git objects from the object store.
 * Encapsulates the read → decompress → parse-header pipeline shared
 * by every command that inspects stored objects (cat-file, ls-tree, etc.).
 */

#ifndef OBJECT_H
#define OBJECT_H

/* Parsed git object — body points into raw, so only raw needs freeing. */
typedef struct {
    unsigned char *body;       /* object content (past the header + \0) */
    unsigned long  body_size;  /* byte count of body */
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

#endif /* OBJECT_H */
