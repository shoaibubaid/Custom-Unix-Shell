#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "trie.h"

struct node {
    unsigned char ch;
    int           is_end;      /* a word ends here */
    struct node  *child;       /* first node of the next level */
    struct node  *next;        /* next sibling, sorted by ch */
};

struct trie {
    struct node *root;         /* first node of the first level */
};

struct trie *trie_new(void)
{
    return calloc(1, sizeof(struct trie));
}

/* Finds the node for `ch` in the sibling list *head, creating it in sorted place. */
static struct node *find_or_add(struct node **head, unsigned char ch)
{
    struct node **p = head;
    while (*p != NULL && (*p)->ch < ch)
        p = &(*p)->next;
    if (*p != NULL && (*p)->ch == ch)
        return *p;

    struct node *n = calloc(1, sizeof(*n));
    if (n == NULL)
        return NULL;
    n->ch = ch;
    n->next = *p;
    *p = n;
    return n;
}

static struct node *find(struct node *head, unsigned char ch)
{
    while (head != NULL && head->ch < ch)
        head = head->next;
    return (head != NULL && head->ch == ch) ? head : NULL;
}

void trie_insert(struct trie *t, const char *word)
{
    struct node **level = &t->root;
    struct node *n = NULL;

    if (word[0] == '\0')
        return;
    for (const unsigned char *c = (const unsigned char *)word; *c; c++) {
        n = find_or_add(level, *c);
        if (n == NULL)
            return;
        level = &n->child;
    }
    n->is_end = 1;
}

int trie_contains(const struct trie *t, const char *word)
{
    struct node *n = NULL, *level = t->root;
    if (word[0] == '\0')
        return 0;
    for (const unsigned char *c = (const unsigned char *)word; *c; c++) {
        n = find(level, *c);
        if (n == NULL)
            return 0;
        level = n->child;
    }
    return n->is_end;
}

struct collect {
    char **out;
    int    max, n;
    char   buf[MAX_INPUT_SIZE + 1];
};

/* Depth-first walk of one level; siblings are in order, so output is sorted. */
static void walk(const struct node *level, size_t depth, struct collect *c)
{
    for (; level != NULL && c->n < c->max; level = level->next) {
        if (depth + 1 >= sizeof(c->buf))
            continue;
        c->buf[depth] = (char)level->ch;
        if (level->is_end) {
            c->buf[depth + 1] = '\0';
            char *s = strdup(c->buf);
            if (s == NULL)
                return;
            c->out[c->n++] = s;
        }
        walk(level->child, depth + 1, c);
    }
}

int trie_complete(const struct trie *t, const char *prefix, char **out, int max)
{
    struct collect c;
    struct node *n = NULL, *level = t->root;
    size_t plen = strlen(prefix);

    c.out = out;
    c.max = max;
    c.n = 0;
    if (plen >= sizeof(c.buf) - 1)
        return 0;

    for (size_t i = 0; i < plen; i++) {
        n = find(level, (unsigned char)prefix[i]);
        if (n == NULL)
            return 0;
        c.buf[i] = prefix[i];
        level = n->child;
    }

    if (n != NULL && n->is_end) {            /* the prefix itself is a word */
        c.buf[plen] = '\0';
        out[c.n++] = strdup(c.buf);
    }
    walk(level, plen, &c);
    return c.n;
}

static void free_nodes(struct node *n)
{
    while (n != NULL) {
        struct node *next = n->next;
        free_nodes(n->child);
        free(n);
        n = next;
    }
}

void trie_free(struct trie *t)
{
    if (t != NULL) {
        free_nodes(t->root);
        free(t);
    }
}
