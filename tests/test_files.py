# SPDX-License-Identifier: GPL-3.0-or-later
"""The file tools in files.c."""

from base import EX_CANTCREAT, EX_DATAERR, EX_NOINPUT, EX_NOPERM, EX_USAGE, SmolTest
from tools import Cat, Chmod, Chown, Cp, Echo, Ln, Ls, Mkdir, Mknod, Mv, Rm, Rmdir, Touch


class TestFiles(SmolTest):
    def setUp(self):
        self.ls = Ls()

    def test_touch_creates_an_empty_file(self):
        path = self.tmp("new")
        Touch().ok(path)
        self.assertEqual(self.ls.entry(path).size, "0")
        Touch().ok(path)   # a second time is fine too

    def test_touch_bad_parameters(self):
        Touch().rejects("/nonexistent/dir/file", status=EX_CANTCREAT)

    def test_cat_one_and_many(self):
        a, b = self.tmp("a"), self.tmp("b")
        Echo().to_file(a, "first")
        Echo().to_file(b, "second")
        self.assertEqual(Cat().ok(a), "first")
        self.assertEqual(Cat().ok(a, b), "first\nsecond")
        Cat().rejects("/nonexistent", status=EX_NOINPUT)
        Cat().rejects(a, ">>", a, status=EX_DATAERR)      # would never end
        Cat().rejects(a, ">", a, status=EX_DATAERR)
        self.assertEqual(Cat().ok(a), "")                        # the shell truncated it first

    def test_cp_copies_contents(self):
        src, dst = self.tmp("src"), self.tmp("dst")
        Echo().to_file(src, "copy me")
        Cp().ok(src, dst)
        self.assertEqual(Cat().ok(dst), "copy me")
        self.assertEqual(Cat().ok(src), "copy me")

    def test_cp_onto_itself_is_refused(self):
        path = self.tmp("self")
        Echo().to_file(path, "keep me")
        Cp().rejects(path, path, status=EX_DATAERR)
        link = self.tmp("self_link")
        Ln().ok(path, link)                                   # the same file by another name
        Cp().rejects(path, link, status=EX_DATAERR)
        self.assertEqual(Cat().ok(path), "keep me")           # and nothing was truncated

    def test_cp_only_does_regular_files(self):
        Cp().rejects("/tmp", self.tmp("dir"), status=EX_DATAERR)

    def test_mv_renames(self):
        src, dst = self.tmp("from"), self.tmp("to")
        Echo().to_file(src, "moving")
        Mv().ok(src, dst)
        self.assertFalse(self.ls.exists(src))
        self.assertEqual(Cat().ok(dst), "moving")

    def test_mv_bad_parameters(self):
        Mv().rejects("onlyone", status=EX_USAGE)
        Mv().rejects("/nonexistent", self.tmp("x"), status=EX_NOINPUT)

    def test_mv_across_filesystems(self):
        src, dst = self.tmp("across"), "/run/across"   # /tmp and /run are separate tmpfs
        Echo().to_file(src, "far away")
        self.addCleanup(Rm(self.root).run, dst)
        Mv(self.root).ok(src, dst)
        self.assertFalse(self.ls.exists(src))
        self.assertEqual(Cat(self.root).ok(dst), "far away")

    def test_ln_symbolic(self):
        target, link = self.tmp("target"), self.tmp("link")
        Echo().to_file(target, "pointed at")
        Ln().symlink(target, link)
        entry = self.ls.entry(link)
        self.assertTrue(entry.mode.startswith("l"))
        self.assertEqual(entry.target, target)
        self.assertEqual(Cat().ok(link), "pointed at")

    def test_ln_hard(self):
        target, link = self.tmp("hard"), self.tmp("hardlink")
        Echo().to_file(target, "two names")
        Ln().ok(target, link)
        self.assertEqual(self.ls.entry(target).links, 2)
        self.assertEqual(Cat().ok(link), "two names")

    def test_ln_arguments(self):
        Ln().rejects("onlyone", status=EX_USAGE)
        Ln().rejects("-z", "a", "b", status=EX_USAGE)

    def test_rm(self):
        path = self.tmp("gone")
        Echo().to_file(path, "x")
        Rm().ok(path)
        self.assertFalse(self.ls.exists(path))
        Rm().rejects(path, status=EX_NOINPUT)

    def test_mkdir_and_rmdir(self):
        path = self.tmp("dir")
        Mkdir().ok(path)
        self.assertEqual(self.ls.entry(path).mode, "drwxr-xr-x")
        Mkdir().rejects(path, status=EX_CANTCREAT)        # already there
        Rmdir().ok(path)
        self.assertFalse(self.ls.exists(path))
        Rmdir().rejects(path, status=EX_NOINPUT)

    def test_chmod(self):
        path = self.tmp("mode")
        Echo().to_file(path, "x")
        Chmod().ok("600", path)
        self.assertEqual(self.ls.entry(path).mode, "-rw-------")
        Chmod().ok("755", path)
        self.assertEqual(self.ls.entry(path).mode, "-rwxr-xr-x")
        Chmod().rejects("rwx", path, status=EX_USAGE)

    def test_chown_needs_root(self):
        path = self.tmp("owned")
        Echo().to_file(path, "x")
        Chown().rejects("0:0", path, status=EX_NOPERM)
        Chown(self.root).ok("0:0", path)
        entry = self.ls.entry(path)
        self.assertEqual((entry.uid, entry.gid), ("root", "root"))
        Chown(self.root).ok("1024", path)
        self.assertEqual(self.ls.entry(path).uid, "1024")
        Chown(self.root).rejects("nobody", path, status=EX_USAGE)

    def test_mknod_needs_root(self):
        path = self.tmp("null")
        Mknod().rejects(path, "c", "1", "3", status=EX_NOPERM)
        Mknod(self.root).ok(path, "c", "1", "3")
        entry = self.ls.entry(path)
        self.assertEqual(entry.mode, "crw-------")
        self.assertEqual(entry.device, (1, 3))
        Mknod(self.root).rejects(path, "x", "1", "3", status=EX_USAGE)

    def test_argument_counts(self):
        for tool, args in ((Touch(), ()), (Rm(), ()), (Rmdir(), ("a", "b")), (Mkdir(), ()), (Cp(), ("onlyone",))):
            with self.subTest(tool=tool.name):
                tool.rejects(*args, status=EX_USAGE)

    def test_bare_hex_prefix_is_not_a_number(self):
        Mknod(self.root).rejects(self.tmp("hexnode"), "c", "0x", "1", status=EX_USAGE)
