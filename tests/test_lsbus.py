# SPDX-License-Identifier: GPL-3.0-or-later
from base import EX_USAGE, SmolTest, Tool
from tools import Ls


class Lspci(Tool):
    name = "lspci"


class Lsusb(Tool):
    name = "lsusb"


class TestLsbus(SmolTest):
    def test_lspci(self):
        if not Ls().exists("/sys/bus/pci"):
            self.skipTest("no PCI on this machine")
        lines = Lspci().ok().splitlines()
        self.assertEqual(len(lines), len(Ls().names("/sys/bus/pci/devices")))
        for line in lines:
            self.assertRegex(line, r"^\S+ \[[0-9a-f]{4}\]: [0-9a-f]{4}:[0-9a-f]{4}$")

    def test_lsusb(self):
        if not Ls().exists("/sys/bus/usb"):
            self.skipTest("no USB on this machine")
        lines = Lsusb().ok().splitlines()
        self.assertTrue(lines, "a USB bus with nothing on it, not even a root hub?")
        for line in lines:
            self.assertRegex(line, r"^Bus \d{3} Device \d{3}: ID [0-9a-f]{4}:[0-9a-f]{4}")

    def test_arguments(self):
        Lspci().rejects("-v", status=EX_USAGE)
        Lsusb().rejects("extra", status=EX_USAGE)
