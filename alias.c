#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alias.h"
#include "config.h"

static struct { char *name, *value; } table[MAX_ALIASES];
static int n = 0;

static int find(const char *name)
{
    for (int i = 0; i < n; i++)
        if (strcmp(table[i].name, name) == 0)
            return i;
    return -1;
}

int alias_set(const char *name, const char *value)
{
    int i = find(name);
    char *v = strdup(value);
    if (v == NULL)
        return -1;
    if (i >= 0) {
        free(table[i].value);
        table[i].value = v;
        return 0;
    }
    if (n == MAX_ALIASES) {
        free(v);
        return -1;
    }
    table[n].name = strdup(name);
    table[n].value = v;
    n++;
    return 0;
}

const char *alias_get(const char *name)
{
    int i = find(name);
    return i >= 0 ? table[i].value : NULL;
}

int alias_remove(const char *name)
{
    int i = find(name);
    if (i < 0)
        return -1;
    free(table[i].name);
    free(table[i].value);
    table[i] = table[--n];
    return 0;
}

void alias_print(void)
{
    for (int i = 0; i < n; i++)
        printf("alias %s='%s'\n", table[i].name, table[i].value);
}

void alias_cleanup(void)
{
    while (n > 0)
        alias_remove(table[0].name);
}
