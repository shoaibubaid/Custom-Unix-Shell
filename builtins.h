#ifndef BUILTINS_H
#define BUILTINS_H

enum builtin_result {
    BUILTIN_NONE = 0,   /* not a built-in: run it as an external command */
    BUILTIN_DONE = 1,   /* built-in handled, keep running the shell      */
    BUILTIN_EXIT = 2    /* "exit": the shell must stop its main loop     */
};

/*
 * Runs `tokens` if it is a built-in (cd, exit, echo, history, alias, unalias,
 * open) and sets $? to 0 or 1. Tells the caller what to do next.
 */
enum builtin_result run_builtin(char **tokens);

/* Returns 1 if `name` is one of the shell's built-in commands. */
int is_builtin(const char *name);

/* Built-ins that change the shell itself (cd, exit, alias, unalias): they
 * make no sense in a child process, so pipelines/background refuse them. */
int is_stateful_builtin(const char *name);

/* i-th built-in name (0, 1, ...), NULL after the last; for Tab completion. */
const char *builtin_name(int i);

#endif
