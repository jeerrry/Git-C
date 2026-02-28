/*
 * file.h
 *
 * File I/O utilities and path manipulation for the git object store.
 * Functions that return heap-allocated memory document ownership
 * in their docstrings — callers must free returned pointers.
 */

#ifndef FILE_H
#define FILE_H

#include <stddef.h>

/*
 * Converts a SHA-1 hex string into a git object path: "xx/xxx...".
 * Git shards objects by the first two hex digits to avoid
 * filesystem limits on directory entry counts.
 *
 * @param sha1_hex  40-character hex hash string.
 * @return          Heap-allocated path string (caller must free), or NULL.
 */
char *get_file_path(const char *sha1_hex);

/*
 * Writes raw bytes to a file.
 *
 * @param path       Absolute path to the target file.
 * @param data       Buffer to write.
 * @param byte_count Number of bytes to write from data.
 * @param mode       fopen mode string (e.g., "wb" for write-binary).
 * @return           0 on success, 1 on failure.
 */
int write_file(const char *path, const char *data, size_t byte_count, const char *mode);

/*
 * Reads an entire file into a heap-allocated buffer.
 *
 * @param path       Absolute path to the file.
 * @param file_size  Output: set to the number of bytes read.
 * @return           Heap-allocated buffer (caller must free), or NULL.
 *                   Buffer is null-terminated for convenience.
 */
char *read_file(const char *path, long *file_size);

/*
 * Reads a compressed git blob object by its SHA-1 hash.
 * Resolves the hash to .git/objects/<xx>/<rest> and reads the raw bytes.
 *
 * @param sha1_hex        40-character hex hash string.
 * @param compressed_size Output: set to the size of the compressed data.
 * @return                Heap-allocated compressed data (caller must free), or NULL.
 */
char *read_git_blob_file(const char *sha1_hex, long *compressed_size);

/*
 * Splits a path at its last '/' into directory and filename components.
 * Both output strings are heap-allocated; caller must free both.
 *
 * @param path      Input path containing at least one '/'.
 * @param directory Output: heap-allocated directory portion (caller must free).
 * @param file_name Output: heap-allocated filename portion (caller must free).
 * @return          0 on success, 1 on invalid input or allocation failure.
 */
int split_file_path(const char *path, char **directory, char **file_name);

#endif /* FILE_H */
