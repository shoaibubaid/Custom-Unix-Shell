#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "alias.h"
#include "builtins.h"
#include "history.h"
#include "vars.h"

static int argc_of(char **t)
{
    int n = 0;
    while (t[n] != NULL)
        n++;
    return n;
}

/* cd must run in the shell itself: chdir in a child would only change
 * the child's working directory. */
static int builtin_cd(char **t)
{
    int argc = argc_of(t);
    if (argc > 2) {
        fprintf(stderr, "cd: usage: cd [directory]\n");
        return 1;
    }
    const char *dir = (argc == 1) ? home_dir() : t[1];
    if (chdir(dir) != 0) {
        fprintf(stderr, "cd: %s: %s\n", dir, strerror(errno));
        return 1;
    }
    return 0;
}

static int builtin_echo(char **t)
{
    int i = 1, newline = 1;
    if (t[1] != NULL && strcmp(t[1], "-n") == 0) {
        newline = 0;
        i = 2;
    }
    for (; t[i] != NULL; i++)
        printf("%s%s", t[i], t[i + 1] != NULL ? " " : "");
    if (newline)
        printf("\n");
    return 0;
}

/* alias            -> list all
 * alias name       -> show one
 * alias name=value -> define (quote the value to keep spaces: ll='ls -l') */
static int builtin_alias(char **t)
{
    int status = 0;
    if (t[1] == NULL) {
        alias_print();
        return 0;
    }
    for (int i = 1; t[i] != NULL; i++) {
        char *eq = strchr(t[i], '=');
        if (eq == NULL) {
            const char *v = alias_get(t[i]);
            if (v != NULL) {
                printf("alias %s='%s'\n", t[i], v);
            } else {
                fprintf(stderr, "alias: %s: not found\n", t[i]);
                status = 1;
            }
        } else if (eq == t[i]) {
            fprintf(stderr, "alias: missing name before '='\n");
            status = 1;
        } else {
            *eq = '\0';
            if (alias_set(t[i], eq + 1) < 0) {
                fprintf(stderr, "alias: too many aliases\n");
                status = 1;
            }
            *eq = '=';
        }
    }
    return status;
}

static int builtin_unalias(char **t)
{
    int status = 0;
    if (t[1] == NULL) {
        fprintf(stderr, "unalias: usage: unalias name...\n");
        return 1;
    }
    for (int i = 1; t[i] != NULL; i++) {
        if (alias_remove(t[i]) < 0) {
            fprintf(stderr, "unalias: %s: not found\n", t[i]);
            status = 1;
        }
    }
    return status;
}

/* `open FILE` is rewritten to "<program> FILE" before it gets here (see
 * expand.c); only a bare `open` reaches this function. */
static int builtin_open(char **t)
{
    (void)t;
    fprintf(stderr, "open: usage: open FILE   (programs are set with "
                    "'opener EXT PROGRAM' in ~/.shellrc)\n");
    return 1;
}

static const char *names[] = {
    "cd", "exit", "echo", "history", "alias", "unalias", "open", NULL
};

const char *builtin_name(int i)
{
    return (i >= 0 && i < (int)(sizeof(names) / sizeof(names[0])) - 1) ? names[i] : NULL;
}

int is_builtin(const char *name)
{
    for (int i = 0; names[i] != NULL; i++)
        if (strcmp(name, names[i]) == 0)
            return 1;
    return 0;
}

int is_stateful_builtin(const char *name)
{
    return strcmp(name, "cd") == 0 || strcmp(name, "exit") == 0
        || strcmp(name, "alias") == 0 || strcmp(name, "unalias") == 0;
}

enum builtin_result run_builtin(char **t)
{
    int status = 0;

    if (!is_builtin(t[0]))
        return BUILTIN_NONE;

    if (strcmp(t[0], "cd") == 0) {
        status = builtin_cd(t);
    } else if (strcmp(t[0], "exit") == 0) {
        /* only reports "please stop": cleanup happens in main(), so Ctrl+D
         * shares exactly the same shutdown path */
        if (t[1] != NULL) {
            fprintf(stderr, "exit: too many arguments\n");
            status = 1;
        } else {
            return BUILTIN_EXIT;
        }
    } else if (strcmp(t[0], "echo") == 0) {
        status = builtin_echo(t);
    } else if (strcmp(t[0], "history") == 0) {
        history_print();
    } else if (strcmp(t[0], "alias") == 0) {
        status = builtin_alias(t);
    } else if (strcmp(t[0], "unalias") == 0) {
        status = builtin_unalias(t);
    } else if (strcmp(t[0], "open") == 0) {
        status = builtin_open(t);
    }

    set_last_status(status);
    return BUILTIN_DONE;
}
