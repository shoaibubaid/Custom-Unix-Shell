#ifndef TOKENIZER_H
#define TOKENIZER_H

/*
 * Splits `line` into words.
 *
 *   - words are separated by spaces and tabs
 *   - 'single quotes' keep everything literally, "double quotes" keep spaces
 *     but still expand variables; \x makes the next character literal
 *   - $NAME, ${NAME}, $? (last exit status) and $$ (shell pid) are replaced
 *     by their values (not inside single quotes)
 *   - an unquoted word that starts with ~ or ~/ has the ~ replaced by $HOME
 *
 * Returns a NULL-terminated array of strings: tokens[0] is the command, the
 * rest are its arguments. Release it with free_tokens().
 * Returns NULL (after printing a message) on a quote mismatch or when the
 * line has too many words.
 */
char **tokenize(const char *line);
void   free_tokens(char **tokens);

/*
 * If the last token is "&", removes it from `tokens` and returns 1
 * (background command). Otherwise leaves `tokens` alone and returns 0.
 */
int    strip_background(char **tokens);

#endif
