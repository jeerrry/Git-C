/*
 * compression.h
 *
 * Wrappers around zlib compress/uncompress with automatic memory
 * management. All returned buffers are heap-allocated and must
 * be freed by the caller. Returns NULL on failure.
 */

#ifndef COMPRESSION_H
#define COMPRESSION_H

/*
 * Decompresses zlib-compressed data with automatic buffer sizing.
 * Uses a retry loop that doubles the output buffer on Z_BUF_ERROR,
 * so it handles any compression ratio without a known output size.
 *
 * @param compressed_data      Input compressed bytes.
 * @param compressed_data_size Size of input in bytes.
 * @param decompressed_data_size Output: set to actual decompressed size.
 * @return Heap-allocated decompressed data (caller must free), or NULL.
 */
unsigned char *decompress_data(const unsigned char *compressed_data,
                               unsigned long compressed_data_size,
                               unsigned long *decompressed_data_size);

/*
 * Compresses data using zlib default compression level.
 *
 * @param file_data           Input bytes to compress.
 * @param file_data_size      Size of input in bytes.
 * @param compressed_data_size Output: set to actual compressed size.
 * @return Heap-allocated compressed data (caller must free), or NULL.
 */
unsigned char *compress_data(const unsigned char *file_data,
                             unsigned long file_data_size,
                             unsigned long *compressed_data_size);

#endif /* COMPRESSION_H */
