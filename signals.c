#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "signals.h"

/* Written by the main program, read by the handler, so they must be
 * volatile sig_atomic_t. A 0 slot means "unused". */
static volatile sig_atomic_t fg_pids[MAX_FG_PIDS];
static volatile sig_atomic_t got_sigint = 0;

/*
 * Only async-signal-safe calls are allowed in here: kill() and write() are,
 * printf() is not.
 */
static void sigint_handler(int sig)
{
    (void)sig;
    int saved_errno = errno;           /* don't disturb the interrupted code */
    int relayed = 0;

    got_sigint = 1;

    for (int i = 0; i < MAX_FG_PIDS; i++) {
        pid_t pid = (pid_t)fg_pids[i];
        if (pid > 0) {
            kill(pid, SIGINT);         /* relay to each foreground child */
            relayed = 1;
        }
    }

    if (!relayed) {
        /* Ctrl+C at the prompt: finish the line so the next prompt is clean */
        ssize_t r = write(STDOUT_FILENO, "\n", 1);
        (void)r;
    }
    errno = saved_errno;
}

void install_signal_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;                   /* no SA_RESTART: fgets()/waitpid() see EINTR */

    if (sigaction(SIGINT, &sa, NULL) < 0)
        perror("sigaction");

    /* Ctrl+\ and Ctrl+Z must not kill or stop the shell. SIG_IGN survives
     * exec: SIGTSTP stays ignored in the commands we start (nothing can be
     * left stopped), while the executor resets SIGQUIT to its default. */
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
}

void fg_register(pid_t pid)
{
    for (int i = 0; i < MAX_FG_PIDS; i++) {
        if (fg_pids[i] == 0) {
            fg_pids[i] = pid;
            return;
        }
    }
}

void fg_unregister(pid_t pid)
{
    for (int i = 0; i < MAX_FG_PIDS; i++) {
        if (fg_pids[i] == pid) {
            fg_pids[i] = 0;
            return;
        }
    }
}

int sigint_received(void)
{
    return got_sigint;
}

void sigint_clear(void)
{
    got_sigint = 0;
}

void block_sigint(sigset_t *saved)
{
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigprocmask(SIG_BLOCK, &set, saved);
}

void restore_sigmask(const sigset_t *saved)
{
    sigprocmask(SIG_SETMASK, saved, NULL);
}
