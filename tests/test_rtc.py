# SPDX-License-Identifier: GPL-3.0-or-later
import re
import time

from base import EX_NOPERM, EX_USAGE, SmolTest, Tool
from tools import Ls

DATE = re.compile(r"^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$")


class Hwclock(Tool):
    name = "hwclock"


class Rtcwake(Tool):
    name = "rtcwake"


class TestRtc(SmolTest):
    def setUp(self):
        if not Ls().exists("/dev/rtc0"):
            self.skipTest("no RTC on this machine")

    def test_read_clock(self):
        self.assertRegex(Hwclock(self.root).ok("-r"), DATE)
        self.assertRegex(Hwclock(self.root).ok(), DATE)

    def test_system_to_hardware_and_back(self):
        before = Hwclock(self.root).ok("-r")
        Hwclock(self.root).ok("-w")
        Hwclock(self.root).ok("-s")
        after = Hwclock(self.root).ok("-r")
        self.assertLessEqual(abs(self.seconds(after) - self.seconds(before)), 5)

    def seconds(self, date):
        """A YYYY-MM-DD HH:MM:SS string as seconds since the epoch."""
        return time.mktime(time.strptime(date, "%Y-%m-%d %H:%M:%S"))

    def test_alarm(self):
        if not Ls().exists("/sys/class/rtc/rtc0/wakealarm"):
            self.skipTest("this RTC has no alarm")
        rtcwake = Rtcwake(self.root)
        self.addCleanup(rtcwake.run, "-m", "disable")        # don't leave it armed
        rtcwake.ok("-m", "no", "-s", "3600")
        self.assertRegex(rtcwake.ok("-m", "show"), r"^alarm: on \d{4}-")
        rtcwake.ok("-m", "disable")
        self.assertEqual(rtcwake.ok("-m", "show"), "alarm: off")

    def test_arguments(self):
        for args in (("-r", "-w"), ("-z",)):
            with self.subTest(args=args):
                Hwclock().rejects(*args, status=EX_USAGE)
        for args in (("-m", "bogus"), ("-m", "no"), ()):
            with self.subTest(args=args):
                Rtcwake().rejects(*args, status=EX_USAGE)
        Hwclock().rejects("-r", status=EX_NOPERM)   # the user can't open it
