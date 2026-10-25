#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""
The smolutils side of a machine: init's command line, logging in on a tty
and running commands in smolsh. Works on anything with console, securetty
and tty Consoles, qemu.Qemu for one.
"""

import argparse
import re
import sys
import time
from dataclasses import dataclass

from qemu import Qemu

PROMPT = re.compile(r"smolsh .*? > $")
EXIT_STATUS = re.compile(r"Exited with non-zero return code: (\d+)$")
KILLED = re.compile(r"Killed by signal: (\d+)$")


class LoginFailed(Exception):
    pass


def kernel_args(securetty, tty, extra="", init=""):
    """Kernel command line: extra, then -- with a getty on securetty and on tty, then init."""
    smol = f"smolinit.getty={securetty} smolinit.securetty={securetty} smolinit.getty={tty}"
    return " ".join(part for part in (extra, "--", smol, init) if part)


@dataclass
class Result:
    output: str    # what the command printed, without the echo and prompt
    status: int    # exit status, 128 + the signal if it was killed, 0 when smolsh didn't complain


class Smolutils:
    def __init__(self, machine):
        self.machine = machine
        self.seen = 0   # how far into the secure tty's output we have looked for codes

    def wait_prompt(self, console, timeout=60):
        """Wait for a prompt and return what came before it."""
        deadline = time.monotonic() + timeout
        out = console.read_until(" > ", timeout)
        while not PROMPT.search(out):
            out += console.read_until(" > ", deadline - time.monotonic())
        return out

    def answer_code(self, console, who, timeout=60):
        """
        Type the code getty or su asked for, the secure tty announced it as
        "<who> joined the party, code xxxxxx". Only looks at announcements
        we haven't used yet so a stale one can't be picked up.
        """
        securetty = self.machine.securetty
        deadline = time.monotonic() + timeout
        start = securetty.find(f"{who} joined the party, code ", self.seen, timeout)
        end = securetty.find("\r\n", start, deadline - time.monotonic())
        self.seen = end
        console.send(securetty.output[start:end - 2].decode())

    def _answer_and_wait(self, console, who, timeout):
        """Answer the code, then either the prompt turns up or getty/su say no."""
        deadline = time.monotonic() + timeout
        self.answer_code(console, who, timeout)
        out = console.read_until((" > ", "no\r\n"), deadline - time.monotonic())
        if out.endswith("no\r\n"):
            raise LoginFailed(f"{console.name}: {who} didn't take the code")
        while not PROMPT.search(out):
            out += console.read_until(" > ", deadline - time.monotonic())

    def login(self, console, timeout=60):
        """Get a shell: the secure tty just has one, a normal tty wants a code."""
        if console is self.machine.securetty:
            self.wait_prompt(console, timeout)
        else:
            deadline = time.monotonic() + timeout
            console.read_until("code: ", timeout)
            self._answer_and_wait(console, console.device, deadline - time.monotonic())
        return console

    def su(self, console, timeout=60):
        """Become root, answering the code unless this is the secure tty."""
        console.send("su")
        if console is self.machine.securetty:
            out = self.wait_prompt(console, timeout)
            if "You shall not pass" in out:
                raise LoginFailed(f"{console.name}: su refused")
        else:
            deadline = time.monotonic() + timeout
            console.read_until("code: ", timeout)
            self._answer_and_wait(console, "su", deadline - time.monotonic())
        return console

    def run(self, console, command, timeout=60):
        """Run a command, return its output and exit status."""
        console.send(command)
        out = PROMPT.sub("", self.wait_prompt(console, timeout))   # the prompt may share the last line
        _, echo, out = out.partition(command + "\r\n")             # drop the echo and anything before it
        if not echo:
            raise RuntimeError(f"{console.name}: didn't see {command!r} echoed")

        lines = out.split("\r\n")
        if lines[-1] == "":
            lines.pop()

        status = 0
        if lines and (m := EXIT_STATUS.match(lines[-1])):
            status = int(m.group(1))
            lines.pop()
        elif lines and (m := KILLED.match(lines[-1])):
            status = 128 + int(m.group(1))
            lines.pop()

        return Result("\n".join(lines), status)


def main():
    parser = argparse.ArgumentParser(description="boot smolutils under qemu, log in and run uname")
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--kernel", required=True)
    parser.add_argument("--initramfs")
    parser.add_argument("--rootfs")
    args = parser.parse_args()

    q = Qemu(args.qemu, args.kernel, initramfs=args.initramfs, rootfs=args.rootfs)
    q.append = kernel_args(q.securetty.device, q.tty.device)
    smol = Smolutils(q)
    try:
        q.boot()
        for console in (q.securetty, q.tty):
            smol.login(console)
            print(f"{console.name}: {smol.run(console, 'uname').output}")
        return 0
    finally:
        q.shutdown()


if __name__ == "__main__":
    sys.exit(main())
