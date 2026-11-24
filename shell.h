#ifndef SHELL_H
#define SHELL_H

/* Umbrella header: everything the shell needs, in one include. */

/* ---- system headers ---- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/* ---- project headers ---- */
#include "config.h"
#include "tokenizer.h"
#include "parser.h"
#include "sequence.h"
#include "prompt.h"
#include "vars.h"
#include "history.h"
#include "alias.h"
#include "rc.h"
#include "expand.h"
#include "complete.h"
#include "lineedit.h"
#include "builtins.h"
#include "executor.h"
#include "jobs.h"
#include "signals.h"

#endif
