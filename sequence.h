#ifndef SEQUENCE_H
#define SEQUENCE_H

#include "builtins.h"
#include "parser.h"

/*
 * Runs a parsed command line:
 *   SEQ_NONE      one pipeline (background if asked)
 *   SEQ_SERIAL    && : one after another, each fully finished and reaped
 *                 before the next starts, whether it succeeded or not
 *   SEQ_PARALLEL  &&& : all at once, returns when every one has finished
 *
 * A lone built-in (cd, exit, alias, echo, ...) runs inside the shell. Inside
 * a pipeline, a parallel group or the background only echo/history-style
 * built-ins are allowed (they run in a child); cd, exit, alias, unalias are
 * refused there because they must change the shell itself.
 *
 * Ctrl+C in a serial sequence kills the current command and drops the rest.
 * Returns BUILTIN_EXIT if an "exit" command asked the shell to stop,
 * BUILTIN_DONE otherwise.
 */
enum builtin_result run_commands(struct cmd_list *list, int background);

#endif
