# SPDX-License-Identifier: GPL-3.0-or-later
from base import EX_NOPERM, EX_USAGE, SmolTest, Tool
from tools import Ls, Uname


class Devmem(Tool):
    name = "devmem"


class TestDevmem(SmolTest):
    def setUp(self):
        if not Ls().exists("/dev/mem"):
            self.skipTest("no /dev/mem on this machine")

    def test_read_the_bios_area(self):
        # the first megabyte is readable even with STRICT_DEVMEM, on a PC
        if Uname().fields()[4] != "x86_64":
            self.skipTest("only a PC has a BIOS area to read")
        lines = Devmem(self.root).ok("-l", "32", "0xf0000").splitlines()
        self.assertEqual(len(lines), 2)
        self.assertEqual([line[:10] for line in lines], ["000f0000: ", "000f0010: "])

    def test_needs_root(self):
        Devmem().rejects("0xf0000", status=EX_NOPERM)

    def test_arguments(self):
        for args in ((), ("xyz",), ("-l", "x", "0"), ("-w", "3", "0"), ("-w", "4", "0x1")):
            with self.subTest(args=args):
                Devmem().rejects(*args, status=EX_USAGE)
        Devmem(self.root).rejects("-w", "1", "-s", "0x100", "0xf0000", status=EX_USAGE)   # 0x100 doesn't fit a byte
