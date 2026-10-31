# SPDX-License-Identifier: GPL-3.0-or-later
"""The gpio tools, against a gpio-mockup chip when the kernel has one (gpio_mockup.gpio_mockup_ranges=-1,8)."""

import re
import time
import unittest

from base import EX_DATAERR, EX_NOINPUT, EX_UNAVAILABLE, EX_USAGE, SmolTest, Tool
from tools import Chmod, Ls

CHIP = "gpiochip0"


class Gpiodetect(Tool):
    name = "gpiodetect"

    def chips(self):
        """{name: (label, lines)}"""
        chips = {}
        for line in self.ok().splitlines():
            m = re.match(r"(\S+) \[(.*)\] \((\d+) lines\)$", line)
            chips[m.group(1)] = (m.group(2), int(m.group(3)))
        return chips


class Gpioinfo(Tool):
    name = "gpioinfo"

    def lines(self, chip):
        """{offset: (name, consumer, direction, active, flags)}"""
        lines = {}
        for line in self.ok(chip).splitlines()[1:]:
            m = re.match(r"\tline\s+(\d+): (\S+)\s+(\S+)\s+(input|output)\s+(active-\w+)\s*(.*)$", line)
            lines[int(m.group(1))] = (m.group(2), m.group(3), m.group(4), m.group(5), m.group(6).split())
        return lines


class Gpioget(Tool):
    name = "gpioget"

    def values(self, chip, *offsets):
        return [int(v) for v in self.ok(chip, *map(str, offsets)).split()]


class Gpioset(Tool):
    name = "gpioset"


class Gpiomon(Tool):
    name = "gpiomon"


class TestGpio(SmolTest):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        if not Ls().exists(f"/dev/{CHIP}"):
            raise unittest.SkipTest("no gpio chip on this machine")
        if not Ls().exists("/bin/gpiodetect"):
            raise unittest.SkipTest("no gpio tools in this image")
        # the node is root only and the tests use both shells, put it back afterwards
        Chmod(cls.root).ok("666", f"/dev/{CHIP}")
        cls.addClassCleanup(Chmod(cls.root).run, "600", f"/dev/{CHIP}")

    def test_detect(self):
        label, lines = Gpiodetect().chips()[CHIP]
        self.assertTrue(label)
        self.assertGreaterEqual(lines, 1)

    def test_info(self):
        lines = Gpioinfo().lines(CHIP)
        self.assertEqual(len(lines), Gpiodetect().chips()[CHIP][1])
        name, consumer, direction, active, flags = lines[0]
        self.assertEqual((consumer, direction, active), ("unused", "input", "active-high"))
        self.assertEqual(Gpioinfo().ok(), Gpioinfo().ok(CHIP))   # one chip, so the same either way

    def test_get(self):
        self.assertEqual(Gpioget().values(CHIP, 5, 6, 7), [0, 0, 0])   # lines no test drives
        self.assertEqual(Gpioget().values("0", 0), [0])          # the chip by number

    def test_set_holds_the_line(self):
        self.user.send(f"gpioset -t 10 {CHIP} 1=1")
        try:
            deadline = time.monotonic() + 5          # until gpioset has got the line
            while True:
                name, consumer, direction, active, flags = Gpioinfo(self.root).lines(CHIP)[1]
                if consumer == "gpioset" or time.monotonic() > deadline:
                    break
                time.sleep(0.1)
            self.assertEqual((consumer, direction), ("gpioset", "output"))
            self.assertIn("[used]", flags)
            Gpioget(self.root).rejects(CHIP, "1", status=EX_UNAVAILABLE)   # busy while held
        finally:
            self.wait_prompt(self.user)
        self.assertEqual(Gpioinfo().lines(CHIP)[1][1], "unused")

    def test_set_without_hold_lets_go(self):
        # a released line's value is the chip's business, so only that it was let go is checked
        Gpioset().ok(CHIP, "2=1", "3=0")
        self.assertEqual(Gpioinfo().lines(CHIP)[2][1], "unused")
        Gpioget().values(CHIP, 2, 3)

    def test_active_low(self):
        self.assertEqual(Gpioget().ok("-l", CHIP, "4"), "1")

    def test_bad_parameters(self):
        Gpiodetect().rejects("extra", status=EX_USAGE)
        Gpioinfo().rejects("/dev/null", status=EX_DATAERR)
        Gpioinfo().rejects("gpiochip99", status=EX_NOINPUT)
        Gpioget().rejects(status=EX_USAGE)
        Gpioget().rejects(CHIP, "x", status=EX_USAGE)
        Gpioget().rejects(CHIP, "65536", status=EX_USAGE)
        past_the_end = Gpiodetect().chips()[CHIP][1]                  # the chip decides, not the tool
        Gpioget().rejects(CHIP, str(past_the_end), status=EX_UNAVAILABLE)
        Gpioset().rejects(CHIP, "1", status=EX_USAGE)
        Gpioset().rejects(CHIP, "1=2", status=EX_USAGE)
        Gpioset().rejects("-t", "x", CHIP, "1=1", status=EX_USAGE)
        Gpiomon().rejects("-n", "0", CHIP, "1", status=EX_USAGE)
        Gpiomon().rejects(CHIP, status=EX_USAGE)

    def test_lines_above_63(self):
        # GPIO_V2_LINES_MAX is a per request limit, not an offset limit
        if Gpiodetect().chips()[CHIP][1] <= 64:
            self.skipTest("the chip has no line 64")
        self.assertEqual(Gpioget().values(CHIP, 64), [0])
        self.assertIn(64, Gpioinfo().lines(CHIP))
