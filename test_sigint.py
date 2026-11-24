#!/usr/bin/env python3
"""
Ctrl+C behaviour test for ./shell.

The shell is started on a pseudo-terminal, so writing the byte 0x03 (what
Ctrl+C types) makes the terminal send a real SIGINT to the terminal's
foreground process group, exactly like pressing the key.

Checks:
  * Ctrl+C kills only the foreground command; the shell and all
    background processes survive
  * Ctrl+C at an idle prompt does not kill the shell
  * a foreground command that reads the terminal (cat) still works,
    and Ctrl+C stops it
  * a SIGINT sent to the shell alone (kill -INT) is relayed to the
    foreground child
  * a background process's own children also survive Ctrl+C, and all of
    them are killed by "exit"
  * Ctrl+C kills every command of a parallel (&&&) group
  * Ctrl+C in a serial (&&) series kills the current command and cancels
    the rest of the series

Run directly (python3 test_sigint.py) or through "make test".
"""
import os
import pty
import select
import signal
import subprocess
import sys
import tempfile
import termios
import time

HERE = os.path.dirname(os.path.abspath(__file__))
SHELL = os.path.join(HERE, "shell")
CTRL_C = "\x03"
results = []


def check(name, cond):
    results.append(bool(cond))
    print(("PASS: " if cond else "FAIL: ") + name)


def alive(cmdline):
    """True if a process with exactly this command line exists."""
    return subprocess.run(["pgrep", "-fx", cmdline],
                          stdout=subprocess.DEVNULL).returncode == 0


class Terminal:
    """Runs ./shell on a pty and lets us type into it and read its output."""

    def __init__(self):
        self.pid, self.fd = pty.fork()
        if self.pid == 0:                       # child: become the shell
            try:
                os.execv(SHELL, [SHELL])
            finally:
                os._exit(127)
        attrs = termios.tcgetattr(self.fd)      # no echo: output is only
        attrs[3] &= ~termios.ECHO               # what the programs print
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        self.buf = ""

    def send(self, text):
        os.write(self.fd, text.encode())

    def wait_for(self, text, timeout=5):
        """Read until `text` shows up; consume the output up to it."""
        end = time.time() + timeout
        while text not in self.buf:
            left = end - time.time()
            if left <= 0:
                return False
            ready, _, _ = select.select([self.fd], [], [], left)
            if ready:
                try:
                    data = os.read(self.fd, 4096)
                except OSError:                 # pty closed
                    return False
                if not data:
                    return False
                self.buf += data.decode(errors="replace")
        self.buf = self.buf.split(text, 1)[1]
        return True

    def prompt(self, timeout=5):
        return self.wait_for("$ ", timeout)

    def shell_alive(self):
        try:
            pid, _ = os.waitpid(self.pid, os.WNOHANG)
            return pid == 0
        except ChildProcessError:
            return False

    def wait_exit(self, timeout=5):
        end = time.time() + timeout
        while time.time() < end:
            pid, status = os.waitpid(self.pid, os.WNOHANG)
            if pid != 0:
                return status
            time.sleep(0.05)
        return None


