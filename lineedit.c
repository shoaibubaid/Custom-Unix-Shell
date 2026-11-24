#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include "complete.h"
#include "config.h"
#include "history.h"
#include "lineedit.h"
#include "signals.h"

#define KEY_CTRL_C 3
#define KEY_CTRL_D 4
#define KEY_CTRL_G 7
#define KEY_TAB    9
#define KEY_ENTER  10
#define KEY_CR     13
#define KEY_CTRL_R 18
#define KEY_CTRL_U 21
#define KEY_ESC    27
#define KEY_DEL    127

#define GOT_EOF   (-1)    /* read failed or input closed (errno says why) */
#define GOT_NONE  (-2)    /* timeout */

static void out(const char *s)
{
    size_t len = strlen(s);
    while (len > 0) {
        ssize_t w = write(STDOUT_FILENO, s, len);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        s += w;
        len -= (size_t)w;
    }
}

/* One byte from the keyboard; timeout_ms < 0 waits forever. */
static int get_key(int timeout_ms)
{
    unsigned char c;

    if (timeout_ms >= 0) {
        fd_set fds;
        struct timeval tv = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        int r = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
        if (r == 0)
            return GOT_NONE;
        if (r < 0)
            return GOT_EOF;
    }
    ssize_t r = read(STDIN_FILENO, &c, 1);
    return r == 1 ? c : GOT_EOF;
}

/* Redraws "prompt + text" on the current line. */
static void redraw(const char *prompt, const char *text)
{
    out("\r");
    out(prompt);
    out(text);
    out("\033[K");                         /* erase to end of line */
}

/* ---- Tab completion ---- */

static int is_separator(const char *w, size_t len)
{
    return (len == 1 && w[0] == '|') || (len == 2 && strncmp(w, "&&", 2) == 0)
        || (len == 3 && strncmp(w, "&&&", 3) == 0);
}

/* Is the word that starts at buf[start] the first word of a command? */
static int at_command_position(const char *buf, size_t start)
{
    size_t e = start;
    while (e > 0 && buf[e - 1] == ' ')
        e--;
    if (e == 0)
        return 1;
    size_t b = e;
    while (b > 0 && buf[b - 1] != ' ')
        b--;
    return is_separator(buf + b, e - b);
}

static void do_tab(const char *prompt, char *buf, size_t size, size_t *len)
{
    size_t start = *len;
    while (start > 0 && buf[start - 1] != ' ' && buf[start - 1] != '\t')
        start--;

    const char *prefix = buf + start;
    size_t plen = *len - start;
    char *cands[MAX_COMPLETIONS];
    int n = complete_candidates(prefix, at_command_position(buf, start), cands, MAX_COMPLETIONS);

    if (n == 0) {
        out("\a");
        return;
    }

    /* longest common start of all candidates */
    size_t common = strlen(cands[0]);
    for (int i = 1; i < n; i++) {
        size_t k = 0;
        while (k < common && cands[i][k] == cands[0][k])
            k++;
        common = k;
    }

    int changed = 0;
    if (common > plen) {
        size_t add = common - plen;
        if (*len + add < size) {
            memcpy(buf + *len, cands[0] + plen, add);
            *len += add;
            changed = 1;
        }
    }
    if (n == 1 && cands[0][common - 1] != '/' && *len + 1 < size)
        buf[(*len)++] = ' ';               /* finished word: ready for the next */
    buf[*len] = '\0';

    if (n > 1 && !changed) {               /* ambiguous: show the choices */
        out("\n");
        for (int i = 0; i < n; i++) {
            out(cands[i]);
            out("  ");
        }
        out("\n");
    }
    redraw(prompt, buf);

    for (int i = 0; i < n; i++)
        free(cands[i]);
}

/* ---- Ctrl+R ---- */

/*
 * Returns 1 if the user accepted a line (buf holds it), 0 if cancelled.
 * Finding: newest history entry containing the typed text; Ctrl+R again
 * goes to the next older match. Enter runs it, Esc/Ctrl+G cancels, any
 * other control key accepts the match for further editing.
 * *interrupted is set if Ctrl+C arrived.
 */
