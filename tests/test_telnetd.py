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

    def __init__(self, port, rcvbuf=None):
        self.sock = socket.socket()
        self.sock.settimeout(10)
        if rcvbuf:
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, rcvbuf)
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

    def connect(self, rcvbuf=None):
        client = Telnet(self.net.host_telnet_port, rcvbuf)
        self.addCleanup(client.close)
        return client

    def test_offers_echo_and_sga(self):
        client = self.connect()
        client.read_until("code: ")
        self.assertTrue(client.raw.startswith(bytes([IAC, WILL, ECHO, IAC, WILL, SGA, IAC, DO, SGA])))

    def login(self, rcvbuf=None):
        client = self.connect(rcvbuf)
        client.read_until("code: ")
        base.smol.answer_code(client, "/dev/pts/")   # the session is a pty, the code goes to the secure tty
        client.read_until(" > ")
        return client

    def test_log_in_and_run_a_command(self):
        client = self.login()
        client.send("echo over telnet")
        self.assertIn("over telnet", client.read_until("over telnet\r\n"))

    def test_a_client_that_stops_reading_doesnt_stall_the_others(self):
        # one loop serves every session, so a write to a client that isn't draining its socket must not hang it
        stuck = self.login(rcvbuf=4096)               # so the buffers fill quickly
        stuck.send("xxd -l 8000000 /dev/zero")       # far more than they hold, and never read
        time.sleep(20)                                # for them to fill, the pty only trickles
        other = self.login()
        other.send("echo still here")
        self.assertIn("still here", other.read_until("still here\r\n"))

    def test_input_for_a_busy_shell_waits_rather_than_being_lost(self):
        # the shell isn't reading while sleep runs and its pty only holds so much, the rest must wait its turn
        client = self.login()
        client.send("sleep 5")
        lines = "".join(f"echo flood {i}\r\n" for i in range(5000))   # far more than a pty buffers
        client.sock.settimeout(60)
        client.sock.sendall(lines.encode() + b"echo the end\r\n")
        self.assertIn("flood 4999", client.read_until("the end\r\n", timeout=120))
