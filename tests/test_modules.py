# SPDX-License-Identifier: GPL-3.0-or-later
from base import EX_NOINPUT, EX_USAGE, SmolTest, Tool
from tools import Ls


class Insmod(Tool):
    name = "insmod"


class Rmmod(Tool):
    name = "rmmod"


class TestModules(SmolTest):
    # both live in /sbin, which only root gets to
    def setUp(self):
        if not Ls().exists("/proc/modules"):
            self.skipTest("this kernel has no modules")

    def test_insmod(self):
        Insmod(self.root).rejects(status=EX_USAGE)
        Insmod(self.root).rejects("/nonexistent.ko", status=EX_NOINPUT)

    def test_rmmod(self):
        Rmmod(self.root).rejects(status=EX_USAGE)
        Rmmod(self.root).rejects("-z", "mod", status=EX_USAGE)
        Rmmod(self.root).rejects("nosuchmodule", status=EX_NOINPUT)
        Rmmod(self.root).rejects("-f", "no-such-module", status=EX_NOINPUT)
