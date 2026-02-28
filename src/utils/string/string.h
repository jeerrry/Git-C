/*
 * string.h
 *
 * String manipulation utilities for hex conversion and slicing.
 */

#ifndef STRING_H
#define STRING_H

#include <stddef.h>

/* Converts a size_t value to its decimal string representation.
 * @param value       The value to convert.
 * @param str         Output buffer (must be large enough).
 * @param buffer_size Size of the output buffer. */
void size_t_to_string(size_t value, char *str, size_t buffer_size);

/* Copies characters from str[start] to str[end-1] into buffer.
 * Buffer must be at least (end - start + 1) bytes for the null terminator. */
void slice_str(const char *str, char *buffer, size_t start, size_t end);

/*
 * Converts a binary byte buffer to a hex string.
 *
 * @param buffer      Raw bytes (e.g., SHA-1 digest).
 * @param buffer_size Number of bytes in buffer.
 * @return            Heap-allocated hex string of length buffer_size*2 + 1
 *                    (caller must free), or NULL on allocation failure.
 */
char *hex_to_string(const unsigned char *buffer, size_t buffer_size);

/*
 * Converts a hex string to raw binary bytes (inverse of hex_to_string).
 *
 * Tree entries store SHA-1 as 20 raw bytes, not 40-char hex.
 * This function converts e.g. "a3f2" → {0xa3, 0xf2}.
 *
 * @param hex_str    Hex string (must have even length).
 * @param out_len    Output: set to number of bytes written.
 * @return           Heap-allocated byte buffer (caller must free), or NULL.
 */
unsigned char *hex_string_to_bytes(const char *hex_str, size_t *out_len);

#endif /* STRING_H */
