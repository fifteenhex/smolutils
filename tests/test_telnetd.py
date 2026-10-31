# SPDX-License-Identifier: GPL-3.0-or-later
"""telnetd from the host, through a port qemu forwards to the guest when there is a network."""

import re
import socket
import time

import base
from base import SmolTest
from tools import Dhcpc

IAC, WILL, DO = 255, 251, 253
ECHO, SGA = 1, 3


class Telnet:
    """Just enough of a client: the raw bytes, and the text with the telnet commands taken out."""

    def __init__(self, port):
        self.sock = socket.socket()
        self.sock.settimeout(10)
        self.sock.connect(("127.0.0.1", port))
        self.raw = b""

    def read_until(self, text, timeout=30):
        deadline = time.monotonic() + timeout
        while text not in self.text():
            left = deadline - time.monotonic()
            if left <= 0:
                raise AssertionError(f"never saw {text!r} in {self.text()!r}")
            self.sock.settimeout(left)                    # the whole wait, not 10s a recv()
            try:
                chunk = self.sock.recv(4096)
            except socket.timeout:
                continue
            if not chunk:
                raise AssertionError(f"closed before {text!r}, got {self.text()!r}")
            self.raw += chunk
        return self.text()

    def text(self):
        return re.sub(rb"\xff[\xfb-\xfe].|\xff\xfa.*?\xff\xf0", b"", self.raw, flags=re.S).decode(errors="replace")

    def send(self, line):
        self.sock.sendall(line.encode() + b"\r\n")

    def close(self):
        self.sock.close()


class TestTelnetd(SmolTest):
    def setUp(self):
        self.net = self.network()
        Dhcpc(self.root).lease()                    # qemu forwards to the leased address

    def connect(self):
        client = Telnet(self.net.host_telnet_port)
        self.addCleanup(client.close)
        return client

    def test_offers_echo_and_sga(self):
        client = self.connect()
        client.read_until("code: ")
        self.assertTrue(client.raw.startswith(bytes([IAC, WILL, ECHO, IAC, WILL, SGA, IAC, DO, SGA])))

    def test_log_in_and_run_a_command(self):
        client = self.connect()
        client.read_until("code: ")
        base.smol.answer_code(client, "/dev/pts/")   # the session is a pty, the code goes to the secure tty
        client.read_until(" > ")
        client.send("echo over telnet")
        self.assertIn("over telnet", client.read_until("over telnet\r\n"))
