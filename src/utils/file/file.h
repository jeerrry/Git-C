/*
 * file.h
 *
 * File I/O utilities and git object path resolution.
 * Functions that return heap-allocated memory document ownership
 * in their docstrings — callers must free returned pointers.
 */

#ifndef FILE_H
#define FILE_H

#include <stddef.h>

/*
 * Resolves a SHA-1 hex hash to absolute object store paths.
 *
 * Converts e.g. "abcdef..." into:
 *   abs_dir  = ".git/objects/ab"
 *   abs_file = ".git/objects/ab/cdef..."
 *
 * Zero heap allocations — caller provides stack buffers.
 * abs_dir may be NULL if the caller only needs the file path.
 *
 * @param sha_hex   40-character hex hash string.
 * @param abs_dir   Output buffer for directory path, or NULL.
 * @param dir_size  Size of abs_dir buffer.
 * @param abs_file  Output buffer for full file path.
 * @param file_size Size of abs_file buffer.
 * @return          0 on success, 1 on invalid SHA.
 */
int object_path(const char *sha_hex, char *abs_dir, size_t dir_size,
                char *abs_file, size_t file_size);

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

#endif /* FILE_H */
