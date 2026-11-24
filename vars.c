#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>

#include "vars.h"

static int last_status = 0;

void set_last_status(int status) { last_status = status; }
int  get_last_status(void)       { return last_status; }

const char *home_dir(void)
{
    const char *h = getenv("HOME");
    if (h != NULL && *h != '\0')
        return h;

    struct passwd *pw = getpwuid(geteuid());
    if (pw != NULL && pw->pw_dir != NULL)
        return pw->pw_dir;
    return "/";
}

static const char *user_name(void)
{
    struct passwd *pw = getpwuid(geteuid());
    return (pw != NULL && pw->pw_name != NULL) ? pw->pw_name : "";
}

const char *lookup_variable(const char *name, char *buf, size_t buflen)
{
    if (strcmp(name, "?") == 0) {
        snprintf(buf, buflen, "%d", last_status);
        return buf;
    }
    if (strcmp(name, "$") == 0) {
        snprintf(buf, buflen, "%d", (int)getpid());
        return buf;
    }

    const char *v = getenv(name);
    if (v != NULL)
        return v;

    /* Not exported by the environment: fall back like a login shell would. */
    if (strcmp(name, "HOME") == 0)
        return home_dir();
    if (strcmp(name, "USER") == 0)
        return user_name();
    if (strcmp(name, "HOSTNAME") == 0) {
        if (gethostname(buf, buflen) == 0) {
            buf[buflen - 1] = '\0';
            return buf;
        }
        return "";
    }
    if (strcmp(name, "PS1") == 0)
        return geteuid() == 0 ? "#" : "$";

    return "";
}
