#ifndef HISTORY_H
#define HISTORY_H

/* Loads ~/.shell_history (the last HISTORY_SIZE commands). */
void history_init(void);

/* Adds a command; blank lines and a repeat of the previous command are
 * skipped. The file is updated right away, so history survives a crash. */
void history_add(const char *line);

int         history_count(void);
const char *history_get(int i);        /* 0 = oldest; NULL if out of range */

/* Prints "  1  command" lines (the `history` built-in). */
void history_print(void);

void history_cleanup(void);

#endif
