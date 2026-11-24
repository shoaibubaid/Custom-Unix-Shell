#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

/* Returns 1 if another background process can be started (limit: MAX_BG_PROCS). */
int  bg_has_room(void);

/* Records a newly started background process. */
void bg_add(pid_t pid);

/*
 * Reaps every terminated child without blocking (waitpid(-1, ..., WNOHANG)),
 * printing "Shell: Background process finished" for each one.
 * Only call this when no foreground command is being waited on.
 */
void reap_background(void);

/*
 * Terminates every background process still in the table with kill(), then
 * waits for each one so none is left as a zombie. Used when the shell exits.
 */
void kill_background(void);

#endif
