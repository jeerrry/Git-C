/*
 * directory.h
 *
 * Directory existence checking utility.
 */

#ifndef DIRECTORY_H
#define DIRECTORY_H

/*
 * Checks whether a directory exists at the given path.
 * Uses stat() internally.
 *
 * NOTE: Return value convention is inverted from typical boolean:
 *   0 = directory exists
 *   1 = directory does not exist, path is NULL, or stat() failed
 */
int is_directory_present(const char *path);

#endif /* DIRECTORY_H */
