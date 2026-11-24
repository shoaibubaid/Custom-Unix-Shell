#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#include "builtins.h"
#include "complete.h"
#include "config.h"
#include "trie.h"

/* Commands never change while we run (unless PATH does), so the trie is built
 * once, the first time Tab is pressed on a command word. */
static struct trie *commands = NULL;

static void build_commands(void)
{
    commands = trie_new();
    if (commands == NULL)
        return;

    for (int i = 0; builtin_name(i) != NULL; i++)
        trie_insert(commands, builtin_name(i));

    const char *path = getenv("PATH");
    if (path == NULL)
        return;
    char *copy = strdup(path);
    if (copy == NULL)
        return;

    char *save = NULL;
    for (char *dir = strtok_r(copy, ":", &save); dir != NULL; dir = strtok_r(NULL, ":", &save)) {
        DIR *d = opendir(dir);
        if (d == NULL)
            continue;
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            char full[PATH_MAX];
            if (e->d_name[0] == '.')
                continue;
            snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
            if (access(full, X_OK) == 0) {
                struct stat st;
                if (stat(full, &st) == 0 && S_ISREG(st.st_mode))
                    trie_insert(commands, e->d_name);
            }
        }
        closedir(d);
    }
    free(copy);
}

/* The current directory changes with cd, so its trie is made on demand. */
static int complete_files(const char *prefix, char **out, int max)
{
    struct trie *t = trie_new();
    DIR *d = opendir(".");
    int n = 0;

    if (t == NULL || d == NULL) {
        trie_free(t);
        if (d) closedir(d);
        return 0;
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        char name[NAME_MAX + 2];
        struct stat st;

        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        if (e->d_name[0] == '.' && prefix[0] != '.')
            continue;
        snprintf(name, sizeof(name), "%s", e->d_name);
        if (stat(e->d_name, &st) == 0 && S_ISDIR(st.st_mode))
            strncat(name, "/", sizeof(name) - strlen(name) - 1);
        trie_insert(t, name);
    }
    closedir(d);
    n = trie_complete(t, prefix, out, max);
    trie_free(t);
    return n;
}

int complete_candidates(const char *prefix, int command_position, char **out, int max)
{
    if (strchr(prefix, '/') != NULL)
        return 0;
    if (command_position) {
        if (prefix[0] == '\0')
            return 0;                     /* would list every command */
        if (commands == NULL)
            build_commands();
        return commands ? trie_complete(commands, prefix, out, max) : 0;
    }
    return complete_files(prefix, out, max);
}

void complete_refresh(void)
{
    /* Files are read fresh on every Tab, so only PATH commands are cached. */
    trie_free(commands);
    commands = NULL;
}

void complete_cleanup(void)
{
    complete_refresh();
}
