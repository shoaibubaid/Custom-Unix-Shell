#ifndef RC_H
#define RC_H

/*
 * Reads ~/.shellrc at start-up. One setting per line, '#' starts a comment:
 *     alias ll='ls -l'
 *     opener pdf evince        (what `open file.pdf` runs)
 * A missing file is fine. The file is never modified.
 */
void rc_load(void);

/* Registers an opener (the `opener EXT PROGRAM` setting). 0 ok, -1 table full. */
int rc_set_opener(const char *ext, const char *program);

/* Program for files ending in .ext (case-insensitive); "xdg-open" if none. */
const char *opener_for(const char *filename);

void rc_cleanup(void);

#endif
