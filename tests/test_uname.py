# SPDX-License-Identifier: GPL-3.0-or-later
from base import EX_USAGE, SmolTest
from tools import Uname


class TestUname(SmolTest):
    def test_fields(self):
        sysname, hostname, release, version, machine, os = Uname().fields()
        self.assertEqual(sysname, "Linux")
        self.assertEqual(hostname, "smol")
        self.assertRegex(release, r"^\d+\.\d+")
        self.assertRegex(version, r"^#\d+")
        # the kernel says armv7l, sh4a and so on
        self.assertRegex(machine, r"^(x86_64|m68k|armv\d|sh[234])")
        self.assertEqual(os, "nolibc/Linux")

    def test_takes_no_arguments(self):
        Uname().rejects("-a", status=EX_USAGE)
