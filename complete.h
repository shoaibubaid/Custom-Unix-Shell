#ifndef COMPLETE_H
#define COMPLETE_H

/*
 * Tab completion.
 *   - first word of a command (line start, or after | && &&&): built-in names
 *     and every executable found in $PATH
 *   - any other word: names in the current directory (directories get a "/");
 *     hidden files only if the word starts with '.'
 * A word that contains '/' is not completed (names of the current directory only).
 */

/* Stores up to `max` candidates (new strings, free them) for `prefix`. */
int  complete_candidates(const char *prefix, int command_position, char **out, int max);

/* Forget the cached command list (re-read $PATH on next use). */
void complete_refresh(void);
void complete_cleanup(void);

#endif
