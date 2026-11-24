#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "builtins.h"
#include "parser.h"

/*
 * Runs one pipeline (a lone command is a pipeline with one stage).
 *
 * Every stage is started with fork; stages are joined with pipe()/dup2(),
 * and "< file", "> file", ">> file" are applied in each child. A built-in
 * that is allowed to (echo, history, ...) runs inside its child.
 *
 * Foreground (background == 0): the children stay in the shell's process
 * group (so they can use the terminal and receive Ctrl+C). The shell waits
 * for every stage with waitpid(pid), then reports the LAST stage's outcome
 * ("EXITSTATUS: <n>" or "TERMINATED BY SIGNAL: <n>") and records it in $?.
 *
 * Background (background == 1, single-stage only): the child is put in its
 * own process group (setpgid) so Ctrl+C never reaches it; it is recorded in
 * the jobs table and reaped later by reap_background().
 */
void run_pipeline(const struct pipeline *pl, int background);

/*
 * Runs `n` pipelines at the same time in the foreground (the &&& form).
 * Everything is started first, then the shell waits for every child before
 * returning, so all are reaped. Ctrl+C is relayed to all of them.
 */
void run_parallel(const struct pipeline *pls, int n);

/*
 * Runs a built-in inside the shell process itself (so cd, exit, alias can
 * change the shell's own state), honouring its < and > redirections.
 */
enum builtin_result run_builtin_stage(const struct stage *st);

#endif
