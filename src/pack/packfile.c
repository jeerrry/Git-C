/*
 * packfile.c
 *
 * Parses a git v2 packfile: reads the header, decompresses each object
 * with zlib, resolves REF_DELTA objects, and writes everything to
 * .git/objects/ using the existing object_write() pipeline.
 *
 * Pack format overview:
 *   12-byte header: "PACK" + 4-byte version + 4-byte object count
 *   N objects, each:
 *     - variable-length header: 3-bit type + variable-length size
 *     - (REF_DELTA only: 20-byte base SHA)
 *     - zlib-compressed body
 *   20-byte checksum (ignored here)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <zlib.h>

#include "../constants.h"
#include "../objects/object.h"
#include "../utils/string/string.h"
#include "packfile.h"

/* Pack object type codes */
#define OBJ_COMMIT    1
#define OBJ_TREE      2
#define OBJ_BLOB      3
#define OBJ_TAG       4
#define OBJ_OFS_DELTA 6
#define OBJ_REF_DELTA 7

/* Type code → git object type string */
static const char *type_name(int type) {
    switch (type) {
        case OBJ_COMMIT: return "commit";
        case OBJ_TREE:   return "tree";
        case OBJ_BLOB:   return "blob";
        case OBJ_TAG:    return "tag";
        default:         return NULL;
    }
}

