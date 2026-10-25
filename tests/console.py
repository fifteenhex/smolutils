# SPDX-License-Identifier: GPL-3.0-or-later
"""
A serial port seen from our end. A thread drains it so the guest never
blocks on us, read_until() waits for text to turn up.
"""

import socket
import threading
import time


class Timeout(Exception):
    def __init__(self, name, waiting_for, output):
        super().__init__(f"{name}: timed out waiting for {waiting_for!r}")
        self.output = output   # what did turn up


class Closed(Exception):
    """The other end went away, qemu died or the guest panicked."""


class Console:
    def __init__(self, name, device):
        self.name = name
        self.device = device       # what the other side calls it, /dev/ttyS1
        self.sock = None
        self.thread = None
        self.closed = True
        self.output = b""          # everything printed so far
        self.pending = b""         # printed but not yet returned by read_until()
        self.changed = threading.Condition()

    def attach(self, sock):
        """Start reading from a connected socket."""
        self.sock = sock
        self.closed = False
        self.thread = threading.Thread(target=self._reader, daemon=True)
        self.thread.start()

    def _reader(self):
        while True:
            try:
                chunk = self.sock.recv(65536)
            except OSError:
                chunk = b""
            with self.changed:
                if chunk:
                    self.output += chunk
                    self.pending += chunk
                else:
                    self.closed = True
                self.changed.notify_all()
            if not chunk:
                return

    def close(self):
        """Stop reading and close the socket."""
        if self.sock is None:
            return
        try:
            self.sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        self.thread.join()
        self.sock.close()
        self.sock = None

    def _wait(self, found, waiting_for, timeout):
        """Block until found() is true, Timeout or Closed otherwise. Call with the lock held."""
        deadline = None if timeout is None else time.monotonic() + timeout
        while not found():
            if self.closed:
                raise Closed(f"{self.name}: closed while waiting for {waiting_for!r}")
            left = None if deadline is None else deadline - time.monotonic()
            if left is not None and left <= 0:
                raise Timeout(self.name, waiting_for, self.pending.decode(errors="replace"))
            self.changed.wait(left)

    def read_until(self, texts, timeout=60):
        """
        Return what was printed up to and including the first of texts (a
        string or a tuple of them) to turn up.
        """
        if isinstance(texts, str):
            texts = (texts,)
        wants = [t.encode() for t in texts]

        def find():
            hits = [(at, w) for w in wants if (at := self.pending.find(w)) >= 0]
            return min(hits) if hits else None

        with self.changed:
            self._wait(lambda: find() is not None, texts if len(texts) > 1 else texts[0], timeout)
            at, want = find()
            got, self.pending = self.pending[:at + len(want)], self.pending[at + len(want):]
            return got.decode(errors="replace")

    def find(self, text, start=0, timeout=60):
        """
        Wait for text in everything printed from offset start on and return
        the offset just past it. Consumes nothing, for peeking at a console
        someone else is reading.
        """
        want = text.encode()
        with self.changed:
            self._wait(lambda: self.output.find(want, start) >= 0, text, timeout)
            return self.output.find(want, start) + len(want)

    def send(self, line):
        """Type a line, the newline is added."""
        if self.sock is None or self.closed:
            raise Closed(f"{self.name}: closed")
        self.sock.sendall(line.encode() + b"\n")

    def __repr__(self):
        return f"Console({self.name}, {self.device})"
