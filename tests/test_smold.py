# SPDX-License-Identifier: GPL-3.0-or-later
"""smold's minions and the shutdown names. Only that those refuse arguments, taking the machine
down is not something to test on the machine the rest of the tests are using."""

from base import EX_USAGE, SmolTest, Tool
from tools import Cat, Sleep


class Reboot(Tool):
    name = "reboot"


class Poweroff(Tool):
    name = "poweroff"


class Halt(Tool):
    name = "halt"


class TestMinions(SmolTest):
    def minion(self, option):
        """Skip unless the boot had this minion."""
        if f"smolinit.minion={option}" not in Cat().ok("/proc/cmdline").split():
            self.skipTest(f"this boot has no {option} minion")

    def test_one_that_exits_at_once_is_left_alone(self):
        # uname exits at once, it must not be respawned in a loop
        self.minion("/bin/uname")
        before = int(Cat().ok("/proc/loadavg").split()[-1])   # the last pid handed out
        Sleep().ok("1")
        after = int(Cat().ok("/proc/loadavg").split()[-1])
        self.assertLess(after - before, 10)


class TestShutdown(SmolTest):
    def test_arguments(self):
        Reboot().rejects("now", status=EX_USAGE)
        Poweroff().rejects("-f", status=EX_USAGE)
        Halt().rejects("extra", status=EX_USAGE)
