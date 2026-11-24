#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alias.h"
#include "builtins.h"
#include "config.h"
#include "rc.h"
#include "tokenizer.h"
#include "vars.h"

static struct { char *ext, *program; } openers[MAX_OPENERS];
static int nopen = 0;

int rc_set_opener(const char *ext, const char *program)
{
    if (ext[0] == '.')
        ext++;
    for (int i = 0; i < nopen; i++) {
        if (strcasecmp(openers[i].ext, ext) == 0) {
            free(openers[i].program);
            openers[i].program = strdup(program);
            return 0;
        }
    }
    if (nopen == MAX_OPENERS)
        return -1;
    openers[nopen].ext = strdup(ext);
    openers[nopen].program = strdup(program);
    nopen++;
    return 0;
}

const char *opener_for(const char *filename)
{
    const char *dot = strrchr(filename, '.');
    if (dot != NULL && dot[1] != '\0') {
        for (int i = 0; i < nopen; i++)
            if (strcasecmp(openers[i].ext, dot + 1) == 0)
                return openers[i].program;
    }
    return "xdg-open";
}

void rc_load(void)
{
    char path[MAX_CWD_SIZE + 64], line[MAX_INPUT_SIZE + 2];
    snprintf(path, sizeof(path), "%s/%s", home_dir(), RC_FILE);
    FILE *f = fopen(path, "r");
    if (f == NULL)
        return;

    while (fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        char *p = line + strspn(line, " \t");
        if (*p == '\0' || *p == '#')
            continue;
        char **t = tokenize(p);
        if (t == NULL || t[0] == NULL) {
            if (t) free_tokens(t);
            continue;
        }
        if (strcmp(t[0], "alias") == 0) {
            run_builtin(t);               /* exactly as if typed */
        } else if (strcmp(t[0], "opener") == 0) {
            if (t[1] == NULL || t[2] == NULL || t[3] != NULL)
                fprintf(stderr, "Shell: %s: usage: opener EXT PROGRAM\n", path);
            else if (rc_set_opener(t[1], t[2]) < 0)
                fprintf(stderr, "Shell: too many openers (max %d)\n", MAX_OPENERS);
        } else {
            fprintf(stderr, "Shell: %s: ignoring unknown setting '%s'\n", path, t[0]);
        }
        free_tokens(t);
    }
    fclose(f);
}

void rc_cleanup(void)
{
    for (int i = 0; i < nopen; i++) {
        free(openers[i].ext);
        free(openers[i].program);
    }
    nopen = 0;
}
