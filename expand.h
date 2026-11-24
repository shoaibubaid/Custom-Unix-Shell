#ifndef EXPAND_H
#define EXPAND_H

/*
 * Rewrites the first word of every command (the start of the line and the word
 * after |, && and &&&):
 *   - an alias is replaced by its value (aliases may use other aliases,
 *     but never themselves)
 *   - `open FILE...` becomes `<program for FILE's extension> FILE...`
 *     (see rc.h); `open` with nothing after it is left for the built-in
 *     to explain.
 * Returns a NEW token array (free it with free_tokens()); the input array is
 * left untouched. Returns NULL only if out of memory.
 */
char **expand_tokens(char **tokens);

#endif
