# SPDX-License-Identifier: GPL-3.0-or-later
"""The shell: builtins, redirections and pipes."""

from base import EX_NOINPUT, EX_USAGE, SmolTest, Tool
from smolutils import KILLED, PROMPT
from tools import Cat, Echo, Ps


class Pwd(Tool):
    name = "pwd"


class Whoami(Tool):
    name = "whoami"


class Cd(Tool):
    name = "cd"


class Sleep(Tool):
    name = "sleep"


class Sync(Tool):
    name = "sync"


class Clear(Tool):
    name = "clear"


class NoSuchCommand(Tool):
    name = "nosuchcommand"


class TestSmolsh(SmolTest):
    def test_echo(self):
        self.assertEqual(Echo().ok("hello", "there"), "hello there")

    def test_pwd_and_cd(self):
        self.assertEqual(Pwd().ok(), "/")
        self.addCleanup(Cd().run, "/")        # the shell is shared, put it back whatever happens
        Cd().ok("/tmp")
        self.assertEqual(Pwd().ok(), "/tmp")
        Cd().ok("/")
        Cd().rejects("/nonexistent", status=EX_NOINPUT)

    def test_whoami_user_and_root(self):
        self.assertEqual(Whoami(self.user).ok(), "1024")
        self.assertEqual(Whoami(self.root).ok(), "root")

    def test_unknown_command(self):
        NoSuchCommand().rejects(status=127)          # what every shell says for that

    def test_redirection(self):
        path = self.tmp("file")
        Echo().to_file(path, "one")
        Echo().ok("two", ">>", path)
        self.assertEqual(Cat().ok(path), "one\ntwo")
        Echo().to_file(path, "again")
        self.assertEqual(Cat().ok(path), "again")

    def test_stderr_redirection(self):
        path = self.tmp("err")
        Cat().fail("/nonexistent", "2>", path)
        self.assertNotEqual(Cat().ok(path), "")      # the complaint went into the file

    def test_pipe(self):
        path = self.tmp("piped")
        Echo().to_file(path, "through a pipe")
        self.assertEqual(Cat().ok(path, "|", "cat"), "through a pipe")

    def test_sleep(self):
        self.assertEqual(Sleep().ok("1"), "")
        Sleep().rejects(status=EX_USAGE)

    def test_builtin_arguments(self):
        for tool, args in ((Pwd(), ("extra",)), (Whoami(), ("-x",)), (Sync(), ("/",)),
                           (Clear(), ("1",)), (Cd(), ("a", "b"))):
            with self.subTest(tool=tool.name):
                tool.rejects(*args, status=EX_USAGE)

    def test_ctrl_c_ends_the_command(self):
        self.user.send("cat")                 # waits on the tty
        self.assertTrue(Ps(self.root).wait_for("cat"))
        self.user.send("\x03")
        try:
            output = self.wait_prompt(self.user)
        finally:
            for pid in Ps(self.root).pid_of("cat"):
                self.root.send(f"kill -9 {pid}")
                self.wait_prompt(self.root)
        killed = KILLED.search(PROMPT.sub("", output).rstrip())
        self.assertIsNotNone(killed, output)
        self.assertEqual(killed.group(1), "2")                      # SIGINT
        self.assertEqual(Ps(self.root).pid_of("cat"), [])      # reaped, not left a zombie
