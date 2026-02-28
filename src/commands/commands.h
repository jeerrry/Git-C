/*
 * commands.h
 *
 * Git command implementations. Each function corresponds to a git
 * subcommand and returns 0 on success, 1 on failure.
 * Errors are reported to stderr.
 */

#ifndef COMMANDS_H
#define COMMANDS_H

/* Initializes a new git repository by creating .git/, .git/refs/,
 * .git/objects/, and writing the default HEAD reference. */
int int_git(void);

/*
 * Reads and prints the content of a git blob object.
 *
 * @param sha1  40-character hex SHA-1 hash identifying the object.
 *              Resolved to .git/objects/<first 2 chars>/<remaining 38>.
 * @return      0 on success, 1 on failure.
 */
int cat_file(const char *sha1);

/*
 * Creates a git blob object from a file and writes it to the object store.
 *
 * Constructs "blob <size>\0<content>", computes its SHA-1 hash,
 * compresses with zlib, and writes to .git/objects/.
 * Prints the 40-character hex hash to stdout.
 *
 * @param path  Path to the file to hash.
 * @return      0 on success, 1 on failure.
 */
int hash_object(const char *path);

/*
 * Lists the contents of a git tree object.
 *
 * Reads and decompresses the tree object, then parses the binary
 * entry format (<mode> <name>\0<20-byte SHA>) and prints each
 * entry name to stdout, one per line.
 *
 * @param sha1  40-character hex SHA-1 hash identifying the tree object.
 * @return      0 on success, 1 on failure.
 */
int ls_tree(const char *sha1);

#endif /* COMMANDS_H */