static int reverse_search(const char *prompt, char *buf, size_t size, size_t *len,
                          int *run_now, int *interrupted)
{
    char query[MAX_INPUT_SIZE + 1] = "";
    size_t qlen = 0;
    int pos = history_count();            /* search starts below this index */
    int found = -1;
    (void)prompt;

    *run_now = 0;
    for (;;) {
        char line[MAX_INPUT_SIZE + 64];
        const char *m = found >= 0 ? history_get(found) : "";
        snprintf(line, sizeof(line), "(reverse-i-search)`%s': %s", query, m);
        out("\r");
        out(line);
        out("\033[K");

        int c = get_key(-1);
        if (c == GOT_EOF) {
            if (errno == EINTR)
                *interrupted = 1;
            return 0;
        }
        if (c == KEY_ESC || c == KEY_CTRL_G)
            return 0;
        if (c == KEY_CTRL_R) {
            for (int i = (found >= 0 ? found : pos) - 1; i >= 0; i--) {
                if (strstr(history_get(i), query) != NULL) {
                    found = i;
                    break;
                }
            }
            continue;
        }
        if (c == KEY_DEL || c == 8) {
            if (qlen > 0)
                query[--qlen] = '\0';
        } else if (c >= 32 && c < 127) {
            if (qlen + 1 < sizeof(query)) {
                query[qlen++] = (char)c;
                query[qlen] = '\0';
            }
        } else {                           /* Enter or another control key: accept */
            if (found >= 0) {
                snprintf(buf, size, "%s", history_get(found));
                *len = strlen(buf);
            }
            *run_now = (c == KEY_ENTER || c == KEY_CR) && found >= 0;
            return 1;
        }

        /* the text changed: search again from the newest entry */
        found = -1;
        if (qlen > 0) {
            for (int i = history_count() - 1; i >= 0; i--) {
                if (strstr(history_get(i), query) != NULL) {
                    found = i;
                    break;
                }
            }
        }
    }
}

/* ---- the editor ---- */

static enum read_result edit_line(const char *prompt, char *buf, size_t size)
{
    size_t len = 0;
    int hpos = history_count();           /* == count: the line being typed */
    char draft[MAX_INPUT_SIZE + 1] = "";

    buf[0] = '\0';
    out(prompt);

    for (;;) {
        int c = get_key(-1);

        if (c == GOT_EOF) {
            if (errno == EINTR)            /* Ctrl+C: handler printed the newline */
                return READ_INTR;
            return READ_EOF;
        }

        switch (c) {
        case KEY_ENTER:
        case KEY_CR:
            out("\n");
            buf[len] = '\0';
            return READ_OK;

        case KEY_CTRL_D:
            if (len == 0)
                return READ_EOF;
            break;

        case KEY_DEL:
        case 8:
            if (len > 0) {
                buf[--len] = '\0';
                redraw(prompt, buf);
            }
            break;

        case KEY_CTRL_U:
            len = 0;
            buf[0] = '\0';
            redraw(prompt, buf);
            break;

        case KEY_TAB:
            do_tab(prompt, buf, size, &len);
            break;

        case KEY_CTRL_R: {
            int run_now = 0, intr = 0;
            int accepted = reverse_search(prompt, buf, size, &len, &run_now, &intr);
            if (intr)                      /* the signal handler already wrote the newline */
                return READ_INTR;
            if (!accepted) {               /* cancelled: keep what was typed */
                buf[len] = '\0';
            }
            if (run_now) {
                redraw(prompt, buf);
                out("\n");
                return READ_OK;
            }
            redraw(prompt, buf);
            break;
        }

        case KEY_ESC: {
            int c1 = get_key(50);
            if (c1 != '[' && c1 != 'O')
                break;                     /* lone Esc or Alt+key: ignore */
            int c2 = get_key(50);
            if (c2 == 'A' || c2 == 'B') {
                int cnt = history_count();
                if (c2 == 'A' && hpos > 0) {
                    if (hpos == cnt) {     /* leaving the draft: remember it */
                        memcpy(draft, buf, len);
                        draft[len] = '\0';
                    }
                    hpos--;
                } else if (c2 == 'B' && hpos < cnt) {
                    hpos++;
                } else {
                    break;
                }
                const char *src = (hpos == cnt) ? draft : history_get(hpos);
                snprintf(buf, size, "%s", src);
                len = strlen(buf);
                redraw(prompt, buf);
            } else {
                /* other arrows / Delete / Home / End: swallow "1;5C", "3~" ... */
                while ((c2 >= '0' && c2 <= '9') || c2 == ';')
                    c2 = get_key(50);
            }
            break;
        }

        default:
            if (c >= 32 && c < 127 && len + 1 < size) {
                buf[len++] = (char)c;
                buf[len] = '\0';
                char one[2] = { (char)c, '\0' };
                out(one);
            }
            break;
        }
    }
}

enum read_result read_line(const char *prompt, char *buf, size_t size)
{
    fflush(stdout); // forces the computer to immediately print any text waiting in the output buffer to your screen.

    if (!isatty(STDIN_FILENO)) {
        out(prompt);
        if (fgets(buf, (int)size, stdin) == NULL) {
            if (ferror(stdin) && errno == EINTR) {
                clearerr(stdin);           /* Ctrl+C: handler ended the line */
                return READ_INTR;
            }
            return READ_EOF;
        }
        buf[strcspn(buf, "\n")] = '\0';
        return READ_OK;
    }

    struct termios saved, raw;
    if (tcgetattr(STDIN_FILENO, &saved) < 0) {
        /* odd terminal: fall back to the cooked line */
        out(prompt);
        if (fgets(buf, (int)size, stdin) == NULL)
            return READ_EOF;
        buf[strcspn(buf, "\n")] = '\0';
        return READ_OK;
    }
    raw = saved;
    raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO);   /* keep ISIG: Ctrl+C still signals */
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    enum read_result r = edit_line(prompt, buf, size);

    tcsetattr(STDIN_FILENO, TCSANOW, &saved);
    return r;
}
