# SPDX-License-Identifier: GPL-3.0-or-later
"""Wrappers for smolutils tools so they can be used to write tests"""

import re
import time
import unittest

from base import Tool, ToolFailed

COLOUR = re.compile(r"\x1b\[[0-9;]*m")


def needs(path, reason):
    """Skip the test unless path exists, for hardware and tools the image may not have."""
    if not Ls().exists(path):
        raise unittest.SkipTest(reason)


def wait_for(check, timeout=5):
    """Poll check() until it returns something true or the time is up, returns the last result."""
    deadline = time.monotonic() + timeout
    while True:
        result = check()
        if result or time.monotonic() > deadline:
            return result
        time.sleep(0.1)


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


class Mount(Tool):
    name = "mount"

    def tmpfs(self, target):
        return self.ok("-t", "tmpfs", "tmp", target)


class Umount(Tool):
    name = "umount"


class Mode:
    """What ls -l shows for a kind of file and its permissions, Mode("d", 0o755) is "drwxr-xr-x"."""

    def __init__(self, kind, perms):
        self.kind, self.perms = kind, perms

    def __str__(self):
        bits = "".join(c if self.perms & (1 << (8 - i)) else "-" for i, c in enumerate("rwxrwxrwx"))
        return self.kind + bits

    def __eq__(self, other):
        return str(self) == str(other)


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
        """False when the directory it would be in isn't there either."""
        directory, _, name = path.rpartition("/")
        result = self.run(directory or "/")
        return result.status == 0 and name in COLOUR.sub("", result.output).split()

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
        return wait_for(lambda: self.pid_of(cmd), timeout)

    def kill_all(self, cmd):
        """kill -9 every cmd, to clean up what a test left running."""
        for pid in self.pid_of(cmd):
            Kill(self.shell).run("-9", str(pid))


class Kill(Tool):
    name = "kill"


class Sleep(Tool):
    name = "sleep"
