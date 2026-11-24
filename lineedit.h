#ifndef LINEEDIT_H
#define LINEEDIT_H

#include <stddef.h>

enum read_result {
    READ_OK,       /* a line is in buf (possibly empty)               */
    READ_EOF,      /* Ctrl+D on an empty line / end of input          */
    READ_INTR      /* Ctrl+C: line abandoned, show a fresh prompt     */
};

/*
 * Prints `prompt` and reads one line (without the newline) into buf.
 *
 * On a terminal it is a small line editor (raw mode):
 *   Backspace   delete the last character     Ctrl+U  clear the line
 *   Tab         complete (see complete.h)     Ctrl+D  quit on an empty line
 *   Up / Down   walk through the history      Ctrl+R  reverse history search
 * When input is not a terminal (pipe, file) it is a plain fgets().
 * Terminal settings are restored before this function returns, so commands
 * always run with the normal terminal behaviour.
 */
enum read_result read_line(const char *prompt, char *buf, size_t size);

#endif
