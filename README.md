# shell — a Unix shell in C

A learning rebuild of a small Unix shell (originally C++), written in plain C with POSIX calls.

```
make          # build ./shell
make run      # build and start it
make test     # 95 piped checks + 38 Ctrl+C pty checks + 25 line-editor pty checks
make clean
```

Optional switches in `config.h`:
`PROMPT_STYLE=1` (prompt `user@host:~/dir$`), `SERIAL_CTRL_C_CANCELS_REST=0` (Ctrl+C in a `&&` series only kills the current command).

## Everything the shell can do

### Running commands
- Run any program found in `$PATH` (`ls`, `pwd`, `grep`, `sleep`, ...) with arguments
- Run programs by path (`./a.out`, `/bin/ls`)
- Prompt shows the current directory: `/home/me $ `
- Empty lines are ignored
- `command not found` message for unknown commands; the shell keeps running
- After a failing command prints `EXITSTATUS: <n>`; after a killed one `TERMINATED BY SIGNAL: <n>`
- `$?` holds the last exit status (128+signal if killed)
- Up to 1024 characters per line, 256 words per line

### Built-in commands
- `cd [dir]` — change directory; no argument or `~` goes to `$HOME`
- `exit` — leave the shell (kills background jobs first)
- `echo [-n] words...` — print words
- `history` — list the saved command history
- `alias`, `alias name`, `alias name='text'` — list / show / define aliases
- `unalias name...` — remove aliases
- `open FILE` — open a file with the program set for its extension

### Quoting, variables, `~`
- `'single quotes'` keep everything literally
- `"double quotes"` keep spaces but still expand variables
- `\` escapes the next character (`echo a\ b`)
- `$NAME`, `${NAME}` — environment variables; `$?` last status; `$$` shell pid
- `~` and `~/path` expand to `$HOME`
- Unterminated quote → `QUOTE MISMATCH`

### Pipes and redirection
- `a | b | c` — up to 32 commands in a pipeline; the status shown is the last command's
- `cmd > file` — write (create / truncate)
- `cmd >> file` — append
- `cmd < file` — read from a file
- Redirections combine with pipes (`cat < in | sort > out`) and work on built-ins (`history > h.txt`)
- Syntax errors are reported (`ls |`, `echo >`) without running anything

### Background execution
- `cmd &` — run in the background; the prompt returns at once
- Background jobs run in their own process group, so Ctrl+C never hits them
- Finished jobs are reaped (no zombies) and reported once: `Shell: Background process finished`
- Up to 64 background jobs; all are killed when the shell exits

### Several commands on one line
- `a && b && c` — serial: one after another, each finished before the next, whatever its result
- `a &&& b &&& c` — parallel: all at once, the prompt returns when every one has finished
- Pipelines work inside both (`a | b && c | d`); up to 64 commands per line
- Mixing `&&` and `&&&` on one line is refused

### Signals
- Ctrl+C stops the foreground command only; the shell and background jobs survive
- Ctrl+C at the prompt abandons the typed line
- Ctrl+C stops every command of a `&&&` group and cancels the rest of a `&&` series
- Ctrl+\ kills the foreground command (SIGQUIT) but not the shell
- Ctrl+Z is ignored (there is no job control)
- Ctrl+D on an empty line exits the shell

### Line editing (when typing in a terminal)
- Backspace deletes; Ctrl+U clears the line
- **Tab completion**
  - first word of a command (also after `|`, `&&`, `&&&`): built-ins and every executable in `$PATH`
  - other words: files and directories of the current directory (directories end in `/`)
  - one match completes it; several matches complete the common start, a second Tab lists them
  - hidden files only when the word starts with `.`
- **↑ / ↓** walk through the history; the line you were typing is kept
- **Ctrl+R** reverse search: type to search, Ctrl+R for the next older match, Enter runs it, Esc / Ctrl+G cancels

### History
- Saved in `~/.shell_history` after every command, loaded at start-up
- Last 100 commands kept; blank lines and immediate repeats are skipped

### Aliases and settings file `~/.shellrc`
- Aliases work at the start of any command (also after `|`, `&&`, `&&&`) and can use other aliases
- `~/.shellrc` is read at start-up; `#` starts a comment
  ```
  alias ll='ls -l'
  opener pdf evince
  opener txt cat
  ```
- `open FILE` runs the program chosen by the extension (case-insensitive), `xdg-open` if none
- The file is only read, never rewritten

## Known limitations
- Operators (`&&`, `&&&`, `|`, `<`, `>`, `>>`, `&`) need spaces around them, and are recognised even inside quotes
- `cd`, `exit`, `alias`, `unalias` are refused in pipelines, `&&&` groups and the background (they must change the shell itself)
- `&` works only on a single command (not with `&&`, `&&&` or `|`)
- No globbing (`*.c`), no `2>` / `2>&1`, no `;`, `||`, `if`/`for`, no job control (`fg`, `bg`, `jobs`)
- Variables are expanded when the line is read; no cursor movement inside a line (left/right arrows are ignored)
- Tab completes names in the current directory only (no `dir/pre<Tab>`)

## Source layout

| File | Job |
|---|---|
| `shell.c` | main loop |
| `lineedit.c` | terminal line editor (Tab, ↑/↓, Ctrl+R) |
| `complete.c`, `trie.c` | Tab candidates; prefix tree |
| `history.c` | saved history |
| `tokenizer.c`, `vars.c` | words, quotes, `$vars`, `~`, `$?` |
| `alias.c`, `rc.c`, `expand.c` | aliases, `~/.shellrc`, alias and `open` rewriting |
| `parser.c` | split at `&&`, `&&&`, `\|`, `<`, `>`, `>>` |
| `sequence.c` | single / serial / parallel running |
| `executor.c` | fork, exec, pipes, redirections, waiting |
| `builtins.c` | built-in commands |
| `jobs.c`, `signals.c` | background table; SIGINT relay |
| `prompt.c`, `config.h` | prompt; limits and switches |
| `test.sh`, `test_sigint.py`, `test_lineedit.py` | tests |
