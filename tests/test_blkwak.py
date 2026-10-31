# SPDX-License-Identifier: GPL-3.0-or-later
"""blkwak works on a block device or a plain file, so it is tested on files. It lives in /sbin, so as root."""

from base import EX_USAGE, SmolTest, Tool
from tools import Cat, Echo


def opt(*args):
    """The optional trailing arguments that were given, as strings."""
    return [str(a) for a in args if a is not None]


class Blkwak(Tool):
    name = "blkwak"

    def info(self, dev):
        return self.ok("info", dev)

    def read(self, dev, into, off=None, length=None):
        return self.ok("read", dev, into, *opt(off, length))

    def write(self, dev, image, off=None):
        return self.ok("-v", "write", dev, image, *opt(off))

    def cmp(self, dev, image, off=None):
        """The Result, not the output: differing is a status, not a failure."""
        return self.run("cmp", dev, image, *opt(off))


class TestBlkwak(SmolTest):
    def setUp(self):
        self.dev = self.tmp("dev")
        self.image = self.tmp("img")
        self.blkwak = Blkwak(self.root)
        Echo(self.root).to_file(self.dev, "0123456789abcdef")
        Echo(self.root).to_file(self.image, "0123456789abcdef")

    def test_info_on_a_file(self):
        self.assertIn("size 17 bytes", self.blkwak.info(self.dev))

    def test_cmp_same_and_different(self):
        self.assertIn("17 bytes match", self.blkwak.cmp(self.dev, self.image).output)
        Echo(self.root).to_file(self.image, "0123456789abcdeX")
        result = self.blkwak.cmp(self.dev, self.image)
        self.assertEqual(result.status, 1)                   # like cmp(1)
        self.assertIn("1 of 17 bytes differ, first at 0xf", result.output)

    def test_read_part_of_a_device(self):
        out = self.tmp("out")
        self.blkwak.read(self.dev, out, 2, 5)
        self.assertEqual(Cat(self.root).ok(out), "23456")

    def test_write_at_an_offset_and_verify(self):
        patch = self.tmp("patch")
        Echo(self.root).to_file(patch, "XY")
        self.blkwak.write(self.dev, patch, 4)
        self.assertEqual(Cat(self.root).ok(self.dev), "0123XY\n789abcdef")

    def test_errors(self):
        cases = [(), ("frob", self.dev), ("read", self.dev, self.tmp("x"), "100"),   # past the end
                 ("cmp", self.dev, self.dev), ("-b", "x", "info", self.dev)]
        for args in cases:
            with self.subTest(args=args):
                self.blkwak.rejects(*args, status=EX_USAGE)
