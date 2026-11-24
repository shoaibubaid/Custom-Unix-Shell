#ifndef CONFIG_H
#define CONFIG_H

#define MAX_INPUT_SIZE  1024  /* max chars typed on one command line        */
#define MAX_NUM_TOKENS  256   /* max words on one line (64 commands plus
                                 their separators need more than 127)       */
#define MAX_CWD_SIZE    256   /* max chars in the cwd                       */
#define MAX_BG_PROCS    64    /* max background processes at once           */
#define MAX_FG_CMDS     64    /* max commands in one && / &&& line          */
#define MAX_PIPE_STAGES 32    /* max commands in one pipeline (a | b | c)   */
#define MAX_FG_PIDS     (MAX_FG_CMDS * MAX_PIPE_STAGES)

#define HISTORY_SIZE    100   /* commands kept in the history               */
#define HISTORY_FILE    ".shell_history"   /* in $HOME                      */
#define RC_FILE         ".shellrc"         /* in $HOME: aliases, openers    */
#define MAX_ALIASES     64
#define MAX_OPENERS     32
#define MAX_COMPLETIONS 512   /* candidates collected for one Tab press     */

/*
 * Prompt style:
 *   0 = "<cwd> $ "                 (what the assignment asks for)
 *   1 = "user@host:~/dir$ "        (the original repository's style)
 */
#ifndef PROMPT_STYLE
#define PROMPT_STYLE 0
#endif

/*
 * What Ctrl+C does during a serial (&&) series:
 *   1 = kill the current command and drop the rest of the series (required)
 *   0 = kill only the current command and carry on with the next one
 */
#ifndef SERIAL_CTRL_C_CANCELS_REST
#define SERIAL_CTRL_C_CANCELS_REST 1
#endif

#endif
