# SPDX-License-Identifier: GPL-3.0-or-later
"""The shutdown names from smold.c. Only that they refuse arguments, taking the machine down is
not something to test on the machine the rest of the tests are using."""

from base import EX_USAGE, SmolTest, Tool


class Reboot(Tool):
    name = "reboot"


class Poweroff(Tool):
    name = "poweroff"


class Halt(Tool):
    name = "halt"


class TestShutdown(SmolTest):
    def test_arguments(self):
        Reboot().rejects("now", status=EX_USAGE)
        Poweroff().rejects("-f", status=EX_USAGE)
        Halt().rejects("extra", status=EX_USAGE)