def main():
    if not os.access(SHELL, os.X_OK):
        print("shell binary not found: run make first")
        return 1

    script = tempfile.NamedTemporaryFile("w", suffix=".sh", delete=False)
    script.write("sleep 106 &\nsleep 106\n")     # a bg job with its own child
    script.close()

    t = Terminal()
    try:
        check("shell starts and shows a prompt", t.prompt())

        # --- background jobs + a foreground job, then Ctrl+C -------------
        t.send("sleep 101 &\n"); t.prompt()
        t.send("sleep 102 &\n"); t.prompt()
        t.send("sleep 103\n")
        time.sleep(0.5)
        check("foreground sleep 103 is running", alive("sleep 103"))

        t.send(CTRL_C)
        check("Ctrl+C reports the foreground command was killed by SIGINT",
              t.wait_for("TERMINATED BY SIGNAL: 2"))
        check("shell is back at a prompt after Ctrl+C", t.prompt())
        check("foreground sleep 103 was terminated", not alive("sleep 103"))
        check("background sleep 101 survived Ctrl+C", alive("sleep 101"))
        check("background sleep 102 survived Ctrl+C", alive("sleep 102"))
        check("the shell itself survived Ctrl+C", t.shell_alive())

        t.send("echo STILL_HERE\n")
        check("shell still runs commands after Ctrl+C", t.wait_for("\nSTILL_HERE"))
        t.prompt()

        # --- Ctrl+C at an idle prompt ------------------------------------
        t.send(CTRL_C)
        check("Ctrl+C at an idle prompt gives a fresh prompt", t.prompt())
        t.send("echo AFTER_IDLE\n")
        check("shell works after Ctrl+C at an idle prompt",
              t.wait_for("\nAFTER_IDLE"))
        t.prompt()
        check("background jobs still alive after idle Ctrl+C",
              alive("sleep 101") and alive("sleep 102"))

        # --- a foreground command that reads the terminal ----------------
        t.send("cat\n")
        time.sleep(0.3)
        t.send("hello_cat\n")
        check("foreground cat can read the terminal (not stopped by SIGTTIN)",
              t.wait_for("hello_cat"))
        t.send(CTRL_C)
        check("Ctrl+C stops the foreground cat",
              t.wait_for("TERMINATED BY SIGNAL: 2"))
        t.prompt()

        # --- SIGINT delivered to the shell only: must be relayed ---------
        t.send("sleep 104\n")
        time.sleep(0.5)
        os.kill(t.pid, signal.SIGINT)           # only the shell, not its group
        check("SIGINT sent to the shell is relayed to the foreground child",
              t.wait_for("TERMINATED BY SIGNAL: 2"))
        check("relayed SIGINT really ended sleep 104", not alive("sleep 104"))
        t.prompt()

        # --- background job that has its own child -----------------------
        t.send("sh %s &\n" % script.name); t.prompt()
        time.sleep(0.5)
        check("background job with a child is running", alive("sleep 106"))
        t.send("sleep 105\n")
        time.sleep(0.3)
        t.send(CTRL_C)
        t.wait_for("TERMINATED BY SIGNAL: 2"); t.prompt()
        check("Ctrl+C left the background job's child alone", alive("sleep 106"))

        # --- Ctrl+C during a parallel (&&&) group ------------------------
        t.send("sleep 201 &\n"); t.prompt()
        t.send("sleep 202 &&& sleep 203 &&& sleep 204\n")
        time.sleep(0.5)
        check("parallel: all three commands are running",
              alive("sleep 202") and alive("sleep 203") and alive("sleep 204"))
        t.send(CTRL_C)
        check("parallel: Ctrl+C returns to the prompt", t.prompt())
        check("parallel: Ctrl+C terminated every command",
              not (alive("sleep 202") or alive("sleep 203") or alive("sleep 204")))
        check("parallel: background sleep 201 survived", alive("sleep 201"))
        check("parallel: shell survived", t.shell_alive())

        t.send("sleep 210 &&& sleep 211\n")
        time.sleep(0.5)
        os.kill(t.pid, signal.SIGINT)           # only the shell, not its group
        check("parallel: SIGINT sent to the shell is relayed to all children",
              t.prompt() and not (alive("sleep 210") or alive("sleep 211")))

        # --- Ctrl+C during a serial (&&) series --------------------------
        t.send("sleep 205 && sleep 206 && sleep 207\n")
        time.sleep(0.5)
        check("serial: first command running, later ones not started yet",
              alive("sleep 205") and not alive("sleep 206"))
        t.send(CTRL_C)
        check("serial: Ctrl+C returns to the prompt", t.prompt())
        time.sleep(0.5)                         # long enough for a wrongly-started next command
        check("serial: current command terminated", not alive("sleep 205"))
        check("serial: the rest of the series was cancelled",
              not (alive("sleep 206") or alive("sleep 207")))

        t.send("echo S1 && sleep 208 && sleep 209\n")
        check("serial: first command of the series ran", t.wait_for("S1"))
        time.sleep(0.3)
        t.send(CTRL_C)
        t.prompt()
        time.sleep(0.5)
        check("serial: Ctrl+C in the middle cancels what is left",
              not (alive("sleep 208") or alive("sleep 209")))
        check("serial: background sleep 201 survived", alive("sleep 201"))
        check("serial: shell survived", t.shell_alive())

        t.send("echo AFTER_SERIAL\n")
        check("shell works after Ctrl+C in a series", t.wait_for("\nAFTER_SERIAL"))
        t.prompt()

        # --- exit is what ends background jobs ---------------------------
        t.send("exit\n")
        status = t.wait_exit()
        check("exit ends the shell with status 0",
              status is not None and os.WIFEXITED(status)
              and os.WEXITSTATUS(status) == 0)
        time.sleep(0.2)
        check("exit killed background sleep 101", not alive("sleep 101"))
        check("exit killed background sleep 102", not alive("sleep 102"))
        check("exit killed background sleep 201", not alive("sleep 201"))
        check("exit killed the background job and its child (sleep 106)",
              not alive("sleep 106"))
    finally:
        try:
            os.kill(t.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        for arg in ("101", "102", "103", "104", "105", "106", "201", "202",
                    "203", "204", "205", "206", "207", "208", "209", "210", "211"):
            subprocess.run(["pkill", "-fx", "sleep " + arg],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        os.unlink(script.name)

    print("\nctrl-c tests passed: %d  failed: %d" %
          (sum(results), len(results) - sum(results)))
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
