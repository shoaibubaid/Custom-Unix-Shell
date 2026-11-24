#ifndef TRIE_H
#define TRIE_H

/*
 * A prefix tree (trie). Each node holds one character; words that share a
 * start share nodes, so "all words beginning with ..." is one walk down the
 * prefix plus a walk of what hangs below it.
 *
 * To keep nodes small a node has two links instead of an array of 256:
 * `child` (first letter after this one) and `next` (the next alternative at
 * the same position). Alternatives are kept sorted, so results come out in
 * alphabetical order.
 */
struct trie;

struct trie *trie_new(void);
void         trie_insert(struct trie *t, const char *word);
int          trie_contains(const struct trie *t, const char *word);

/*
 * Stores up to `max` words that start with `prefix` in out[] (each a new
 * string: free() them), alphabetically. Returns how many were stored.
 */
int          trie_complete(const struct trie *t, const char *prefix, char **out, int max);

void         trie_free(struct trie *t);

#endif
