# SPDX-License-Identifier: GPL-3.0-or-later
from base import EX_NOINPUT, EX_NOPERM, EX_USAGE, SmolTest, Tool
from tools import Cat, Echo, Mkdir, Rmdir


class Mount(Tool):
    name = "mount"

    def tmpfs(self, target):
        return self.ok("-t", "tmpfs", "tmp", target)


class Umount(Tool):
    name = "umount"


class TestMount(SmolTest):
    def test_mount_and_umount_a_tmpfs(self):
        target = self.tmp("mnt")
        Mkdir(self.root).ok(target)
        Mount(self.root).tmpfs(target)
        self.addCleanup(Umount(self.root).run, target)       # whatever happens below
        Echo(self.root).to_file(f"{target}/inside", "mounted")
        self.assertEqual(Cat(self.root).ok(f"{target}/inside"), "mounted")
        Umount(self.root).ok(target)
        Cat(self.root).rejects(f"{target}/inside", status=EX_NOINPUT)
        Rmdir(self.root).ok(target)

    def test_needs_root(self):
        Mount().rejects("-t", "tmpfs", "tmp", "/tmp", status=EX_NOPERM)

    def test_arguments(self):
        Mount().rejects(status=EX_USAGE)
        Mount().rejects("-t", "tmpfs", "tmp", status=EX_USAGE)
        Mount().rejects("-t", "tmpfs", "tmp", "/tmp", "extra", status=EX_USAGE)
        Mount().rejects("-t", "tmpfs", "-o", "bogus", "tmp", "/tmp", status=EX_USAGE)   # as the user, root would really mount
        Umount(self.root).rejects("/nonexistent", status=EX_NOINPUT)
        Umount().rejects(status=EX_USAGE)
