# SPDX-License-Identifier: GPL-3.0-or-later
from base import EX_NOINPUT, EX_USAGE, SmolTest, Tool
from tools import Ls


class I2cdetect(Tool):
    name = "i2cdetect"


class I2cget(Tool):
    name = "i2cget"


class I2cset(Tool):
    name = "i2cset"


class I2cdump(Tool):
    name = "i2cdump"


class TestI2c(SmolTest):
    def setUp(self):
        if not Ls().exists("/bin/i2cdetect"):
            self.skipTest("no i2c tools in this image")

    def test_detect_scans_a_bus(self):
        if not Ls().exists("/dev/i2c-0"):
            self.skipTest("no I2C bus on this machine")
        grid = I2cdetect(self.root).ok("-y", "0").splitlines()
        self.assertTrue(grid[0].startswith("     0  1  2"))
        self.assertEqual(len(grid), 9)

    def test_no_such_bus(self):
        I2cdetect().rejects("99", status=EX_NOINPUT)
        I2cget().rejects("99", "0x50", status=EX_NOINPUT)

    def test_arguments(self):
        I2cget().rejects(status=EX_USAGE)
        I2cget().rejects("0", "0x80", status=EX_USAGE)
        I2cget().rejects("0", "0x50", "0x100", status=EX_USAGE)
        I2cset().rejects("0", "0x50", status=EX_USAGE)
        I2cset().rejects("0", "0x50", "0", "0x10000", "w", status=EX_USAGE)
        I2cdetect().rejects("0", "0x03", status=EX_USAGE)           # a range needs both ends
        I2cdetect().rejects("0", "0x70", "0x10", status=EX_USAGE)
        I2cdump().rejects("0", status=EX_USAGE)
