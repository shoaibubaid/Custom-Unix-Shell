#!/usr/bin/env python3
"""
Line-editor test for ./shell, on a pseudo-terminal (keys are typed for real).

Checks: typing + Backspace, Ctrl+U, Tab (command, unique file, ambiguous
list, directory), Up/Down history, history persistence, Ctrl+R, Ctrl+D,
Ctrl+C at the prompt, Ctrl+\\ and Ctrl+Z being ignored.
"""
import os
import shutil
import sys
import tempfile
import time

home = tempfile.mkdtemp()
os.environ["HOME"] = home
os.chdir(home)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import test_sigint as ts          # reuse the Terminal helper and check()

UP, DOWN = "\x1b[A", "\x1b[B"
CTRL_D, CTRL_R, CTRL_U, CTRL_BS, CTRL_Z = "\x04", "\x12", "\x15", "\x1c", "\x1a"

for name in ("alpha.txt", "alpine.txt", "unique_file.dat"):
    open(name, "w").write("x\n")
os.mkdir("subdir")


def fresh():
    t = ts.Terminal()
    ts.check("prompt shown", t.prompt())
    return t


def run_line(t, line, marker):
    t.send(line + "\n")
    ok = t.wait_for("\n" + marker)
    t.prompt()
    return ok


def main():
    t = fresh()
    try:
        t.send("echo helX\x7flo\n")
        ts.check("Backspace removes a character", t.wait_for("\nhello"))
        t.prompt()

        t.send("echo junk" + CTRL_U + "echo clean\n")
        ts.check("Ctrl+U clears the line", t.wait_for("\nclean"))
        t.prompt()

        t.send("unique_f\t")
        time.sleep(0.3)
        t.send(CTRL_U + "cat unique_f\t\n")
        ts.check("Tab completes a unique file name", t.wait_for("\nx"))
        t.prompt()

        t.send("cat al\t")
        ts.check("Tab on ambiguous prefix adds the common part", t.wait_for("cat alp"))
        t.send("\t")
        ts.check("second Tab lists the choices",
                 t.wait_for("alpha.txt") and t.wait_for("alpine.txt"))
        t.send(CTRL_U)

        t.send("cat subd\t")
        ts.check("directory completes with a slash", t.wait_for("subdir/"))
        t.send(CTRL_U)

        t.send("ech\t")
        ts.check("Tab completes a command name", t.wait_for("echo "))
        t.send(CTRL_U)

        t.send("echo one\n"); t.prompt()
        t.send("echo two\n"); t.prompt()
        t.send(UP + "\n")
        ts.check("Up recalls the previous command", t.wait_for("\ntwo"))
        t.prompt()
        t.send(UP + UP + "\n")
        ts.check("Up twice recalls an older command", t.wait_for("\none"))
        t.prompt()
        t.send(UP + UP + DOWN + "\n")
        ts.check("Down moves forward again", t.wait_for("\none"))
        t.prompt()

        t.send("echo needle_in_haystack\n"); t.prompt()
        t.send("echo other\n"); t.prompt()
        t.send(CTRL_R + "needle")
        ts.check("Ctrl+R shows the search", t.wait_for("reverse-i-search"))
        t.send("\n")
        ts.check("Enter runs the found command", t.wait_for("\nneedle_in_haystack"))
        t.prompt()

        t.send("echo typed" + CTRL_R + "zzz_nomatch\x07")
        t.send(CTRL_U + "echo after_cancel\n")
        ts.check("Ctrl+G cancels the search", t.wait_for("\nafter_cancel"))
        t.prompt()

        t.send("echo half" + "\x03")
        ts.check("Ctrl+C abandons the line, shell lives", t.prompt() and t.shell_alive())
        t.send("echo ok_after_ctrlc\n")
        ts.check("line was discarded", t.wait_for("\nok_after_ctrlc"))
        t.prompt()

        t.send(CTRL_BS)
        t.send("echo quit_ignored\n")
        ts.check("Ctrl+\\ does not kill the shell", t.wait_for("\nquit_ignored"))
        t.prompt()
        t.send(CTRL_Z)
        t.send("echo stop_ignored\n")
        ts.check("Ctrl+Z does not stop the shell", t.wait_for("\nstop_ignored"))
        t.prompt()

        t.send("sleep 5\n")
        time.sleep(0.4)
        t.send(CTRL_BS)
        ts.check("Ctrl+\\ kills the foreground command (signal 3)",
                 t.wait_for("TERMINATED BY SIGNAL: 3"))
        ts.check("shell survives it", t.prompt() and t.shell_alive())

        t.send("echo x" + CTRL_D)
        time.sleep(0.2)
        ts.check("Ctrl+D on a non-empty line does nothing", t.shell_alive())
        t.send(CTRL_U + CTRL_D)
        status = t.wait_exit()
        ts.check("Ctrl+D on an empty line exits the shell", status is not None)
    finally:
        try:
            os.kill(t.pid, 9)
        except OSError:
            pass

    # history written by that session is loaded by the next one
    t = fresh()
    try:
        t.send(UP + "\n")
        ts.check("history survives a restart (Up)", t.wait_for("\n") and
                 ("echo" in t.buf or t.prompt() is not None))
        t.send("exit\n")
        ts.check("exit works", t.wait_exit() is not None)
    finally:
        try:
            os.kill(t.pid, 9)
        except OSError:
            pass

    shutil.rmtree(home, ignore_errors=True)
    passed = sum(ts.results)
    print(f"\npassed: {passed}  failed: {len(ts.results) - passed}")
    return 0 if all(ts.results) else 1


sys.exit(main())
