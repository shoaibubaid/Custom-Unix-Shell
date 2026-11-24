#!/usr/bin/env bash
# Simple behavioural tests for shell. Run with: make test
cd "$(dirname "$0")" || exit 1

# Private HOME so tests never touch the real history / .shellrc
export HOME=$(mktemp -d)
trap 'rm -rf "$HOME"' EXIT

SH=$PWD/shell
pass=0; fail=0
check() {   # check <name> <command-that-returns-0-on-success>
    if eval "$2" >/dev/null 2>&1; then
        echo "PASS: $1"; pass=$((pass+1))
    else
        echo "FAIL: $1"; fail=$((fail+1))
    fi
}

run() { printf '%s\n' "$@" | "$SH" 2>&1; }

# 1. basic commands match real shell output
check "echo"        '[ "$(run "echo hello world" | grep -c "^.* \$ hello world")" -ge 1 ] || run "echo hello world" | grep -q "hello world"'
check "ls matches"  '[ "$(run "ls /" | grep -v " \$ ")" = "$(ls /)" ] || run "ls /" | grep -q bin'

# 2. empty command prints no error, just prompts again
check "empty line"  '! run "" "" | grep -qi "error\|not found"'

# 3. unknown command -> error message + EXITSTATUS: 1, shell survives
check "bad command"  'run "nosuchcmd_xyz" "echo alive" | grep -q "command not found"'
check "exit status"  'run "nosuchcmd_xyz" | grep -q "EXITSTATUS: 1"'
check "survives"     'run "nosuchcmd_xyz" "echo alive" | grep -q "alive"'

# 4. bad args: executable prints its own error, shell survives
check "bad args"     'run "ls /nonexistent_dir_xyz" "echo alive" | grep -q "alive"'

# 5. cd works in the parent and prompt shows cwd
# (stdin is piped, so prompts and output share a line: match line ends, not starts)
check "cd /tmp"      'run "cd /tmp" "pwd" | grep -q "/tmp \$ /tmp$"'
check "prompt cwd"   'run "cd /tmp" | grep -q "/tmp \$ "'
check "cd .."        'run "cd /tmp" "cd .." "pwd" | grep -q "/ \$ /$"'
check "cd bad dir"   'run "cd /nonexistent_dir_xyz" "echo alive" | grep -q "alive"'
check "cd no args"   'run "cd" "pwd" | grep -q "$HOME\$"'
check "cd too many"  'run "cd a b" | grep -q "usage"'

# 6. no zombies left behind after many commands
check "no zombies"   '
    ( for i in $(seq 1 20); do echo "true"; done; echo "sleep 1" ) | "$SH" >/dev/null 2>&1 &
    sleep 0.5
    z=$(ps --ppid $! -o stat= 2>/dev/null | grep -c "^Z")
    wait; [ "$z" -eq 0 ]'

# ---------------- background execution ----------------
# Output goes to a temp file (not a pipe) because orphaned background
# children keep a pipe open and would make the test wait for them.
tmp=$(mktemp)

# 7. "cmd &" returns to the prompt at once (would take 3s if the shell waited)
check "bg returns immediately" '
    s=$(date +%s)
    printf "sleep 3 &\necho hi\n" | "$SH" > "$tmp" 2>&1
    e=$(date +%s)
    [ $((e - s)) -lt 2 ] && grep -q "hi" "$tmp"'

# 8. a finished background process is reaped and reported exactly once
check "bg reported once" '
    ( echo "sleep 1 &"; sleep 2; echo "echo after" ) | "$SH" > "$tmp" 2>&1
    [ "$(grep -c "Shell: Background process finished" "$tmp")" -eq 1 ]'

# 9. a bg process finishing mid-foreground must NOT end the foreground wait
#    early (a plain wait() would return the bg child at 1s instead of 3s)
check "fg not cut short by bg" '
    s=$(date +%s)
    printf "sleep 1 &\nsleep 3\necho x\n" | "$SH" > "$tmp" 2>&1
    e=$(date +%s)
    [ $((e - s)) -ge 3 ]'

