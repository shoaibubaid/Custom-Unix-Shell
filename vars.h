#ifndef VARS_H
#define VARS_H

#include <stddef.h>

/* Exit status of the last foreground command: the value of $? */
void set_last_status(int status);
int  get_last_status(void);

/* The user's home directory: $HOME, else the passwd entry, else "/". */
const char *home_dir(void);

/*
 * Value of a $-variable. `name` is "?" (last status), "$" (the shell's pid),
 * or an environment variable name. USER, HOME, HOSTNAME and PS1 have
 * fallbacks when the environment does not provide them. An unknown name
 * gives "". The result is either static, in the environment, or `buf`.
 */
const char *lookup_variable(const char *name, char *buf, size_t buflen);

#endif
