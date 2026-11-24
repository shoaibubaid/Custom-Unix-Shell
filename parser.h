#ifndef PARSER_H
#define PARSER_H

#include "config.h"

enum seq_kind {
    SEQ_NONE,        /* a single pipeline, no separator         */
    SEQ_SERIAL,      /* a && b && c   (one after another)       */
    SEQ_PARALLEL     /* a &&& b &&& c (all at once)             */
};

/* One command of a pipeline: its argv plus optional file redirections. */
struct stage {
    char       **argv;        /* NULL-terminated, ready for execvp()        */
    const char  *in_file;     /* "< file"  (NULL if none)                   */
    const char  *out_file;    /* "> file" or ">> file" (NULL if none)       */
    int          append;      /* 1 for ">>"                                 */
};

/* cmd1 | cmd2 | cmd3 -- a lone command is a pipeline with one stage. */
struct pipeline {
    struct stage stages[MAX_PIPE_STAGES];
    int          nstages;
};

/*
 * A command line split at && / &&& into pipelines. The argv arrays are new,
 * but the strings (and file names) still belong to the token array:
 * free_cmd_list() frees only the arrays, free_tokens() frees the strings.
 */
struct cmd_list {
    struct pipeline pipes[MAX_FG_CMDS];
    int             count;
    enum seq_kind   kind;
};

/*
 * Splits `tokens` at "&&" / "&&&", then each piece at "|", then pulls
 * "< file", "> file" and ">> file" out of every command.
 * Returns 0 on success. On a syntax error (empty command, missing file name,
 * && mixed with &&&, too many commands or stages) prints a message, leaves
 * `out` empty and returns -1.
 */
int  parse_commands(char **tokens, struct cmd_list *out);

void free_cmd_list(struct cmd_list *list);

#endif
