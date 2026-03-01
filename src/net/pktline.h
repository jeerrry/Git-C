/*
 * pktline.h
 *
 * Git pkt-line wire format parser and builder.
 *
 * Every line in git's smart HTTP protocol is prefixed with a
 * 4-character hex length (e.g. "003e"). The length includes the
 * 4 prefix bytes themselves. "0000" is a flush packet — a separator
 * between logical groups.
 */

#ifndef PKTLINE_H
#define PKTLINE_H

#include <stddef.h>

/*
 * Parses a refs discovery response and extracts the HEAD commit SHA.
 *
 * The server sends pkt-line formatted ref advertisements like:
 *   003eREF_SHA capabilities\n
 *   003fSHA refs/heads/master\n
 *   0000
 *
 * The first ref line after the service header contains HEAD's SHA
 * as the first 40 hex characters.
 *
 * @param data      Raw response body from http_get_refs().
 * @param data_len  Byte count of data.
 * @param sha_out   Output buffer — must be at least 41 bytes (40 hex + NUL).
 * @return          0 on success, 1 on failure.
 */
int pktline_parse_head(const char *data, size_t data_len, char *sha_out);

/*
 * Builds a "want" request body for git-upload-pack.
 *
 * Produces the pkt-line encoded request:
 *   0032want <40-char SHA>\n
 *   00000009done\n
 *
 * The caller must free() the returned buffer.
 *
 * @param sha       40-character hex SHA to request.
 * @param out_body  Output: pointer to the allocated request body.
 * @param out_len   Output: byte count of the request body.
 * @return          0 on success, 1 on failure.
 */
int pktline_build_want(const char *sha, char **out_body, size_t *out_len);

/*
 * Extracts the raw packfile from an upload-pack response.
 *
 * Handles two response formats:
 *   1. Side-band framing: pkt-line packets with \x01 channel byte
 *   2. Raw: packfile bytes directly after a NAK pkt-line
 * Scans for the "PACK" magic as a fallback if no side-band data is found.
 *
 * @param data      Raw response body from http_post_pack().
 * @param data_len  Byte count of data.
 * @param pack_out  Output: heap-allocated raw packfile (caller must free).
 * @param pack_len  Output: byte count of the packfile.
 * @return          0 on success, 1 on failure.
 */
int pktline_strip_sideband(const char *data, size_t data_len,
                           unsigned char **pack_out, size_t *pack_len);

#endif /* PKTLINE_H */
