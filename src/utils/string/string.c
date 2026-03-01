/*
 * string.c
 *
 * Hex conversion utilities for SHA-1 hashes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../constants.h"

char *hex_to_string(const unsigned char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) return NULL;

    char *str = malloc(buffer_size * 2 + 1);
    if (str == NULL) {
        GIT_ERR("Failed to allocate memory for hex string\n");
        return NULL;
    }

    for (size_t i = 0; i < buffer_size; i++) {
        snprintf(str + (i * 2), 3, "%02x", buffer[i]);
    }

    str[buffer_size * 2] = '\0';
    return str;
}

unsigned char *hex_string_to_bytes(const char *hex_str, size_t *out_len) {
    if (hex_str == NULL || out_len == NULL) return NULL;

    size_t hex_len = strlen(hex_str);

    /* Odd-length hex strings would silently lose the last nibble */
    if (hex_len % 2 != 0) {
        GIT_ERR("Odd-length hex string (%zu chars)\n", hex_len);
        return NULL;
    }

    size_t byte_len = hex_len / 2;
    unsigned char *bytes = malloc(byte_len);
    if (bytes == NULL) {
        GIT_ERR("Failed to allocate memory for hex-to-bytes conversion\n");
        return NULL;
    }

    /* Parse two hex characters at a time into one byte.
     * sscanf with %2hhx reads exactly 2 hex digits into an unsigned char.
     * Check return value to catch non-hex characters. */
    for (size_t i = 0; i < byte_len; i++) {
        if (sscanf(hex_str + (i * 2), "%2hhx", &bytes[i]) != 1) {
            GIT_ERR("Invalid hex character at position %zu\n", i * 2);
            free(bytes);
            return NULL;
        }
    }

    *out_len = byte_len;
    return bytes;
}
