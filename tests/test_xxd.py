# SPDX-License-Identifier: GPL-3.0-or-later
from base import EX_NOINPUT, EX_USAGE, SmolTest
from tools import Echo, Rm, Xxd


def xxd_format(data, offset=0):
    """What xxd prints for data: offset, byte pairs, then the printable text."""
    lines = []
    for at in range(0, len(data), 16):
        row = data[at:at + 16]
        pairs = []
        for i in range(0, 16, 2):
            chunk = row[i:i + 2]
            pairs.append(chunk.hex().ljust(4) if chunk else "    ")
        text = "".join(chr(b) if 32 <= b < 127 else "." for b in row)
        lines.append(f"{offset + at:08x}: {' '.join(pairs)} {text.ljust(16)}")
    return "\n".join(lines)


class TestXxd(SmolTest):
    data = b"hello world, this is thirty bytes\n"   # 34: two full rows and a two byte tail

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.path = "/tmp/TestXxd_bytes"
        cls.addClassCleanup(Rm().run, cls.path)
        Echo().to_file(cls.path, cls.data[:-1].decode())

    def test_dump(self):
        dump = Xxd().ok(self.path)
        # one line spelt out so xxd_format() is checked against something real
        self.assertEqual(dump.splitlines()[0],
                         "00000000: 6865 6c6c 6f20 776f 726c 642c 2074 6869 hello world, thi")
        self.assertEqual(dump, xxd_format(self.data))

    def test_length(self):
        self.assertEqual(Xxd().ok("-l", "7", self.path), xxd_format(self.data[:7]))

    def test_skip_and_offset(self):
        self.assertEqual(Xxd().ok("-s", "6", self.path), xxd_format(self.data[6:], 6))
        self.assertEqual(Xxd().ok("-s", "6", "-o", "0x100", self.path), xxd_format(self.data[6:], 0x106))

    def test_errors(self):
        cases = [
            ((), EX_USAGE),
            (("-l", "x", self.path), EX_USAGE),
            (("/nonexistent",), EX_NOINPUT),
        ]
        for args, status in cases:
            with self.subTest(args=args):
                Xxd().rejects(*args, status=status)

    def test_input_with_no_size(self):
        # devices, pipes and /proc files stat as 0 bytes, read them to the end or -l
        self.assertIn("Linux", Xxd().ok("/proc/version"))
        self.assertEqual(Xxd().ok("-l", "16", "/dev/zero"), xxd_format(bytes(16)))
