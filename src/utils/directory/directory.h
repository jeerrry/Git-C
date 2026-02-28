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
 * @param path  Path to check.
 * @return      1 if directory exists, 0 otherwise.
 */
int directory_exists(const char *path);

#endif /* DIRECTORY_H */
