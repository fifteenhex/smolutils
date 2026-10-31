# SPDX-License-Identifier: GPL-3.0-or-later
"""ps, kill and free from process.c."""

from base import EX_NOINPUT, EX_NOPERM, EX_USAGE, SmolTest, Tool
from tools import Kill, Ps


class Free(Tool):
    name = "free"

    def mem(self):
        """(total, used, free, cache) in kB"""
        header, mem = self.ok().splitlines()
        return tuple(int(v) for v in mem.split()[1:])


class TestProcess(SmolTest):
    def test_ps_shows_init_and_us(self):
        table = Ps().table()
        self.assertEqual(table[1], ("root", "init"))
        self.assertIn(("1024", "smolsh"), table.values())
        self.assertIn(("1024", "ps"), table.values())

    def test_kill_a_blocked_command(self):
        self.user.send("cat")                 # waits on stdin until killed
        try:
            pids = Ps(self.root).wait_for("cat")
            self.assertEqual(len(pids), 1)
            Kill(self.root).ok("-9", str(pids[0]))
        finally:
            for pid in Ps(self.root).pid_of("cat"):
                Kill(self.root).run("-9", str(pid))
            self.wait_prompt(self.user)
        self.assertEqual(Ps(self.root).pid_of("cat"), [])

    def test_kill_arguments(self):
        Kill().rejects(status=EX_USAGE)
        Kill().rejects("abc", status=EX_USAGE)
        Kill().rejects("-x", "1", status=EX_USAGE)
        Kill().rejects("1", status=EX_NOPERM)         # not ours to kill
        Kill().rejects("99999", status=EX_NOINPUT)    # nobody there

    def test_ps_and_free_take_no_arguments(self):
        Ps().rejects("aux", status=EX_USAGE)
        Free().rejects("-h", status=EX_USAGE)

    def test_free(self):
        total, used, free, cache = Free().mem()
        self.assertGreater(total, 0)
        self.assertLessEqual(free, total)
        self.assertLessEqual(used, total)
