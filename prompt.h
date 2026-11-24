#ifndef PROMPT_H
#define PROMPT_H

#include <stddef.h>

/* Writes the prompt into buf: "<cwd> $ " (PROMPT_STYLE 1: "user@host:~/dir$ "). */
void format_prompt(char *buf, size_t len);

#endif
