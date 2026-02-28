/*
 * string.c
 *
 * String manipulation utilities: hex conversion for SHA-1 output,
 * substring slicing, and numeric formatting.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../constants.h"

void slice_str(const char *str, char *buffer, const size_t start, const size_t end) {
    size_t index = 0;
    for (size_t i = start; i < end; i++) {
        buffer[index++] = str[i];
    }

    buffer[index] = '\0';
}

void size_t_to_string(size_t value, char *str, size_t buffer_size) {
    snprintf(str, buffer_size, "%zu", value);
}

char *hex_to_string(const unsigned char *buffer, size_t buffer_size) {
    char *str = malloc(buffer_size * 2 + 1);
    if (str == NULL) {
        GIT_ERR("Failed to allocate memory for hex string\n");
        return NULL;
    }

    for (size_t i = 0; i < buffer_size; i++) {
        sprintf(str + (i * 2), "%02x", buffer[i]);
    }

    str[buffer_size * 2] = '\0';
    return str;
}

unsigned char *hex_string_to_bytes(const char *hex_str, size_t *out_len) {
    size_t hex_len = strlen(hex_str);
    size_t byte_len = hex_len / 2;

    unsigned char *bytes = malloc(byte_len);
    if (bytes == NULL) {
        GIT_ERR("Failed to allocate memory for hex-to-bytes conversion\n");
        return NULL;
    }

    /* Parse two hex characters at a time into one byte.
     * sscanf with %2hhx reads exactly 2 hex digits into an unsigned char. */
    for (size_t i = 0; i < byte_len; i++) {
        sscanf(hex_str + (i * 2), "%2hhx", &bytes[i]);
    }

    *out_len = byte_len;
    return bytes;
}
