/*
 * directory.c
 *
 * Directory existence checking using stat().
 */

#include <sys/stat.h>

#include "../../constants.h"

int directory_exists(const char *path) {
    if (path == NULL) return 0;

    struct stat statbuf;
    return (stat(path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) ? 1 : 0;
}
