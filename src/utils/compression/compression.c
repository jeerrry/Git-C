/*
 * compression.c
 *
 * Zlib compression/decompression wrappers with automatic buffer management.
 * Git objects are always zlib-compressed on disk.
 */

#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

#include "../../constants.h"

unsigned char *decompress_data(const unsigned char *compressed_data, const unsigned long compressed_data_size,
                               unsigned long *decompressed_data_size) {
    /* Start with 8x the compressed size as an initial estimate.
     * If the data was highly compressed (common for text), the buffer
     * may be too small — uncompress returns Z_BUF_ERROR in that case.
     * We retry with doubled buffer size until it fits or a real error occurs. */
    uLongf dest_size = compressed_data_size * 8;

    while (1) {
        unsigned char *decompressed_data = malloc(dest_size);
        if (decompressed_data == NULL) {
            GIT_ERR("malloc failed\n");
            return NULL;
        }

        uLongf out_size = dest_size;
        int result = uncompress(decompressed_data, &out_size, compressed_data, compressed_data_size);
        if (result == Z_OK) {
            *decompressed_data_size = out_size;
            return decompressed_data;
        }

        free(decompressed_data);

        if (result != Z_BUF_ERROR) {
            GIT_ERR("uncompress failed: %d\n", result);
            return NULL;
        }

        dest_size *= 2;
    }
}

unsigned char *compress_data(const unsigned char *file_data, unsigned long file_data_size,
                             unsigned long *compressed_data_size) {
    if (!file_data || file_data_size == 0 || !compressed_data_size) {
        return NULL;
    }

    uLongf max_compressed_data_size = compressBound(file_data_size);
    unsigned char *compressed_data = malloc(max_compressed_data_size);
    if (!compressed_data) {
        GIT_ERR("malloc failed\n");
        return NULL;
    }

    int result = compress(compressed_data, &max_compressed_data_size, file_data, file_data_size);
    if (result != Z_OK) {
        GIT_ERR("compress failed: %d\n", result);
        free(compressed_data);
        return NULL;
    }

    *compressed_data_size = max_compressed_data_size;

    return compressed_data;
}
