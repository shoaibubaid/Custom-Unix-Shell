#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

static int is_serial_sep(const char *t)   { return strcmp(t, "&&")  == 0; }
static int is_parallel_sep(const char *t) { return strcmp(t, "&&&") == 0; }
static int is_pipe(const char *t)         { return strcmp(t, "|")   == 0; }

static int is_redirect(const char *t)
{
    return strcmp(t, "<") == 0 || strcmp(t, ">") == 0 || strcmp(t, ">>") == 0;
}

static int is_operator(const char *t)
{
    return is_serial_sep(t) || is_parallel_sep(t) || is_pipe(t) || is_redirect(t);
}

void free_cmd_list(struct cmd_list *list)
{
    for (int i = 0; i < list->count; i++) {
        struct pipeline *pl = &list->pipes[i];
        for (int j = 0; j < pl->nstages; j++) {
            free(pl->stages[j].argv);      /* the array only, not the strings */
            pl->stages[j].argv = NULL;
        }
        pl->nstages = 0;
    }
    list->count = 0;
    list->kind = SEQ_NONE;
}

/* tokens[start .. end) is one command: words plus redirections. */
static int parse_stage(char **tokens, int start, int end, struct stage *st)
{
    memset(st, 0, sizeof(*st));

    char **argv = malloc((size_t)(end - start + 1) * sizeof(char *));
    if (argv == NULL) {
        perror("malloc");
        return -1;
    }

    int nwords = 0;
    for (int i = start; i < end; i++) {
        const char *t = tokens[i];

        if (is_redirect(t)) {
            if (i + 1 >= end || is_operator(tokens[i + 1])) {
                fprintf(stderr, "Shell: syntax error: missing file name after '%s'\n", t);
                free(argv);
                return -1;
            }
            const char *file = tokens[++i];
            if (t[0] == '<') {
                st->in_file = file;
            } else {
                st->out_file = file;
                st->append = (t[1] == '>');
            }
        } else {
            argv[nwords++] = tokens[i];
        }
    }

    if (nwords == 0) {
        fprintf(stderr, "Shell: syntax error: empty command (nothing to run next to | or a redirection)\n");
        free(argv);
        return -1;
    }
    argv[nwords] = NULL;
    st->argv = argv;
    return 0;
}

/* tokens[start .. end) is a pipeline: stages separated by "|". */
static int parse_pipeline(char **tokens, int start, int end, struct pipeline *pl)
{
    pl->nstages = 0;
    int s = start;

    for (int i = start; ; i++) {
        int at_end = (i == end);
        if (!at_end && !is_pipe(tokens[i]))
            continue;

        if (pl->nstages >= MAX_PIPE_STAGES) {
            fprintf(stderr, "Shell: too many commands in one pipeline (max %d)\n",
                    MAX_PIPE_STAGES);
            return -1;
        }
        if (parse_stage(tokens, s, i, &pl->stages[pl->nstages]) < 0)
            return -1;
        pl->nstages++;

        if (at_end)
            break;
        s = i + 1;
    }
    return 0;
}

int parse_commands(char **tokens, struct cmd_list *out)
{
    out->count = 0;
    out->kind = SEQ_NONE;

    int start = 0;                       /* first token of the current piece */

    for (int i = 0; ; i++) {
        char *t = tokens[i];
        int is_sep = (t != NULL) && (is_serial_sep(t) || is_parallel_sep(t));

        if (t != NULL && !is_sep)
            continue;                    /* ordinary word: keep scanning */

        /* tokens[start .. i) is one pipeline */
        if (i - start == 0) {
            fprintf(stderr, "Shell: syntax error: empty command next to && or &&&\n");
            goto fail;
        }
        if (out->count >= MAX_FG_CMDS) {
            fprintf(stderr, "Shell: too many commands (max %d)\n", MAX_FG_CMDS);
            goto fail;
        }

        struct pipeline *pl = &out->pipes[out->count];
        pl->nstages = 0;
        out->count++;                    /* counted first, so a failure frees it */
        if (parse_pipeline(tokens, start, i, pl) < 0)
            goto fail;

        if (t == NULL)                   /* end of line */
            break;

        enum seq_kind k = is_serial_sep(t) ? SEQ_SERIAL : SEQ_PARALLEL;
        if (out->kind != SEQ_NONE && out->kind != k) {
            fprintf(stderr, "Shell: cannot mix && and &&& on one line\n");
            goto fail;
        }
        out->kind = k;
        start = i + 1;
    }
    return 0;

fail:
    free_cmd_list(out);
    return -1;
}
