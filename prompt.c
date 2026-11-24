#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "prompt.h"
#include "vars.h"

void format_prompt(char *buf, size_t len)
{
    char cwd[MAX_CWD_SIZE + 1];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        strcpy(cwd, "?");
    }
#if PROMPT_STYLE == 1
    char tmp[64], host[64];
    const char *user = lookup_variable("USER", tmp, sizeof(tmp));
    const char *h = lookup_variable("HOSTNAME", host, sizeof(host));
    const char *home = home_dir();
    size_t hl = strlen(home);
    const char *shown = cwd;
    char short_cwd[MAX_CWD_SIZE + 2];
    if (hl > 1 && strncmp(cwd, home, hl) == 0 && (cwd[hl] == '/' || cwd[hl] == '\0')) {
        snprintf(short_cwd, sizeof(short_cwd), "~%s", cwd + hl);
        shown = short_cwd;
    }
    snprintf(buf, len, "%s@%s:%s$ ", user, h, shown);
#else
    snprintf(buf, len, "%s $ ", cwd);
#endif
}
