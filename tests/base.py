# SPDX-License-Identifier: GPL-3.0-or-later
"""
Base for the tests. One machine is booted for the whole run with a user shell
on the normal tty and a root shell on the secure tty.

run.py tells us about the machine with SMOL_QEMU, SMOL_KERNEL, SMOL_INITRAMFS
or SMOL_ROOTFS, SMOL_APPEND (kernel command line), SMOL_INIT (init options,
after the --) and SMOL_NIC (network card model).
"""

import atexit
import os
import shutil
import tempfile
import unittest

from qemu import Qemu
from smolutils import Smolutils, kernel_args

# Same as inc/sysexits.h
EX_USAGE = 64
EX_DATAERR = 65
EX_NOINPUT = 66
EX_UNAVAILABLE = 69
EX_SOFTWARE = 70
EX_OSERR = 71
EX_CANTCREAT = 73
EX_IOERR = 74
EX_NOPERM = 77

machine = None
smol = None
user = None     # a shell on the normal tty
root = None     # a root shell on the secure tty
failed = None   # the boot error, don't try again for every class


def boot():
    global machine, smol, user, root, failed
    if machine is not None:
        return
    if failed is not None:
        raise failed

    nic = os.environ.get("SMOL_NIC")
    tftp_dir = None
    if nic:
        tftp_dir = tempfile.mkdtemp(prefix="smolutils-tftp-")   # served over tftp
        atexit.register(shutil.rmtree, tftp_dir)

    m = Qemu(os.environ["SMOL_QEMU"], os.environ["SMOL_KERNEL"],
             initramfs=os.environ.get("SMOL_INITRAMFS"),
             rootfs=os.environ.get("SMOL_ROOTFS"),
             nic=nic, tftp_dir=tftp_dir)
    init = os.environ.get("SMOL_INIT", "")
    if nic:
        init += f" smolinit.telnetd={m.network.telnet_port}"
    m.append = kernel_args(m.securetty.device, m.tty.device, os.environ.get("SMOL_APPEND", ""), init.strip())
    try:
        m.boot()
        s = Smolutils(m)
        u = s.login(m.tty)
        r = s.su(s.login(m.securetty))
    except Exception as e:
        said = "".join(f"\n--- {c.name} said:\n{c.output.decode(errors='replace')}" for c in m.consoles.values())
        failed = RuntimeError(f"{e}{said}\n--- qemu said:\n{m.output()}")
        m.shutdown()
        raise failed from e

    machine, smol, user, root = m, s, u, r
    atexit.register(machine.shutdown)


class ToolFailed(AssertionError):
    pass


class Tool:
    """A command on the machine."""

    name = None

    def __init__(self, shell=None):
        self.shell = shell   # None: the user shell

    def run(self, *args):
        shell = self.shell or user
        if shell is None:
            raise RuntimeError("the machine isn't booted yet")
        return smol.run(shell, " ".join([self.name, *args]))

    def ok(self, *args):
        """Run, must succeed, returns the output."""
        result = self.run(*args)
        if result.status != 0:
            raise ToolFailed(f"{self.name} {' '.join(args)} failed: {result}")
        return result.output

    def fail(self, *args):
        """Run, must fail, returns the Result."""
        result = self.run(*args)
        if result.status == 0:
            raise ToolFailed(f"{self.name} {' '.join(args)} should have failed: {result}")
        return result

    def rejects(self, *args, status):
        """Must fail with this exit status. Don't check messages, they change."""
        result = self.fail(*args)
        if result.status != status:
            raise ToolFailed(f"{self.name} {' '.join(args)} exited {result.status} not {status}: {result}")
        return result


class SmolTest(unittest.TestCase):
    """A test on the machine, self.user and self.root are the shells."""

    @classmethod
    def setUpClass(cls):
        boot()
        cls.user = user
        cls.root = root

    def wait_prompt(self, console):
        """Wait for the prompt after sending something by hand."""
        return smol.wait_prompt(console)

    def tmp(self, name):
        """A /tmp path for this test, cleaned up after."""
        path = f"/tmp/{type(self).__name__}_{self._testMethodName}_{name}"
        # one command per line, ; makes the shell print a prompt in between
        self.addCleanup(smol.run, root, f"rmdir {path}")
        self.addCleanup(smol.run, root, f"rm {path}")
        return path

    def network(self):
        """The network, or skip the test."""
        if machine.network is None:
            self.skipTest("no network on this machine")
        return machine.network
