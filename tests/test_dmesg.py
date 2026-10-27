# SPDX-License-Identifier: GPL-3.0-or-later
from base import EX_USAGE, SmolTest, Tool
from tools import COLOUR


class Dmesg(Tool):
    name = "dmesg"

    def lines(self):
        return COLOUR.sub("", self.ok()).splitlines()


class TestDmesg(SmolTest):
    def test_kernel_log_is_there(self):
        # as root so a /dev/kmsg only root may read doesn't matter
        lines = Dmesg(self.root).lines()
        self.assertTrue(any(line.startswith("Linux version") for line in lines), lines[:5])
        # "Run /init as init process", or /sbin/init from a rootfs
        self.assertTrue(any("as init process" in line for line in lines), lines[-5:])

    def test_takes_no_arguments(self):
        Dmesg().rejects("-c", status=EX_USAGE)
