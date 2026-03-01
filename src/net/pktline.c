/*
 * pktline.c
 *
 * Git pkt-line wire format: parse ref advertisements and
 * build "want" requests for the smart HTTP protocol.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../constants.h"
#include "pktline.h"

/*
 * Converts a 4-character hex string to an integer.
 * Returns -1 if any character is not valid hex.
 *
 * Example: "003e" → 62
 */
static int hex4_to_int(const char *hex) {
    int value = 0;
    for (int i = 0; i < 4; i++) {
        char c = hex[i];
        int digit;
        if (c >= '0' && c <= '9')      digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else return -1;
        value = value * 16 + digit;
    }
    return value;
}

int pktline_parse_head(const char *data, size_t data_len, char *sha_out) {
    /*
     * Walk through pkt-lines, skipping the service announcement header.
     *
     * Typical response structure:
     *   001e# service=git-upload-pack\n   ← service header
     *   0000                              ← flush (end of header)
     *   00XXsha1 HEAD\0capabilities\n     ← first ref = HEAD
     *   00XXsha1 refs/heads/master\n      ← other refs
     *   0000                              ← flush (end of refs)
     *
     * We want the SHA from the first ref line after the first flush.
     */
    size_t pos = 0;
    int seen_flush = 0;

    while (pos + 4 <= data_len) {
        int pkt_len = hex4_to_int(data + pos);
        if (pkt_len < 0) {
            GIT_ERR("pktline: invalid hex length at offset %zu\n", pos);
            return 1;
        }

        /* Flush packet */
        if (pkt_len == 0) {
            seen_flush = 1;
            pos += 4;
            continue;
        }

        /* Sanity: length must cover at least the 4-byte prefix */
        if (pkt_len < 4 || pos + (size_t)pkt_len > data_len) {
            GIT_ERR("pktline: bad packet length %d at offset %zu\n", pkt_len, pos);
            return 1;
        }

        /*
         * After the first flush, the next packet's payload starts with
         * the 40-char hex SHA of HEAD. The payload starts at pos+4.
         */
        if (seen_flush) {
            const char *payload = data + pos + 4;
            int payload_len = pkt_len - 4;

            if (payload_len < 40) {
                GIT_ERR("pktline: ref line too short (%d bytes)\n", payload_len);
                return 1;
            }

            memcpy(sha_out, payload, 40);
            sha_out[40] = '\0';
            return 0;
        }

        pos += (size_t)pkt_len;
    }

    GIT_ERR("pktline: HEAD SHA not found in refs response\n");
    return 1;
}

int pktline_build_want(const char *sha, char **out_body, size_t *out_len) {
    /*
     * Build the request body that tells the server which objects we want.
     * No capabilities requested — keeps the exchange simple.
     *
     * Format:
     *   "0032want <40-char SHA>\n"  ← 0x32 = 50 bytes total
     *   "0000"                      ← flush
     *   "0009done\n"               ← 0x09 = 9 bytes total
     *
     * Total: 50 + 4 + 9 = 63 bytes.
     */
    const size_t total = 50 + 4 + 9;
    char *body = malloc(total + 1);
    if (body == NULL) {
        GIT_ERR("pktline: malloc failed\n");
        return 1;
    }

    snprintf(body, total + 1, "0032want %.40s\n00000009done\n", sha);

    *out_body = body;
    *out_len = total;
    return 0;
}

int pktline_strip_sideband(const char *data, size_t data_len,
                           unsigned char **pack_out, size_t *pack_len) {
    /*
     * Extract the raw packfile from an upload-pack response.
     *
     * Strategy 1: Walk pkt-line packets and extract channel 1 data.
     *   Flush packets ("0000") between NAK and the side-band data
     *   are skipped (not treated as terminators).
     *
     * Strategy 2 (fallback): If no side-band data is found, scan
     *   for the "PACK" magic directly — handles servers that send
     *   the packfile as raw bytes after NAK.
     */
    unsigned char *out = NULL;
    size_t out_size = 0;
    size_t pos = 0;

    while (pos + 4 <= data_len) {
        int pkt_len = hex4_to_int(data + pos);

        /* Not valid pkt-line data — stop scanning */
        if (pkt_len < 0) break;

        /* Flush packet — skip it and keep looking.
         * There may be a flush between NAK and the side-band data. */
        if (pkt_len == 0) {
            pos += 4;
            continue;
        }

        if (pkt_len < 4 || pos + (size_t)pkt_len > data_len) break;

        /* Payload starts after the 4-byte hex prefix.
         * The first byte of payload is the channel indicator. */
        if (pkt_len > 5 && (unsigned char)data[pos + 4] == 1) {
            /* Channel 1: packfile data — skip the channel byte */
            const char *chunk = data + pos + 5;
            size_t chunk_len = (size_t)pkt_len - 5;

            unsigned char *grown = realloc(out, out_size + chunk_len);
            if (grown == NULL) {
                GIT_ERR("pktline: realloc failed\n");
                free(out);
                return 1;
            }
            out = grown;
            memcpy(out + out_size, chunk, chunk_len);
            out_size += chunk_len;
        }
        /* Channel 2 (progress) and channel 3 (error) are silently skipped.
         * Non-sideband lines like NAK are also skipped. */

        pos += (size_t)pkt_len;
    }

    if (out_size > 0) {
        *pack_out = out;
        *pack_len = out_size;
        return 0;
    }
    free(out);

    /* Fallback: server sent raw PACK bytes (no side-band framing).
     * Scan for the "PACK" magic and copy everything from there. */
    for (size_t i = 0; i + 4 <= data_len; i++) {
        if (memcmp(data + i, "PACK", 4) == 0) {
            size_t raw_len = data_len - i;
            unsigned char *raw = malloc(raw_len);
            if (raw == NULL) {
                GIT_ERR("pktline: malloc failed\n");
                return 1;
            }
            memcpy(raw, data + i, raw_len);
            *pack_out = raw;
            *pack_len = raw_len;
            return 0;
        }
    }

    GIT_ERR("pktline: no packfile data found in response\n");
    return 1;
}
