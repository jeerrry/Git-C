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
     *
     * Format:
     *   "0032want <40-char SHA>\n"    ← 0x32 = 50 bytes total
     *   "0000"                        ← flush
     *   "0009done\n"                  ← 0x09 = 9 bytes total
     *
     * Total: 50 + 4 + 9 = 63 bytes.
     */
    const size_t total = 50 + 4 + 9;  /* want line + flush + done line */
    char *body = malloc(total + 1);    /* +1 for safety NUL */
    if (body == NULL) {
        GIT_ERR("pktline: malloc failed\n");
        return 1;
    }

    /* Build want line: "0032want <sha>\n" */
    snprintf(body, total + 1, "0032want %.40s\n00000009done\n", sha);

    *out_body = body;
    *out_len = total;
    return 0;
}
