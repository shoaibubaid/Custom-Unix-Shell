#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "tokenizer.h"
#include "vars.h"

/* ---- a small growable string, so expanded values can be any length ---- */

struct sbuf {
    char  *s;
    size_t len;
    size_t cap;
};

static void oom(void)
{
    perror("malloc");
    exit(1);
}

static void sb_init(struct sbuf *b)
{
    b->cap = 64;
    b->len = 0;
    b->s = malloc(b->cap);
    if (b->s == NULL)
        oom();
    b->s[0] = '\0';
}

static void sb_addc(struct sbuf *b, char c)
{
    if (b->len + 2 > b->cap) {
        b->cap *= 2;
        b->s = realloc(b->s, b->cap);
        if (b->s == NULL)
            oom();
    }
    b->s[b->len++] = c;
    b->s[b->len] = '\0';
}

static void sb_adds(struct sbuf *b, const char *s)
{
    for (; *s != '\0'; s++)
        sb_addc(b, *s);
}

/* ---- $ expansion ---- */

/*
 * p points at a '$'. Appends its expansion to `b` and returns how many
 * characters AFTER the '$' were consumed (0 if the '$' stays a literal).
 */
static size_t expand_dollar(const char *p, struct sbuf *b)
{
    char numbuf[256];
    char name[256];

    if (p[1] == '?' || p[1] == '$') {
        name[0] = p[1];
        name[1] = '\0';
        sb_adds(b, lookup_variable(name, numbuf, sizeof(numbuf)));
        return 1;
    }

    if (p[1] == '{') {
        const char *end = strchr(p + 2, '}');
        size_t n = (end != NULL) ? (size_t)(end - (p + 2)) : 0;
        if (end != NULL && n > 0 && n < sizeof(name)) {
            memcpy(name, p + 2, n);
            name[n] = '\0';
            sb_adds(b, lookup_variable(name, numbuf, sizeof(numbuf)));
            return (size_t)(end - p);
        }
        sb_addc(b, '$');
        return 0;
    }

    if (isalpha((unsigned char)p[1]) || p[1] == '_') {
        size_t n = 1;
        while (isalnum((unsigned char)p[1 + n]) || p[1 + n] == '_')
            n++;
        if (n < sizeof(name)) {
            memcpy(name, p + 1, n);
            name[n] = '\0';
            sb_adds(b, lookup_variable(name, numbuf, sizeof(numbuf)));
            return n;
        }
    }

    sb_addc(b, '$');                 /* "$" followed by something else */
    return 0;
}

/* ---- the tokenizer ---- */

char **tokenize(const char *line)
{
    char **tokens = malloc((MAX_NUM_TOKENS + 1) * sizeof(char *));
    struct sbuf b;
    int ntok = 0;
    int started = 0;        /* inside a word                                   */
    int forced = 0;         /* the word had quotes, so "" is a real empty word */
    char quote = 0;         /* 0, '\'' or '"'                                  */

    if (tokens == NULL)
        oom();
    sb_init(&b);

    for (size_t i = 0; ; i++) {
        char c = line[i];

        if (c == '\0') {
            if (quote != 0) {
                fprintf(stderr, "QUOTE MISMATCH\n");
                goto fail;
            }
            if (started && (b.len > 0 || forced)) {
                if (ntok >= MAX_NUM_TOKENS)
                    goto too_many;
                tokens[ntok++] = b.s;
                b.s = NULL;
            }
            break;
        }

        if (quote == '\'') {                      /* everything literal */
            if (c == '\'')
                quote = 0;
            else
                sb_addc(&b, c);
            continue;
        }

        if (quote == '"') {
            if (c == '"')
                quote = 0;
            else if (c == '\\' && (line[i + 1] == '"' || line[i + 1] == '\\' ||
                                   line[i + 1] == '$'))
                sb_addc(&b, line[++i]);
            else if (c == '$')
                i += expand_dollar(line + i, &b);
            else
                sb_addc(&b, c);
            continue;
        }

        /* ---- unquoted ---- */
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (started) {
                if (b.len > 0 || forced) {
                    if (ntok >= MAX_NUM_TOKENS)
                        goto too_many;
                    tokens[ntok++] = b.s;
                    sb_init(&b);
                } else {
                    b.len = 0;
                    b.s[0] = '\0';
                }
                started = 0;
                forced = 0;
            }
            continue;
        }

        if (!started) {
            started = 1;
            /* ~ and ~/... at the start of a word mean $HOME */
            if (c == '~' && (line[i + 1] == '\0' || line[i + 1] == '/' ||
                             line[i + 1] == ' '  || line[i + 1] == '\t' ||
                             line[i + 1] == '\n')) {
                sb_adds(&b, home_dir());
                continue;
            }
        }

        if (c == '\'' || c == '"') {
            quote = c;
            forced = 1;
        } else if (c == '\\') {
            if (line[i + 1] != '\0')
                sb_addc(&b, line[++i]);
        } else if (c == '$') {
            i += expand_dollar(line + i, &b);
        } else {
            sb_addc(&b, c);
        }
    }

    tokens[ntok] = NULL;
    free(b.s);                        /* NULL after a hand-over: free(NULL) is fine */
    return tokens;

too_many:
    fprintf(stderr, "Shell: too many words on one line (max %d)\n", MAX_NUM_TOKENS);
fail:
    tokens[ntok] = NULL;
    free_tokens(tokens);
    free(b.s);
    return NULL;
}

void free_tokens(char **tokens)
{
    for (int i = 0; tokens[i] != NULL; i++)
        free(tokens[i]);
    free(tokens);
}

int strip_background(char **tokens)
{
    int n = 0;
    while (tokens[n] != NULL)
        n++;

    if (n > 0 && strcmp(tokens[n - 1], "&") == 0) {
        free(tokens[n - 1]);
        tokens[n - 1] = NULL;      /* keep the array NULL-terminated */
        return 1;
    }
    return 0;
}