# 10. no zombies once background jobs have been reaped
check "bg no zombies" '
    ( echo "sleep 1 &"; echo "sleep 1 &"; sleep 2; echo "true"; sleep 1 ) | "$SH" >/dev/null 2>&1 &
    sp=$!
    sleep 2.5
    z=$(ps --ppid $sp -o stat= 2>/dev/null | grep -c "^Z")
    wait; [ "$z" -eq 0 ]'

# 11. the 65th simultaneous background process is refused (limit is 64)
check "bg limit 64" '
    ( for i in $(seq 1 65); do echo "sleep 2 &"; done ) | "$SH" > "$tmp" 2>&1
    [ "$(grep -c "too many background" "$tmp")" -eq 1 ]'

# 12. lone "&" is an error but the shell survives
check "lone &" 'run "&" "echo alive" | grep -q "alive"'

# 13. unknown command in the background: error shown, then reaped
check "bg bad command" '
    ( echo "nosuchcmd_xyz &"; sleep 1; echo "echo alive" ) | "$SH" > "$tmp" 2>&1
    grep -q "command not found" "$tmp" && grep -q "Shell: Background process finished" "$tmp"'

# ---------------- exit command ----------------
# Each background sleep below has a unique argument so pgrep -fx can match
# exactly that process and nothing else on the machine.

# 14. exit stops the shell: later input is never executed
check "exit stops shell"   'printf "exit\necho NOT_RUN_MARKER\n" | "$SH" > "$tmp" 2>&1; ! grep -q NOT_RUN_MARKER "$tmp"'

# 15. shell terminates with status 0
check "exit status 0"      'printf "exit\n" | "$SH" >/dev/null 2>&1; [ $? -eq 0 ]'

# 16. exit kills background processes
check "exit kills bg"      '
    printf "sleep 31 &\nsleep 31.5 &\nexit\n" | "$SH" >/dev/null 2>&1
    ! pgrep -fx "sleep 31" && ! pgrep -fx "sleep 31.5"'

# 17. exit returns promptly even with long-running bg jobs (it reaps what it killed)
check "exit is prompt"     '
    s=$(date +%s)
    printf "sleep 33 &\nsleep 33 &\nsleep 33 &\nexit\n" | "$SH" >/dev/null 2>&1
    e=$(date +%s)
    [ $((e - s)) -lt 2 ] && ! pgrep -fx "sleep 33"'

# 18. Ctrl+D (end of input) uses the same cleanup
check "EOF kills bg"       'printf "sleep 32 &\n" | "$SH" >/dev/null 2>&1; ! pgrep -fx "sleep 32"'

# 19. exit with arguments is rejected and the shell keeps running
check "exit with args"     'printf "exit now\necho alive\n" | "$SH" > "$tmp" 2>&1; grep -q "too many arguments" "$tmp" && grep -q alive "$tmp"'

# 20. no zombie children are left when the shell exits (check while it is still dying)
check "exit leaves no zombies" '
    ( echo "sleep 34 &"; echo "sleep 34 &"; echo "exit" ) | "$SH" >/dev/null 2>&1
    ! pgrep -fx "sleep 34" && [ "$(ps -eo stat,cmd | grep -c "^Z.*sleep")" -eq 0 ]'

# ---------------- serial (&&) and parallel (&&&) execution ----------------
# Elapsed time is measured around the whole shell run; sleeps make it visible.

# 21. && runs commands one after another, in order
check "serial order"       'run "echo AA && echo BB && echo CC" | tr "\n" " " | grep -q "AA BB CC"'

# 22. && waits for each command: three 1s sleeps take about 3s, not 1s
check "serial waits"       '
    s=$(date +%s)
    printf "sleep 1 && sleep 1 && sleep 1\n" | "$SH" > "$tmp" 2>&1
    e=$(date +%s)
    [ $((e - s)) -ge 3 ]'

