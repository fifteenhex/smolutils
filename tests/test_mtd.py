# SPDX-License-Identifier: GPL-3.0-or-later
"""The mtd tools, against /dev/mtd0 when there is one. Writing needs a writable device, which
mtdram gives the test kernel (mtdram.total_size=1024 mtdram.erase_size=64)."""

from base import EX_DATAERR, EX_NOINPUT, EX_USAGE, SmolTest, Tool
from tools import Cat, Echo, Ls, Sha256sum, Xxd

DEV = "/dev/mtd0"


class Mtdinfo(Tool):
    name = "mtdinfo"

    def info(self, dev):
        """The 'Name: value' lines as a dict, the first line is the device."""
        lines = self.ok(dev).splitlines()
        return {k.strip(): v.strip() for k, v in (l.split(":", 1) for l in lines[1:])}


class FlashErase(Tool):
    name = "flash_erase"


class Nanddump(Tool):
    name = "nanddump"

    def to_file(self, dev, path, start=None, length=None):
        args = ["-f", path]
        if start is not None:
            args += ["-s", str(start)]
        if length is not None:
            args += ["-l", str(length)]
        return self.ok(*args, dev)


class Nandwrite(Tool):
    name = "nandwrite"


class Flashcp(Tool):
    name = "flashcp"


class TestMtd(SmolTest):
    def setUp(self):
        if not Ls().exists("/bin/mtdinfo"):
            self.skipTest("no mtd tools in this image")

    def device(self):
        """The tests that need the device, the argument checks don't."""
        if not Ls().exists(DEV):
            self.skipTest("no MTD on this machine")
        self.info = Mtdinfo(self.root).info(DEV)
        self.erasesize = int(self.info["Eraseblock size"].split()[0])
        self.size = int(self.info["Amount of eraseblocks"].split("(")[1].split()[0])

    def writable(self):
        self.device()
        if self.info["Device is writable"] != "true":
            self.skipTest("the MTD device is read-only")

    def test_mtdinfo(self):
        self.device()
        self.assertIn(self.info["Type"], ("nor", "nand", "ram", "rom", "dataflash", "ubi", "mlc-nand"))
        self.assertEqual(self.size % self.erasesize, 0)
        self.assertIn(self.info["Device is writable"], ("true", "false"))

    def test_dump_and_range_checks(self):
        self.device()
        out = self.tmp("dump")
        Nanddump(self.root).to_file(DEV, out, start=0, length=16)
        self.assertEqual(Ls(self.root).entry(out).size, "16")
        Nanddump(self.root).to_file(DEV, out, start=self.size - 8)        # the last 8 bytes
        self.assertEqual(Ls(self.root).entry(out).size, "8")
        Nanddump(self.root).rejects("-s", str(self.size), DEV, status=EX_USAGE)
        Nanddump(self.root).rejects("-s", str(self.size - 8), "-l", "9", DEV, status=EX_USAGE)
        Nanddump(self.root).rejects("-s", "x", DEV, status=EX_USAGE)

    def test_erase_write_and_read_back(self):
        self.writable()
        data, out = self.tmp("data"), self.tmp("back")
        Echo(self.root).to_file(data, "written to flash")
        FlashErase(self.root).ok("-q", DEV, "0", "1")
        Nanddump(self.root).to_file(DEV, out, start=0, length=4)
        self.assertIn("ffff ffff", Xxd(self.root).ok(out))               # erased flash reads as ones
        Nandwrite(self.root).ok("-p", DEV, data)
        Nanddump(self.root).to_file(DEV, out, start=0, length=17)
        self.assertEqual(Sha256sum(self.root).digest(out), Sha256sum(self.root).digest(data))

    def test_write_at_an_offset(self):
        self.writable()
        data, out = self.tmp("data2"), self.tmp("back2")
        Echo(self.root).to_file(data, "second block")
        FlashErase(self.root).ok("-q", DEV, str(self.erasesize), "1")
        Nandwrite(self.root).ok("-p", "-s", str(self.erasesize), DEV, data)
        Nanddump(self.root).to_file(DEV, out, start=self.erasesize, length=13)
        self.assertEqual(Cat(self.root).ok(out), "second block")
        Nandwrite(self.root).rejects("-s", str(self.size), DEV, data, status=EX_USAGE)

    def test_flashcp(self):
        self.writable()
        if self.info["Type"].endswith("nand"):
            self.skipTest("flashcp refuses NAND")
        data, out = self.tmp("image"), self.tmp("back3")
        Echo(self.root).to_file(data, "a whole image")
        Flashcp(self.root).ok(data, DEV)
        Nanddump(self.root).to_file(DEV, out, start=0, length=14)
        self.assertEqual(Cat(self.root).ok(out), "a whole image")
        Flashcp(self.root).rejects(self.tmp("nonexistent"), DEV, status=EX_NOINPUT)

    def test_erase_range_checks(self):
        self.writable()
        blocks = self.size // self.erasesize
        FlashErase(self.root).rejects("-q", DEV, "1", "1", status=EX_USAGE)
        FlashErase(self.root).rejects("-q", DEV, "0", str(blocks + 1), status=EX_USAGE)
        FlashErase(self.root).rejects("-q", DEV, str(self.size), "1", status=EX_USAGE)
        FlashErase(self.root).ok("-q", DEV, str(self.size - self.erasesize), "0")   # 0 means to the end

    def test_arguments(self):
        Mtdinfo().rejects(status=EX_USAGE)
        Mtdinfo().rejects("/dev/null", status=EX_DATAERR)
        FlashErase().rejects(DEV, status=EX_USAGE)
        FlashErase().rejects(DEV, "x", "1", status=EX_USAGE)
        Nanddump().rejects(status=EX_USAGE)
        Nandwrite().rejects(DEV, status=EX_USAGE)
        Flashcp().rejects("file", status=EX_USAGE)
