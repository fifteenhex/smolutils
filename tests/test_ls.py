# SPDX-License-Identifier: GPL-3.0-or-later
from base import EX_NOINPUT, EX_USAGE, SmolTest
from tools import Echo, Ls


class TestLs(SmolTest):
    def test_lists_what_is_there(self):
        a, b = self.tmp("a"), self.tmp("b")
        Echo().to_file(a, "x")
        Echo().to_file(b, "yy")
        self.assertTrue(Ls().exists(a))
        self.assertTrue(Ls().exists(b))

    def test_long_format(self):
        path = self.tmp("long")
        Echo().to_file(path, "12345")
        entry = Ls().entry(path)
        self.assertEqual(entry.mode, "-rw-r--r--")
        self.assertEqual((entry.links, entry.uid, entry.gid, entry.size), (1, "1024", "1024", "6"))

    def test_root_directory(self):
        names = Ls().names("/")
        for expected in ("bin", "sbin", "dev", "proc", "sys", "tmp", "run"):
            self.assertIn(expected, names)

    def test_missing_directory_fails(self):
        Ls().rejects("/nonexistent", status=EX_NOINPUT)

    def test_unknown_option_fails(self):
        Ls().rejects("-z", status=EX_USAGE)

    def test_proc_lists_even_as_processes_come_and_go(self):
        self.assertIn("self", Ls().ok("-l", "/proc"))
