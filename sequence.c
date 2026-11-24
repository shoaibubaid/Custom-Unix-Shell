#include <stdio.h>

#include "config.h"
#include "builtins.h"
#include "executor.h"
#include "sequence.h"
#include "signals.h"
#include "vars.h"

/*
 * Refuses (returns the offending name) a stage that has to change the shell
 * itself but is about to run somewhere else: `where` says where, for the message.
 */
static const char *find_stateful(const struct pipeline *pl)
{
    for (int j = 0; j < pl->nstages; j++) {
        if (is_stateful_builtin(pl->stages[j].argv[0]))
            return pl->stages[j].argv[0];
    }
    return NULL;
}

static int refuse_stateful(const struct pipeline *pl, const char *where)
{
    const char *name = find_stateful(pl);
    if (name == NULL)
        return 0;
    fprintf(stderr, "Shell: built-in '%s' cannot be used %s\n", name, where);
    set_last_status(1);
    return 1;
}

enum builtin_result run_commands(struct cmd_list *list, int background)
{
    sigint_clear();              /* forget any Ctrl+C from before this line */

    if (list->kind == SEQ_PARALLEL) {
        /* Check everything first so nothing starts if part of it is refused. */
        for (int i = 0; i < list->count; i++) {
            if (refuse_stateful(&list->pipes[i], "in parallel (&&&)"))
                return BUILTIN_DONE;
        }
        run_parallel(list->pipes, list->count);
        return BUILTIN_DONE;
    }

    /* SEQ_NONE (one pipeline) and SEQ_SERIAL share this loop. */
    for (int i = 0; i < list->count; i++) {
#if SERIAL_CTRL_C_CANCELS_REST
        if (i > 0 && sigint_received())
            break;               /* Ctrl+C: ignore the rest of the series */
#endif
        struct pipeline *pl = &list->pipes[i];

        if (pl->nstages == 1 && is_builtin(pl->stages[0].argv[0]) && !background) {
            /* A lone built-in runs inside the shell so it can change it. */
            enum builtin_result res = run_builtin_stage(&pl->stages[0]);
            if (res == BUILTIN_EXIT)
                return BUILTIN_EXIT;
            continue;
        }

        if (pl->nstages > 1 && refuse_stateful(pl, "in a pipeline"))
            continue;
        if (background && refuse_stateful(pl, "in the background"))
            continue;

        run_pipeline(pl, background);
    }
    return BUILTIN_DONE;
}
