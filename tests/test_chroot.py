# SPDX-License-Identifier: GPL-3.0-or-later
from base import EX_NOINPUT, EX_USAGE, SmolTest, Tool
from tools import Cp, Mkdir, Rm, Rmdir, Uname


class Chroot(Tool):
    name = "chroot"


class TestChroot(SmolTest):
    def test_runs_a_command_under_a_new_root(self):
        root = self.tmp("root")
        Mkdir(self.root).ok(root)
        self.addCleanup(Rmdir(self.root).run, f"{root}/bin")   # rm has no -r, so one by one, in reverse
        self.addCleanup(Rm(self.root).run, f"{root}/bin/uname")
        Mkdir(self.root).ok(f"{root}/bin")
        Cp(self.root).ok("/bin/uname", f"{root}/bin/uname")   # static, so it needs nothing else
        self.assertEqual(Chroot(self.root).ok(root, "/bin/uname"), Uname().ok())

    def test_default_command_is_a_shell(self):
        root = self.tmp("empty")
        Mkdir(self.root).ok(root)
        Chroot(self.root).rejects(root, status=127)                 # no shell in there

    def test_bad_parameters(self):
        Chroot(self.root).rejects(status=EX_USAGE)
        Chroot(self.root).rejects("/nonexistent", "/bin/uname", status=EX_NOINPUT)
