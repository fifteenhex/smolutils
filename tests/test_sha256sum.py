# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib

from base import EX_DATAERR, EX_IOERR, EX_NOINPUT, EX_USAGE, SmolTest
from tools import Echo, Sha256sum, Touch


class TestSha256sum(SmolTest):
    def check(self, name, text):
        path = self.tmp(name)
        Echo().to_file(path, text)
        self.assertEqual(Sha256sum().digest(path), hashlib.sha256(text.encode() + b"\n").hexdigest())

    def test_matches_python(self):
        self.check("short", "the quick brown fox")

    def test_more_than_a_block(self):
        # sha256 works in 64 byte blocks, make it do a few and a partial one
        self.check("long", "x" * 150)

    def test_empty_file(self):
        path = self.tmp("empty")
        Touch().ok(path)
        self.assertEqual(Sha256sum().digest(path), hashlib.sha256(b"").hexdigest())

    def test_errors(self):
        cases = [
            ((), EX_USAGE),
            (("/nonexistent",), EX_NOINPUT),
            (("/tmp",), EX_DATAERR),
            # a sysfs attribute stats as a page but reads a few bytes, which must not hash as a page
            (("/sys/kernel/uevent_seqnum",), EX_IOERR),
        ]
        for args, status in cases:
            with self.subTest(args=args):
                Sha256sum().rejects(*args, status=status)
