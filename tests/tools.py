# SPDX-License-Identifier: GPL-3.0-or-later
"""Wrappers for smolutils tools so they can be used to write tests"""

import re
import time

from base import Tool, ToolFailed

COLOUR = re.compile(r"\x1b\[[0-9;]*m")


class Echo(Tool):
    name = "echo"

    def to_file(self, path, text):
        """Write text and a newline into path through the shell's redirection."""
        return self.ok(text, ">", path)


class Cat(Tool):
    name = "cat"


class Touch(Tool):
    name = "touch"


class Cp(Tool):
    name = "cp"


class Mv(Tool):
    name = "mv"


class Ln(Tool):
    name = "ln"

    def symlink(self, target, link):
        return self.ok("-s", target, link)


class Rm(Tool):
    name = "rm"


class Mkdir(Tool):
    name = "mkdir"


class Rmdir(Tool):
    name = "rmdir"


class Chmod(Tool):
    name = "chmod"


class Chown(Tool):
    name = "chown"


class Mknod(Tool):
    name = "mknod"


class Entry:
    """One line of ls -l. For a device node the size is the major and minor."""

    def __init__(self, line):
        fields = COLOUR.sub("", line).split()
        self.mode, self.links, self.uid, self.gid = fields[0], int(fields[1]), fields[2], fields[3]
        self.target = None
        if "->" in fields:                       # a symlink: name -> target
            self.target = fields[-1]
            fields = fields[:fields.index("->")]
        self.name = fields[-1]
        self.size = " ".join(fields[4:-1])
        self.device = None
        if "," in self.size:                     # a device node: "1, 3"
            self.device = tuple(int(n) for n in self.size.replace(",", " ").split())


class Ls(Tool):
    name = "ls"

    def names(self, path):
        return COLOUR.sub("", self.ok(path)).split()

    def long(self, path):
        """ls -l as {name: Entry}."""
        return {e.name: e for e in map(Entry, self.ok("-l", path).splitlines())}

    def exists(self, path):
        directory, _, name = path.rpartition("/")
        return name in self.names(directory or "/")

    def entry(self, path):
        """The ls -l Entry for one path."""
        directory, _, name = path.rpartition("/")
        return self.long(directory or "/")[name]


class Sha256sum(Tool):
    name = "sha256sum"

    def digest(self, path):
        digest, name = self.ok(path).split("  ")
        if name != path:
            raise ToolFailed(f"sha256sum named {name!r} not {path!r}")
        return digest


class Xxd(Tool):
    name = "xxd"


IFACE = "eth0"


class Dhcpc(Tool):
    name = "dhcpc"

    def lease(self, iface=IFACE):
        return self.ok("-i", iface)


class Uname(Tool):
    name = "uname"

    def fields(self):
        """(sysname, hostname, release, version, machine, os)"""
        words = self.ok().split()
        return words[0], words[1], words[2], " ".join(words[3:-2]), words[-2], words[-1]


class Ps(Tool):
    name = "ps"

    def table(self):
        """{pid: (user, cmd)}"""
        rows = {}
        for line in self.ok().splitlines()[1:]:
            user, pid, cmd = line.split("\t\t")
            rows[int(pid)] = (user, cmd)
        return rows

    def pid_of(self, cmd):
        return [pid for pid, (_, c) in self.table().items() if c == cmd]

    def wait_for(self, cmd, timeout=5):
        """The pids of cmd once it is running, it takes the shell a moment to exec it."""
        deadline = time.monotonic() + timeout
        while True:
            pids = self.pid_of(cmd)
            if pids or time.monotonic() > deadline:
                return pids


class Kill(Tool):
    name = "kill"
