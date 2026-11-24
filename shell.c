/*
 * shell.c - main loop of the shell
 *
 * start-up:  load history (~/.shell_history) and settings (~/.shellrc)
 * each line: reap finished background jobs -> prompt + line editor
 *   (Tab, history, Ctrl+R) -> save to history -> tokenize (quotes, $vars, ~)
 *   -> strip trailing "&" -> expand aliases / `open` -> split at && / &&&
 *   and | and redirections -> run
 *
 * Ctrl+C does not kill the shell: it is caught and relayed to the
 * foreground children (see signals.c). Background children live in their
 * own process groups, so they never see it.
 *
 * The loop ends on the "exit" command or on Ctrl+D (end of input); either
 * way the shell then kills its background processes and returns from main.
 *
 * Helpers live in their own files:
 *   lineedit.c   read a line: raw-mode editor on a terminal, fgets otherwise
 *   complete.c   Tab candidates (uses trie.c)        history.c  saved history
 *   tokenizer.c  words, quotes, $vars, ~, trailing & alias.c / rc.c  aliases, openers
 *   expand.c     alias and `open` rewriting          vars.c     $? $$ $HOME ...
 *   parser.c     split at && &&& | < > >>            prompt.c   the prompt
 *   sequence.c   run: single, serial (&&), parallel (&&&)
 *   executor.c   fork + exec, pipes, redirections, waiting
 *   builtins.c   cd exit echo history alias unalias open
 *   jobs.c       background process table, reaping, kill on exit
 *   signals.c    SIGINT handler, foreground pid table, signal-mask helpers
 */

#include "shell.h"

int main(void)
{
    char line[MAX_INPUT_SIZE + 1];
    char prompt[2 * MAX_CWD_SIZE + 128];

    install_signal_handlers();
    history_init();
    rc_load();

    while (1) {
        reap_background();      /* report jobs that finished meanwhile */
        format_prompt(prompt, sizeof(prompt));

        enum read_result rr = read_line(prompt, line, sizeof(line));
        if (rr == READ_INTR)
            continue;           /* Ctrl+C at the prompt: fresh prompt */
        if (rr == READ_EOF) {
            printf("\n");       /* Ctrl+D / end of input: leave */
            break;
        }

        reap_background();      /* ...and any that finished while we waited at the prompt */
        history_add(line);

        char **tokens = tokenize(line);
        if (tokens == NULL)                  /* quote mismatch etc., already reported */
            continue;
        int background = strip_background(tokens);

        if (tokens[0] == NULL) {             /* empty line: just re-prompt */
            if (background)
                fprintf(stderr, "Shell: missing command before &\n");
            free_tokens(tokens);
            continue;
        }

        char **expanded = expand_tokens(tokens);
        free_tokens(tokens);
        if (expanded == NULL)
            continue;
        tokens = expanded;

        struct cmd_list list;
        if (parse_commands(tokens, &list) < 0) {   /* syntax error, already reported */
            free_tokens(tokens);
            continue;
        }

        if (background && (list.count > 1 || list.pipes[0].nstages > 1)) {
            fprintf(stderr, "Shell: & works only on a single command (not with &&, &&& or |)\n");
            free_cmd_list(&list);
            free_tokens(tokens);
            continue;
        }

        enum builtin_result res = run_commands(&list, background);

        free_cmd_list(&list);                /* free before we possibly leave */
        free_tokens(tokens);

        if (res == BUILTIN_EXIT)
            break;
    }

    /* ---- shutdown: no foreground command can be running here ---- */
    kill_background();
    history_cleanup();
    alias_cleanup();
    rc_cleanup();
    complete_cleanup();
    return 0;
}
