#ifndef ALIAS_H
#define ALIAS_H

/* Defines or replaces an alias. Returns 0, or -1 if the table is full. */
int         alias_set(const char *name, const char *value);
const char *alias_get(const char *name);   /* NULL if not defined */
int         alias_remove(const char *name);  /* 0 ok, -1 not found */
void        alias_print(void);               /* "alias name='value'" lines */
void        alias_cleanup(void);

#endif
