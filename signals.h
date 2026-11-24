#ifndef SIGNALS_H
#define SIGNALS_H

#include <signal.h>
#include <sys/types.h>

/*
 * Installs the shell's signal handling:
 *   SIGINT  (Ctrl+C)   caught: relayed to every registered foreground child
 *                      (or just starts a fresh line if there is none), and
 *                      recorded. Installed without SA_RESTART so a blocked
 *                      read/wait returns EINTR instead of silently resuming.
 *   SIGQUIT (Ctrl+\)   ignored by the shell; children get the default action
 *   SIGTSTP (Ctrl+Z)   ignored by the shell and by the commands it starts,
 *                      so nothing is ever left stopped (there is no job control)
 */
void install_signal_handlers(void);

/* Foreground children the handler must relay Ctrl+C to (up to MAX_FG_PIDS). */
void fg_register(pid_t pid);
void fg_unregister(pid_t pid);

/*
 * Has Ctrl+C been pressed since sigint_clear()? A serial sequence checks
 * this between commands so it can drop the rest of the series.
 */
int  sigint_received(void);
void sigint_clear(void);

/* Blocks SIGINT, saving the previous mask in *saved. */
void block_sigint(sigset_t *saved);

/* Restores a mask saved by block_sigint(). */
void restore_sigmask(const sigset_t *saved);

#endif