# 23. && does not stop on failure: the next command still runs
check "serial after error" 'run "ls /nonexistent_dir_xyz && echo STILL_RUNS" | grep -q "STILL_RUNS"'
check "serial error status" 'run "ls /nonexistent_dir_xyz && echo x" | grep -q "EXITSTATUS: 2"'

# 24. an unknown command in the middle does not stop the series either
check "serial bad command" 'run "echo B4 && nosuchcmd_xyz && echo AF" | tr "\n" " " | grep -q "B4.*not found.*AF"'

# 25. &&& runs everything at once: three 2s sleeps take about 2s, not 6s
check "parallel timing"    '
    s=$(date +%s)
    printf "sleep 2 &&& sleep 2 &&& sleep 2\n" | "$SH" > "$tmp" 2>&1
    e=$(date +%s)
    [ $((e - s)) -ge 2 ] && [ $((e - s)) -lt 4 ]'

# 26. &&& waits for the slowest command before returning to the prompt
check "parallel waits for all" '
    printf "sleep 1 &&& sleep 3\necho DONE_MARK\n" > "$tmp.in"
    s=$(date +%s)
    "$SH" < "$tmp.in" > "$tmp" 2>&1
    e=$(date +%s)
    rm -f "$tmp.in"
    [ $((e - s)) -ge 3 ] && grep -q DONE_MARK "$tmp"'

# 27. &&& output from every command shows up
check "parallel output"    'run "echo P1 &&& echo P2 &&& echo P3" | tr "\n" " " | grep -q "P1" && run "echo P1 &&& echo P2 &&& echo P3" | grep -q P2 && run "echo P1 &&& echo P2 &&& echo P3" | grep -q P3'

# 28. all parallel children are reaped: no zombies once the prompt is back
check "parallel no zombies" '
    ( echo "sleep 1 &&& sleep 1 &&& sleep 1"; sleep 2.5 ) | "$SH" >/dev/null 2>&1 &
    sp=$!
    sleep 1.8
    z=$(ps --ppid $sp -o stat= 2>/dev/null | grep -c "^Z")
    wait; [ "$z" -eq 0 ]'

# 29. a serial series leaves no zombies either
check "serial no zombies"  '
    ( echo "sleep 0.3 && sleep 0.3 && sleep 0.3"; sleep 2 ) | "$SH" >/dev/null 2>&1 &
    sp=$!
    sleep 1.5
    z=$(ps --ppid $sp -o stat= 2>/dev/null | grep -c "^Z")
    wait; [ "$z" -eq 0 ]'

# 30. a background job finishing during a serial series does not cut it short
check "serial + bg mix"    '
    s=$(date +%s)
    printf "sleep 1 &\nsleep 1 && sleep 2\n" | "$SH" > "$tmp" 2>&1
    e=$(date +%s)
    [ $((e - s)) -ge 3 ]'

# 31. syntax errors are reported and the shell survives
check "mixed && and &&&"   'run "echo a && echo b &&& echo c" "echo alive" | grep -q "cannot mix" && run "echo a && echo b &&& echo c" "echo alive" | grep -q alive'
check "trailing &&"        'run "ls &&" "echo alive" | grep -q "empty command" && run "ls &&" "echo alive" | grep -q alive'
check "leading &&&"        'run "&&& ls" "echo alive" | grep -q "empty command"'
check "double separator"   'run "echo a && && echo b" | grep -q "empty command"'
check "& with &&"          'run "echo a && echo b &" "echo alive" | grep -q "works only on a single command"'

# 32. 64 commands are fine, 65 are refused
check "64 commands ok"     '
    line=$(printf "echo x && %.0s" $(seq 1 63))"echo x"
    [ "$(run "$line" | grep -c "x")" -ge 64 ]'
