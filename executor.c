#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "config.h"
#include "executor.h"
#include "jobs.h"
#include "signals.h"
#include "vars.h"

/* ---- redirections ---- */

/* Applies "< file" and "> file" / ">> file" to this process's fd 0 and 1. */
static int apply_redirections(const struct stage *st)
{
    if (st->in_file != NULL) {
        int fd = open(st->in_file, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "Shell: %s: %s\n", st->in_file, strerror(errno));
            return -1;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (st->out_file != NULL) {
        int flags = O_WRONLY | O_CREAT | (st->append ? O_APPEND : O_TRUNC);
        int fd = open(st->out_file, flags, 0666);
        if (fd < 0) {
            fprintf(stderr, "Shell: %s: %s\n", st->out_file, strerror(errno));
            return -1;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    return 0;
}

/* ---- starting processes ---- */

/*
 * fork + (pipe wiring) + (redirections) + exec/built-in. No waiting.
 * Returns the child's pid, or -1 if fork failed.
 *
 * The caller must have blocked SIGINT (the old mask is `saved`): that way a
 * Ctrl+C cannot arrive before the caller has recorded the pid, and cannot hit
 * the child while it still has the shell's handler installed.
 *
 * in_fd / out_fd are the pipe ends for this stage (-1 = none). pipe_fds lists
 * every pipe fd so the child can close them all after wiring its own.
 */
static pid_t spawn_stage(const struct stage *st, int background,
                         int in_fd, int out_fd,
                         const int *pipe_fds, int npipe_fds,
                         const sigset_t *saved)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        /* ---- child ---- */
        signal(SIGINT, SIG_DFL);        /* undo the shell's handler       */
        signal(SIGQUIT, SIG_DFL);       /* (SIGTSTP stays ignored)        */

        if (background)
            setpgid(0, 0);              /* new group, pgid == own pid     */

        restore_sigmask(saved);         /* unblock SIGINT (default action now) */

        if (in_fd >= 0)
            dup2(in_fd, STDIN_FILENO);
        if (out_fd >= 0)
            dup2(out_fd, STDOUT_FILENO);
        for (int i = 0; i < npipe_fds; i++)
            close(pipe_fds[i]);

        if (apply_redirections(st) < 0)  /* files override the pipe, like bash */
            _exit(1);

        /* Built-ins that don't need to change the shell run right here. */
        if (is_builtin(st->argv[0])) {
            run_builtin(st->argv);
            fflush(stdout);
            _exit(get_last_status());
        }

        execvp(st->argv[0], st->argv);

        /* execvp only returns on failure */
        fprintf(stderr, "%s: %s\n", st->argv[0],
                errno == ENOENT ? "command not found" : strerror(errno));
        _exit(1);                /* _exit: don't flush parent's stdio copy */
    }

    /* ---- parent ---- */
    if (background) {
        /* Also set it from the parent: whichever of parent/child runs first
         * makes the group, so there is no window without it. (After the
         * child has exec'd this call fails with EACCES, which is harmless.) */
        setpgid(pid, pid);
    }
    return pid;
}

/*
 * Starts every stage of `pl`, connected by pipes. Fills pids[] (-1 for a stage
 * whose fork failed) and returns the stage count, or -1 if pipe() failed.
 * Foreground children are registered so Ctrl+C is relayed to them.
 * The caller must have blocked SIGINT.
 */
static int start_pipeline(const struct pipeline *pl, int background,
                          pid_t *pids, const sigset_t *saved)
{
    int n = pl->nstages;
    int fds[2 * MAX_PIPE_STAGES];
    int nfds = 0;

    /* A child that runs a built-in calls fflush(stdout): make sure it
     * cannot flush text the shell itself had buffered before the fork. */
    fflush(stdout);

    for (int i = 0; i < n - 1; i++) {
        if (pipe(&fds[2 * i]) < 0) {
            perror("pipe");
            for (int j = 0; j < nfds; j++)
                close(fds[j]);
            return -1;
        }
        nfds += 2;
    }

    for (int i = 0; i < n; i++) {
        int in_fd  = (i > 0)     ? fds[2 * (i - 1)]     : -1;
        int out_fd = (i < n - 1) ? fds[2 * i + 1]       : -1;

        pids[i] = spawn_stage(&pl->stages[i], background, in_fd, out_fd,
                              fds, nfds, saved);
        if (pids[i] > 0 && !background)
            fg_register(pids[i]);
    }

    /* The shell must not keep any pipe end open, or readers never see EOF. */
    for (int j = 0; j < nfds; j++)
        close(fds[j]);

    return n;
}

/* ---- waiting ---- */

/* Records `ws` in $? and tells the user if the command failed or was killed. */
static void report_status(int ws)
{
    if (WIFEXITED(ws)) {
        int code = WEXITSTATUS(ws);
        set_last_status(code);
        if (code != 0)           /* change to always print if you prefer */
            printf("EXITSTATUS: %d\n", code);
    } else if (WIFSIGNALED(ws)) {
        set_last_status(128 + WTERMSIG(ws));
        printf("TERMINATED BY SIGNAL: %d\n", WTERMSIG(ws));
    }
    fflush(stdout);              /* keep our messages in order with the children's output */
}

/*
 * Waits for every stage (one waitpid per pid, never a plain wait(): that could
 * hand back a finished background child), so all are reaped. EINTR just means
 * our SIGINT handler ran, so wait again. Only the last stage is reported.
 */
static void wait_pipeline(const pid_t *pids, int n)
{
    int last_ws = 0;
    int have_last = 0;

    for (int i = 0; i < n; i++) {
        if (pids[i] <= 0)
            continue;

        int ws;
        while (waitpid(pids[i], &ws, 0) < 0) {
            if (errno != EINTR) {
                perror("waitpid");
                ws = 0;
                break;
            }
        }
        fg_unregister(pids[i]);          /* reaped: nothing to relay to any more */

        if (i == n - 1) {
            last_ws = ws;
            have_last = 1;
        }
    }

    if (have_last)
        report_status(last_ws);
    else
        set_last_status(1);              /* the last stage never started */
}

/* ---- public entry points ---- */

void run_pipeline(const struct pipeline *pl, int background)
{
    if (background && !bg_has_room()) {
        fprintf(stderr, "Shell: too many background processes\n");
        set_last_status(1);
        return;
    }

    pid_t pids[MAX_PIPE_STAGES];
    sigset_t saved;

    block_sigint(&saved);
    int n = start_pipeline(pl, background, pids, &saved);
    restore_sigmask(&saved);

    if (n < 0) {
        set_last_status(1);
        return;
    }

    if (background) {
        if (pids[0] > 0)
            bg_add(pids[0]);     /* don't wait; reap_background() does it later */
        return;
    }

    wait_pipeline(pids, n);
}

void run_parallel(const struct pipeline *pls, int n)
{
    pid_t pids[MAX_FG_CMDS][MAX_PIPE_STAGES];
    int   counts[MAX_FG_CMDS];
    sigset_t saved;

    /* Start everything with SIGINT blocked and register every child before
     * unblocking: a Ctrl+C during startup is then relayed to all of them. */
    block_sigint(&saved);
    for (int i = 0; i < n; i++)
        counts[i] = start_pipeline(&pls[i], 0, pids[i], &saved);
    restore_sigmask(&saved);

    /* Wait for every child of every pipeline so all get reaped. */
    for (int i = 0; i < n; i++) {
        if (counts[i] >= 0)
            wait_pipeline(pids[i], counts[i]);
    }
}

enum builtin_result run_builtin_stage(const struct stage *st)
{
    int saved_in = -1, saved_out = -1;
    enum builtin_result res = BUILTIN_DONE;

    fflush(stdout);
    if (st->in_file != NULL)
        saved_in = dup(STDIN_FILENO);
    if (st->out_file != NULL)
        saved_out = dup(STDOUT_FILENO);

    if (apply_redirections(st) < 0)
        set_last_status(1);
    else
        res = run_builtin(st->argv);

    fflush(stdout);                      /* push the output into the file now */
    if (saved_in >= 0) {
        dup2(saved_in, STDIN_FILENO);
        close(saved_in);
    }
    if (saved_out >= 0) {
        dup2(saved_out, STDOUT_FILENO);
        close(saved_out);
    }
    return res;
}
