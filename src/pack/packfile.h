/*
 * packfile.h
 *
 * Parses a git packfile and writes all contained objects to
 * .git/objects/. Handles non-delta objects (commit, tree, blob, tag)
 * and REF_DELTA objects that reference a base by SHA.
 */

#ifndef PACKFILE_H
#define PACKFILE_H

#include <stddef.h>

/*
 * Parses a raw packfile and writes every object to the object store.
 *
 * The packfile must start with the "PACK" magic and be version 2.
 * Objects are decompressed with zlib and written in git's standard
 * format ("type size\0body") via object_write().
 *
 * Delta objects (REF_DELTA) are resolved by reading their base from
 * .git/objects/ — base objects must already be written (packfiles
 * guarantee bases come before their deltas).
 *
 * @param data  Raw packfile bytes (starting with "PACK").
 * @param len   Byte count of data.
 * @return      0 on success, 1 on failure.
 */
int packfile_parse(const unsigned char *data, size_t len);

#endif /* PACKFILE_H */
