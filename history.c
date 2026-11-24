#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "history.h"
#include "vars.h"

static char *entries[HISTORY_SIZE];
static int   count = 0;

static void history_path(char *buf, size_t len)
{
    snprintf(buf, len, "%s/%s", home_dir(), HISTORY_FILE);
}

static void add_entry(const char *line)
{
    if (count == HISTORY_SIZE) {          /* full: drop the oldest */
        free(entries[0]);
        memmove(entries, entries + 1, sizeof(entries[0]) * (HISTORY_SIZE - 1));
        count--;
    }
    entries[count] = strdup(line);
    if (entries[count] != NULL)
        count++;
}

static void save_all(void)
{
    char path[MAX_CWD_SIZE + 64];
    history_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (f == NULL)
        return;                           /* read-only home: just no persistence */
    for (int i = 0; i < count; i++)
        fprintf(f, "%s\n", entries[i]);
    fclose(f);
}

void history_init(void)
{
    char path[MAX_CWD_SIZE + 64], line[MAX_INPUT_SIZE + 2];
    history_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return;
    while (fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] != '\0')
            add_entry(line);
    }
    fclose(f);
}

void history_add(const char *line)
{
    size_t skip = strspn(line, " \t");
    if (line[skip] == '\0')
        return;                           /* blank */
    if (count > 0 && strcmp(entries[count - 1], line) == 0)
        return;                           /* same as the last one */
    add_entry(line);
    save_all();
}

int history_count(void) { return count; }

const char *history_get(int i)
{
    return (i >= 0 && i < count) ? entries[i] : NULL;
}

void history_print(void)
{
    for (int i = 0; i < count; i++)
        printf("%4d  %s\n", i + 1, entries[i]);
}

void history_cleanup(void)
{
    for (int i = 0; i < count; i++)
        free(entries[i]);
    count = 0;
}