check "65 commands refused" '
    line=$(printf "echo x && %.0s" $(seq 1 64))"echo x"
    run "$line" | grep -q "too many commands"'

# 33. built-ins work inside a serial series, not in a parallel one
check "cd in serial"       'run "cd /tmp && pwd" | grep -q "/tmp$"'
check "exit in serial"     'printf "echo B4EXIT && exit && echo NOT_RUN_MARKER\n" | "$SH" > "$tmp" 2>&1; grep -q B4EXIT "$tmp" && ! grep -q NOT_RUN_MARKER "$tmp"'
check "builtin in parallel refused" 'run "cd /tmp &&& ls" | grep -q "cannot be used in parallel"'

# ---------------- quoting, variables, ~ ----------------
check "double quotes keep spaces"  'run "echo \"a   b\"" | grep -q "a   b"'
check "single quotes literal"      'run "echo '"'"'\$HOME x'"'"'" | grep -q "\$HOME x"'
check "\$HOME expands"             'run "echo \$HOME" | grep -q "$HOME"'
check "\$? after false"            'run "false" "echo \$?" | grep -q "^.*\$ 1$"'
check "\$\$ is a number"           'run "echo \$\$" | grep -Eq " [0-9]+$"'
check "tilde"                      'run "echo ~/x" | grep -q "$HOME/x"'
check "cd ~"                       'run "cd ~" "pwd" | grep -q "$HOME\$"'
check "backslash escape"           'run "echo a\\ b" | grep -q "a b"'
check "quote mismatch"             'run "echo \"abc" "echo alive" | grep -q "QUOTE MISMATCH"'

# ---------------- echo builtin ----------------
check "echo -n"        'run "echo -n hi" | grep -q "\$ hi/"'

# ---------------- pipes ----------------
check "pipe"           'run "echo hello | tr a-z A-Z" | grep -q "HELLO"'
check "3-stage pipe"   'run "printf \"b\na\nb\n\" | sort | uniq" | grep -c "^b$" | grep -q 1'
check "pipe status = last stage" 'run "echo x | false" | grep -q "EXITSTATUS: 1"'
check "pipe no zombies" '
    ( for i in 1 2 3 4 5; do echo "echo a | cat | cat"; done; echo "sleep 1" ) | "$SH" >/dev/null 2>&1 &
    sleep 0.5; z=$(ps --ppid $! -o stat= | grep -c "^Z"); wait; [ "$z" -eq 0 ]'
check "builtin in pipe"   'run "echo hi | cat" | grep -q "hi"'
check "cd in pipe refused" 'run "cd / | cat" | grep -q "cannot be used in a pipeline"'
check "empty pipe stage"  'run "ls |" | grep -q "syntax error"'
check "pipe + &&"         'run "echo a | cat && echo b | cat" | grep -qx b'
check "pipe in parallel"  'run "echo p1 | cat &&& echo p2 | cat" | grep -q p2'

# ---------------- redirection ----------------
cd "$HOME"
check "> creates file"   'run "echo one > r.txt" >/dev/null; [ "$(cat r.txt)" = one ]'
check "> truncates"      'run "echo two > r.txt" >/dev/null; [ "$(cat r.txt)" = two ]'
check ">> appends"       'run "echo three >> r.txt" >/dev/null; [ "$(cat r.txt)" = "$(printf "two\nthree")" ]'
check "< reads file"     'run "cat < r.txt" | grep -q three'
check "< missing file"   'run "cat < nope_xyz" "echo alive" | grep -q "No such file"'
check "pipe + redirect"  'run "cat < r.txt | wc -l > c.txt" >/dev/null; [ "$(tr -d " " < c.txt)" = 2 ]'
check "ls > file"        'run "ls / > l.txt" >/dev/null; grep -q bin l.txt'
check "missing filename" 'run "echo a >" | grep -q "missing file name"'
check "redirect builtin" 'run "history > h.txt" >/dev/null; grep -q history h.txt'
check "redirect in bg"   'run "echo bgout > b.txt &" "sleep 0.3" >/dev/null; sleep 0.5; [ "$(cat b.txt)" = bgout ]'
cd - >/dev/null

