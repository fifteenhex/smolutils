# SPDX-License-Identifier: GPL-3.0-or-later
from base import EX_USAGE, SmolTest, Tool


class Df(Tool):
    name = "df"

    def table(self):
        """{mountpoint: (filesystem, blocks, used, available, percent)}"""
        rows = {}
        for line in self.ok().splitlines()[1:]:
            fs, blocks, used, avail, pct, mount = line.split()
            rows[mount] = (fs, int(blocks), int(used), int(avail), int(pct.rstrip("%")))
        return rows


class TestDf(SmolTest):
    def test_tmpfs_mounts_are_listed(self):
        table = Df().table()
        for source, mount in (("tmp", "/tmp"), ("run", "/run")):
            fs, blocks, used, avail, pct = table[mount]
            self.assertEqual(fs, source)
            self.assertGreater(blocks, 0)
            self.assertLessEqual(used + avail, blocks)
            self.assertLessEqual(pct, 100)

    def test_takes_no_arguments(self):
        Df().rejects("/tmp", status=EX_USAGE)
