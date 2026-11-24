#include <stdlib.h>
#include <string.h>

#include "alias.h"
#include "expand.h"
#include "rc.h"
#include "tokenizer.h"

#define MAX_ALIAS_DEPTH 8

static int is_separator(const char *t)
{
    return strcmp(t, "|") == 0 || strcmp(t, "&&") == 0 || strcmp(t, "&&&") == 0;
}

static int count_tokens(char **t)
{
    int n = 0;
    while (t[n] != NULL)
        n++;
    return n;
}

/* Replaces `word` (one token at *pos) by the alias value's words. */
static char **replace_with_alias(char **t, int pos, const char *value)
{
    char **words = tokenize(value);
    if (words == NULL)
        return t;                         /* broken alias text: leave as is */

    int nw = count_tokens(words), nt = count_tokens(t);
    char **r = malloc(sizeof(char *) * (nt - 1 + nw + 1));
    if (r == NULL) {
        free_tokens(words);
        return t;
    }
    int k = 0;
    for (int i = 0; i < pos; i++)
        r[k++] = t[i];
    for (int i = 0; i < nw; i++)
        r[k++] = words[i];
    for (int i = pos + 1; i < nt; i++)
        r[k++] = t[i];
    r[k] = NULL;
    free(t[pos]);
    free(t);
    free(words);                          /* strings now owned by r */
    return r;
}

char **expand_tokens(char **tokens)
{
    int n = count_tokens(tokens);
    char **t = malloc(sizeof(char *) * (n + 1));
    if (t == NULL)
        return NULL;
    for (int i = 0; i < n; i++)
        t[i] = strdup(tokens[i]);
    t[n] = NULL;

    int at_start = 1;
    for (int i = 0; t[i] != NULL; i++) {
        if (is_separator(t[i])) {
            at_start = 1;
            continue;
        }
        if (!at_start)
            continue;
        at_start = 0;

        /* alias chain: ll -> ls -l -> ..., each name used at most once */
        char *seen[MAX_ALIAS_DEPTH];
        int nseen = 0;
        const char *val;
        while (nseen < MAX_ALIAS_DEPTH && (val = alias_get(t[i])) != NULL) {
            int again = 0;
            for (int s = 0; s < nseen; s++)
                if (strcmp(seen[s], t[i]) == 0)
                    again = 1;
            if (again)
                break;
            seen[nseen] = strdup(t[i]);
            t = replace_with_alias(t, i, val);
            nseen++;
            if (t[i] == NULL || is_separator(t[i]))
                break;
        }
        for (int s = 0; s < nseen; s++)
            free(seen[s]);

        /* open FILE -> program FILE */
        if (t[i] != NULL && strcmp(t[i], "open") == 0 && t[i + 1] != NULL
            && !is_separator(t[i + 1])) {
            const char *prog = opener_for(t[i + 1]);
            char *copy = strdup(prog);
            if (copy != NULL) {
                free(t[i]);
                t[i] = copy;
            }
        }
    }
    return t;
}
