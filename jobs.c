#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "config.h"
#include "jobs.h"

static pid_t bg_pids[MAX_BG_PROCS];   /* background children still alive */
static int   bg_count = 0;

int bg_has_room(void)
{
    return bg_count < MAX_BG_PROCS;
}

void bg_add(pid_t pid)
{
    if (bg_count < MAX_BG_PROCS)
        bg_pids[bg_count++] = pid;
}

static void bg_remove(pid_t pid)
{
    for (int i = 0; i < bg_count; i++) {
        if (bg_pids[i] == pid) {
            bg_pids[i] = bg_pids[--bg_count];   /* swap with last, shrink */
            return;
        }
    }
}

void reap_background(void)
{
    int ws;
    pid_t pid;

    /*
     * WNOHANG: return immediately instead of blocking.
     *   > 0  a child was reaped      -> keep looping for more
     *   == 0 children exist, none finished yet -> stop
     *   < 0  no children at all (ECHILD)       -> stop
     */
    while ((pid = waitpid(-1, &ws, WNOHANG)) > 0) {
        bg_remove(pid);
        printf("Shell: Background process finished\n");
        fflush(stdout);
    }
}

void kill_background(void)
{
    /*
     * SIGKILL cannot be caught or ignored, so the blocking waitpid() below
     * is guaranteed to return. SIGTERM is gentler, but a process that
     * ignores it would make the shell hang on exit.
     */
    /*
     * Each background process leads its own process group (pgid == pid), so
     * kill(-pid) takes down the process and anything it started. The plain
     * kill(pid) is a fallback in case the group does not exist.
     */
    for (int i = 0; i < bg_count; i++) {
        if (kill(-bg_pids[i], SIGKILL) < 0 &&
            kill(bg_pids[i], SIGKILL) < 0 && errno != ESRCH)
            perror("kill");
    }

    /* Reap them all: nothing is left behind as a zombie. */
    for (int i = 0; i < bg_count; i++) {
        while (waitpid(bg_pids[i], NULL, 0) < 0 && errno == EINTR)
            ;
    }
    bg_count = 0;
}