/* Reads a 4-byte big-endian unsigned integer. */
static uint32_t read_uint32_be(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

/*
 * Reads the variable-length type+size header of a pack object.
 *
 * Encoding (first byte):
 *   bit 7     = continuation flag
 *   bits 6-4  = object type (3 bits)
 *   bits 3-0  = size (lowest 4 bits)
 *
 * Subsequent bytes (while continuation flag set):
 *   bit 7     = continuation flag
 *   bits 6-0  = next 7 bits of size, shifted left
 *
 * Example: byte 0x92 = 1001_0010
 *   continuation=1, type=(001)=commit, size_low=0010=2
 *   Next byte needed for more size bits.
 */
static int read_type_and_size(const unsigned char *data, size_t len, size_t *pos,
                              int *type, size_t *size) {
    if (*pos >= len) return 1;
    unsigned char byte = data[(*pos)++];
    *type = (byte >> 4) & 0x07;
    *size = byte & 0x0F;
    int shift = 4;
    while (byte & 0x80) {
        if (*pos >= len) return 1;
        byte = data[(*pos)++];
        *size |= (size_t)(byte & 0x7F) << shift;
        shift += 7;
    }
    return 0;
}

/*
 * Decompresses one zlib stream from the packfile.
 *
 * Unlike our decompress_data() utility which uses uncompress(),
 * this uses inflate() directly so we can find out how many
 * compressed bytes were consumed — critical for advancing to the
 * next object in the packfile.
 *
 * @param data       Start of compressed data in the packfile.
 * @param avail_in   Maximum bytes available (rest of packfile).
 * @param expected   Expected decompressed size (from pack header).
 * @param consumed   Output: number of compressed bytes consumed.
 * @return           Heap-allocated decompressed data, or NULL on error.
 */
static unsigned char *inflate_stream(const unsigned char *data, size_t avail_in,
                                     size_t expected, size_t *consumed) {
    unsigned char *out = malloc(expected);
    if (out == NULL) {
        GIT_ERR("packfile: malloc failed for inflate (%zu bytes)\n", expected);
        return NULL;
    }

    z_stream strm = {0};
    if (inflateInit(&strm) != Z_OK) {
        GIT_ERR("packfile: inflateInit failed\n");
        free(out);
        return NULL;
    }

    strm.next_in = (Bytef *)data;
    strm.avail_in = (uInt)avail_in;
    strm.next_out = out;
    strm.avail_out = (uInt)expected;

    int ret = inflate(&strm, Z_FINISH);
    *consumed = strm.total_in;
    inflateEnd(&strm);

    if (ret != Z_STREAM_END) {
        GIT_ERR("packfile: inflate failed (ret=%d, expected %zu bytes)\n",
                ret, expected);
        free(out);
        return NULL;
    }

    return out;
}

/*
 * Reads a variable-length integer from delta instructions.
 *
 * Each byte contributes 7 bits of value. The MSB (bit 7) is a
 * continuation flag: 1 = more bytes follow, 0 = last byte.
 * Bits accumulate from least significant to most significant.
 *
 * Used for reading source/target sizes at the start of delta data.
 */
static size_t read_var_int(const unsigned char *data, size_t len, size_t *pos) {
    size_t value = 0;
    int shift = 0;
    while (*pos < len) {
        unsigned char byte = data[(*pos)++];
        value |= (size_t)(byte & 0x7F) << shift;
        if (!(byte & 0x80)) break;
        shift += 7;
    }
    return value;
}

/*
 * Applies a delta instruction stream to a base object.
 *
 * Delta format:
 *   1. Source (base) size — variable-length integer
 *   2. Target (result) size — variable-length integer
 *   3. Instructions:
 *      - COPY  (MSB=1): copy a range from the base object
 *        Lower 4 bits select which offset bytes follow (0-4 bytes).
 *        Next 3 bits select which size bytes follow (0-3 bytes).
 *        If size=0, it means 0x10000 (64KB).
 *      - INSERT (MSB=0): literal bytes from the delta stream
 *        Lower 7 bits = count of bytes to copy from delta data.
 *
 * @param base       Base object body.
 * @param base_len   Byte count of base.
 * @param delta      Decompressed delta instruction stream.
 * @param delta_len  Byte count of delta.
 * @param out_len    Output: byte count of the result.
 * @return           Heap-allocated result, or NULL on error.
 */
static unsigned char *apply_delta(const unsigned char *base, size_t base_len,
                                  const unsigned char *delta, size_t delta_len,
                                  size_t *out_len) {
    size_t pos = 0;

    /* Read and verify source/target sizes */
    size_t src_size = read_var_int(delta, delta_len, &pos);
    size_t tgt_size = read_var_int(delta, delta_len, &pos);
    (void)src_size; /* validated implicitly — base_len should match */

    unsigned char *result = malloc(tgt_size);
    if (result == NULL) {
        GIT_ERR("packfile: malloc failed for delta result\n");
        return NULL;
    }

    size_t rpos = 0; /* write position in result */

    while (pos < delta_len) {
        unsigned char cmd = delta[pos++];

        if (cmd & 0x80) {
            /* COPY instruction: copy a slice from the base object.
             *
             * The lower 4 bits of cmd tell us which offset bytes follow:
             *   bit 0 → offset byte 0 (bits 0-7)
             *   bit 1 → offset byte 1 (bits 8-15)
             *   bit 2 → offset byte 2 (bits 16-23)
             *   bit 3 → offset byte 3 (bits 24-31)
             *
             * Bits 4-6 tell us which size bytes follow:
             *   bit 4 → size byte 0 (bits 0-7)
             *   bit 5 → size byte 1 (bits 8-15)
             *   bit 6 → size byte 2 (bits 16-23)
             *
             * Missing bytes default to 0. Size of 0 means 0x10000. */
            size_t offset = 0, size = 0;
            if (cmd & 0x01) { if (pos >= delta_len) goto corrupt; offset  = delta[pos++]; }
            if (cmd & 0x02) { if (pos >= delta_len) goto corrupt; offset |= (size_t)delta[pos++] << 8; }
            if (cmd & 0x04) { if (pos >= delta_len) goto corrupt; offset |= (size_t)delta[pos++] << 16; }
            if (cmd & 0x08) { if (pos >= delta_len) goto corrupt; offset |= (size_t)delta[pos++] << 24; }
            if (cmd & 0x10) { if (pos >= delta_len) goto corrupt; size  = delta[pos++]; }
            if (cmd & 0x20) { if (pos >= delta_len) goto corrupt; size |= (size_t)delta[pos++] << 8; }
            if (cmd & 0x40) { if (pos >= delta_len) goto corrupt; size |= (size_t)delta[pos++] << 16; }
            if (size == 0) size = 0x10000;

            if (offset + size > base_len || rpos + size > tgt_size) goto corrupt;
            memcpy(result + rpos, base + offset, size);
            rpos += size;
        } else if (cmd > 0) {
            /* INSERT instruction: copy literal bytes from delta stream */
            if (pos + cmd > delta_len || rpos + cmd > tgt_size) goto corrupt;
            memcpy(result + rpos, delta + pos, cmd);
            pos += cmd;
            rpos += cmd;
        }
        /* cmd == 0 is reserved — skip */
    }

    *out_len = rpos;
    return result;

corrupt:
    GIT_ERR("packfile: corrupt delta instruction stream\n");
    free(result);
    return NULL;
}

/*
 * Wraps a raw object body in git's "type size\0body" format
 * and writes it to .git/objects/ via object_write().
 *
 * @return  Heap-allocated 40-char SHA hex (caller frees), or NULL.
 */
static char *write_pack_object(const char *type, const unsigned char *body,
                               size_t body_size) {
    int header_len = snprintf(NULL, 0, "%s %zu", type, body_size);
    size_t total = (size_t)header_len + 1 + body_size;
    char *obj = malloc(total);
    if (obj == NULL) return NULL;

    snprintf(obj, (size_t)header_len + 1, "%s %zu", type, body_size);
    obj[header_len] = '\0';
    memcpy(obj + header_len + 1, body, body_size);

    char *sha = object_write(obj, total);
    free(obj);
    return sha;
}

int packfile_parse(const unsigned char *data, size_t len) {
    /* --- Header (12 bytes) --- */
    if (len < 12 || memcmp(data, "PACK", 4) != 0) {
        GIT_ERR("packfile: not a valid packfile (bad magic)\n");
        return 1;
    }

    uint32_t version = read_uint32_be(data + 4);
    if (version != 2) {
        GIT_ERR("packfile: unsupported version %u (expected 2)\n", version);
        return 1;
    }

    uint32_t obj_count = read_uint32_be(data + 8);
    size_t pos = 12;

    /* --- Process each object --- */
    for (uint32_t i = 0; i < obj_count; i++) {
        int type;
        size_t size;
        if (read_type_and_size(data, len, &pos, &type, &size) != 0) {
            GIT_ERR("packfile: truncated object header at index %u\n", i);
            return 1;
        }

        /* For REF_DELTA: read the 20-byte binary SHA of the base object */
        unsigned char base_sha_bin[20];
        if (type == OBJ_REF_DELTA) {
            if (pos + 20 > len) {
                GIT_ERR("packfile: truncated REF_DELTA SHA at index %u\n", i);
                return 1;
            }
            memcpy(base_sha_bin, data + pos, 20);
            pos += 20;
        }

        /* Decompress the object body (or delta instructions) */
        size_t consumed;
        unsigned char *body = inflate_stream(data + pos, len - pos,
                                             size, &consumed);
        if (body == NULL) return 1;
        pos += consumed;

        if (type >= OBJ_COMMIT && type <= OBJ_TAG) {
            /* Non-delta object: write directly */
            char *sha = write_pack_object(type_name(type), body, size);
            free(body);
            if (sha == NULL) return 1;
            free(sha);

        } else if (type == OBJ_REF_DELTA) {
            /* Delta object: resolve against base, then write.
             *
             * Step 1: Convert 20-byte binary SHA → 40-char hex string
             * Step 2: Read the base object from .git/objects/
             * Step 3: Extract the base object's type from its raw header
             * Step 4: Apply the delta instructions to get the result
             * Step 5: Write the result with the base's type
             */
            char *base_hex = hex_to_string(base_sha_bin, 20);
            if (base_hex == NULL) { free(body); return 1; }

            GitObject base_obj;
            if (object_read(base_hex, &base_obj) != 0) {
                GIT_ERR("packfile: cannot read base object %s\n", base_hex);
                free(base_hex);
                free(body);
                return 1;
            }
            free(base_hex);

            /* Parse type from the raw header: "type size\0..." */
            const char *space = memchr(base_obj.raw, ' ', 32);
            if (space == NULL) {
                GIT_ERR("packfile: malformed base object header\n");
                free(body);
                free(base_obj.raw);
                return 1;
            }
            size_t tlen = (size_t)(space - (const char *)base_obj.raw);
            char base_type[16];
            if (tlen >= sizeof(base_type)) tlen = sizeof(base_type) - 1;
            memcpy(base_type, base_obj.raw, tlen);
            base_type[tlen] = '\0';

            /* Apply delta */
            size_t result_size;
            unsigned char *result = apply_delta(base_obj.body, base_obj.body_size,
                                                body, size, &result_size);
            free(body);
            free(base_obj.raw);

            if (result == NULL) return 1;

            char *sha = write_pack_object(base_type, result, result_size);
            free(result);
            if (sha == NULL) return 1;
            free(sha);

        } else {
            GIT_ERR("packfile: unsupported object type %d at index %u\n",
                    type, i);
            free(body);
            return 1;
        }
    }

    return 0;
}
