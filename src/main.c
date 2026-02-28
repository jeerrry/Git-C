/*
 * main.c
 *
 * CLI entry point for the git implementation.
 * Uses a table-driven dispatch — adding a new command is just
 * adding a row to the commands[] array and a thin wrapper.
 */

#include <stdio.h>
#include <string.h>
#include "commands/commands.h"

/* Wrappers adapt the generic (argc, argv) dispatch signature
 * to each command's specific parameters. CLI parsing stays here
 * so the command implementations stay clean and testable. */

static int cmd_init(int argc, char **argv) {
    (void)argc; (void)argv;
    return init_git();
}

static int cmd_cat_file(int argc, char **argv) {
    (void)argc;
    return cat_file(argv[3]);
}

static int cmd_hash_object(int argc, char **argv) {
    (void)argc;
    return hash_object(argv[3]);
}

static int cmd_ls_tree(int argc, char **argv) {
    (void)argc;
    return ls_tree(argv[3]);
}

static int cmd_write_tree(int argc, char **argv) {
    (void)argc; (void)argv;
    return write_tree();
}

typedef struct {
    const char *name;      /* command name to match against argv[1] */
    int min_argc;          /* minimum argc required */
    const char *flag;      /* expected flag at argv[2], or NULL if none */
    const char *usage;     /* usage string (NULL if no args needed) */
    int (*handler)(int argc, char **argv);
} Command;

static const Command commands[] = {
    { "init",        2, NULL,          NULL,                            cmd_init },
    { "cat-file",    4, "-p",          "cat-file -p <sha1>",           cmd_cat_file },
    { "hash-object", 4, "-w",          "hash-object -w <file>",        cmd_hash_object },
    { "ls-tree",     4, "--name-only", "ls-tree --name-only <sha1>",   cmd_ls_tree },
    { "write-tree",  2, NULL,          NULL,                            cmd_write_tree },
};

static const size_t num_commands = sizeof(commands) / sizeof(commands[0]);

int main(const int argc, char *argv[]) {
    /* Disable output buffering so CodeCrafters test runner sees output immediately */
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    if (argc < 2) {
        fprintf(stderr, "Usage: ./your_program.sh <command> [<args>]\n");
        return 1;
    }

    const char *command = argv[1];

    for (size_t i = 0; i < num_commands; i++) {
        if (strcmp(command, commands[i].name) != 0) continue;

        if (argc < commands[i].min_argc) {
            fprintf(stderr, "Usage: ./your_program.sh %s\n", commands[i].usage);
            return 1;
        }
        if (commands[i].flag != NULL && strcmp(argv[2], commands[i].flag) != 0) {
            fprintf(stderr, "Unknown flag %s for %s\n", argv[2], commands[i].name);
            return 1;
        }
        return commands[i].handler(argc, argv);
    }

    fprintf(stderr, "Unknown command %s\n", command);
    return 1;
}