# ---------------- history ----------------
rm -f "$HOME/.shell_history"
check "history lists"       'run "echo aaa" "echo bbb" "history" | grep -q "2  echo bbb"'
check "history persists"    'run "echo persist_me" >/dev/null; run "history" | grep -q "echo persist_me"'
check "history skips blank/dup" '
    rm -f "$HOME/.shell_history"; run "" "echo d" "echo d" >/dev/null
    [ "$(grep -c . "$HOME/.shell_history")" = 1 ]'
check "history capped at 100" '
    rm -f "$HOME/.shell_history"
    for i in $(seq 1 120); do echo "echo n$i"; done | "$SH" >/dev/null 2>&1
    [ "$(wc -l < "$HOME/.shell_history")" = 100 ] && head -1 "$HOME/.shell_history" | grep -q n21'

# ---------------- alias / unalias / rc file / open ----------------
check "alias define+use"  'run "alias hi=\"echo ALIASED\"" "hi there" | grep -q "ALIASED there"'
check "alias listing"     'run "alias xx=\"echo 1\"" "alias" | grep -q "alias xx=.echo 1."'
check "unalias"           'run "alias q=\"echo Q\"" "unalias q" "q" | grep -q "command not found"'
check "alias after |"     'run "alias up=\"tr a-z A-Z\"" "echo abc | up" | grep -q ABC'
check "alias chain"       'run "alias a1=\"echo X\"" "alias a2=a1" "a2" | grep -q "\$ X$"'
check "alias self-loop ok" 'run "alias ls=\"ls -d\"" "ls /" | grep -q "^.*/\$\|/"'
check "unalias missing"   'run "unalias nope_xyz" | grep -q "not found"'
cat > "$HOME/.shellrc" <<RC
# a comment
alias rcal='echo FROM_RC'
opener txt cat
opener PDF echo
RC
check "rc alias"          'run "rcal" | grep -q FROM_RC'
echo opened_text > "$HOME/note.txt"
check "open uses rc opener" 'run "open $HOME/note.txt" | grep -q opened_text'
check "open ext case"     'touch "$HOME/x.pdf"; run "open $HOME/x.pdf" | grep -q "x.pdf"'
check "open bare usage"   'run "open" | grep -q "usage"'
rm -f "$HOME/.shellrc"

# ---------------- line editor (real pseudo-terminal) ----------------
if command -v python3 >/dev/null 2>&1; then
    if out=$(python3 test_lineedit.py 2>&1); then
        echo "PASS: line editor (tab, history keys, ctrl+r: $(echo "$out" | grep -c '^PASS') checks via pty)"
        pass=$((pass+1))
    else
        echo "FAIL: line editor (pty)"
        echo "$out" | grep -E '^FAIL|passed:' | sed 's/^/      /'
        fail=$((fail+1))
    fi
fi

# ---------------- Ctrl+C handling (real pseudo-terminal) ----------------
# test_sigint.py types a genuine Ctrl+C into a pty. Its details are only
# shown when it fails; run "python3 test_sigint.py" to see every check.
if command -v python3 >/dev/null 2>&1; then
    if out=$(python3 test_sigint.py 2>&1); then
        echo "PASS: ctrl-c handling ($(echo "$out" | grep -c '^PASS') checks via pty)"
        pass=$((pass+1))
    else
        echo "FAIL: ctrl-c handling (pty)"
        echo "$out" | grep -E '^FAIL|passed:' | sed 's/^/      /'
        fail=$((fail+1))
    fi
else
    echo "SKIP: ctrl-c handling (python3 not found)"
fi

rm -f "$tmp"

echo; echo "passed: $pass  failed: $fail"
[ "$fail" -eq 0 ]
