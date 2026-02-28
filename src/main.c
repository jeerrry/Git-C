/*
 * main.c
 *
 * CLI entry point for the git implementation.
 * Dispatches subcommands (init, cat-file, hash-object) to their handlers.
 * Uses a linear if-chain for dispatch — will be replaced with a command
 * registry table once more commands are added (ls-tree, write-tree, etc.).
 */

#include <stdio.h>
#include <string.h>
#include "commands/commands.h"

int main(const int argc, char *argv[]) {
    /* Disable output buffering so CodeCrafters test runner sees output immediately */
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    if (argc < 2) {
        fprintf(stderr, "Usage: ./your_program.sh <command> [<args>]\n");
        return 1;
    }

    const char *command = argv[1];

    if (strcmp(command, "init") == 0) {
        return int_git();
    }

    if (strcmp(command, "cat-file") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: ./your_program.sh cat-file -p <sha1>\n");
            return 1;
        }
        if (strcmp(argv[2], "-p") == 0) {
            return cat_file(argv[3]);
        }
    }

    if (strcmp(command, "hash-object") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: ./your_program.sh hash-object -w <file>\n");
            return 1;
        }
        if (strcmp(argv[2], "-w") == 0) {
            return hash_object(argv[3]);
        }
    }

    fprintf(stderr, "Unknown command %s\n", command);
    return 1;
}
