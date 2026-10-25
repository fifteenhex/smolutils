#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""
A machine made of qemu: a kernel, an initramfs or rootfs and some serial
ports. Knows nothing about smolutils, just boot(), shutdown() and a Console
per serial port. They come out as ttyS0, ttyS1, .. in the order given and
live at q.consoles[name], or q.console, q.securetty, q.tty for the usual three.

nic="e1000" puts that card on qemu's user mode network: DHCP, DNS, tftp_dir
over tftp and a forward to the guest's telnet port, see Network.

Run directly to boot an image, print the console until init starts and
shut down.
"""

import argparse
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time

from console import Console, Timeout


def free_port():
    """A TCP port nothing on the host is using right now. Racy, fine for tests."""
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class Network:
    """The guest's view of qemu's user mode network, fixed by qemu."""

    gateway = "10.0.2.2"      # also the tftp server
    dns = "10.0.2.3"
    address = "10.0.2.15"     # the first DHCP lease
    netmask = 24
    gateway6 = "fec0::2"      # IPv6 on the fec0::/64 prefix qemu advertises
    address6 = "fec0::5054:ff:fe12:3456"   # SLAAC from qemu's default MAC

    telnet_port = 23          # in the guest, host_telnet_port on the host side

    def __init__(self, nic, tftp_dir=None):
        self.nic = nic
        self.tftp_dir = tftp_dir
        self.host_telnet_port = free_port()


class Qemu:
    """
    An initramfs (cpio.gz) or a rootfs (raw image, virtio disk as root), not
    both. The kernel console is the first serial port, append goes after it
    and may be changed up until boot().
    """

    def __init__(self, qemu, kernel, initramfs=None, rootfs=None, append="",
                 serials=("console", "securetty", "tty"), memory=128, machine="pc",
                 nic=None, tftp_dir=None):
        if (initramfs is None) == (rootfs is None):
            raise ValueError("give an initramfs or a rootfs, not both or neither")
        if not serials:
            raise ValueError("need at least one serial port for the kernel console")

        self.qemu = qemu
        self.kernel = kernel
        self.initramfs = initramfs
        self.rootfs = rootfs
        self.append = append
        self.memory = memory
        self.machine = machine
        self.network = Network(nic, tftp_dir) if nic else None

        self.consoles = {}
        for n, name in enumerate(serials):
            # The name ends up in qemu options and as an attribute here
            if not re.fullmatch(r"[a-z][a-z0-9_]*", name) or hasattr(self, name):
                raise ValueError(f"bad serial port name {name!r}")
            self.consoles[name] = Console(name, f"/dev/ttyS{n}")
            setattr(self, name, self.consoles[name])

        self.process = None
        self.log = None
        self.tmpdir = None

    def command_line(self):
        first = next(iter(self.consoles.values()))

        cmd = [self.qemu,
               "-machine", self.machine,
               "-m", str(self.memory),
               "-display", "none",
               "-monitor", "none",
               "-no-reboot",
               "-kernel", self.kernel]

        for console in self.consoles.values():
            cmd += ["-chardev", f"socket,id={console.name},path={self.socket_path(console)}",
                    "-serial", f"chardev:{console.name}"]

        append = [f"console={first.device.removeprefix('/dev/')}", "panic=1"]

        if self.initramfs:
            cmd += ["-initrd", self.initramfs]
        else:
            cmd += ["-drive", f"file={self.rootfs},format=raw,if=virtio"]
            append = ["root=/dev/vda", "ro"] + append

        if self.append:
            append.append(self.append)
        cmd += ["-append", " ".join(append)]

        if self.network:
            netdev = "user,id=net0"
            if self.network.tftp_dir:
                netdev += f",tftp={self.network.tftp_dir}"
            netdev += f",hostfwd=tcp:127.0.0.1:{self.network.host_telnet_port}-:{self.network.telnet_port}"
            cmd += ["-netdev", netdev, "-device", f"{self.network.nic},netdev=net0"]

        return cmd

    def socket_path(self, console):
        return os.path.join(self.tmpdir, console.name)

    def output(self):
        """What qemu itself has printed, errors and the like."""
        if self.log is None:
            return ""
        self.log.seek(0)
        return self.log.read().decode(errors="replace")

    def boot(self, timeout=10):
        """Start qemu and connect the serial ports, nothing is waited for."""
        if self.tmpdir is not None:
            raise RuntimeError("already booted")

        self.tmpdir = tempfile.mkdtemp(prefix="smolutils-qemu-")
        listeners = []
        try:
            # We listen on a socket per port, qemu connects to them as it starts
            for console in self.consoles.values():
                listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                listener.bind(self.socket_path(console))
                listener.listen(1)
                listener.settimeout(0.2)
                listeners.append((console, listener))

            self.log = open(os.path.join(self.tmpdir, "qemu.log"), "w+b")
            self.process = subprocess.Popen(self.command_line(),
                                            stdin=subprocess.DEVNULL,
                                            stdout=self.log,
                                            stderr=subprocess.STDOUT)

            deadline = time.monotonic() + timeout
            for console, listener in listeners:
                while True:
                    try:
                        sock, _ = listener.accept()
                        break
                    except TimeoutError:
                        pass
                    if self.process.poll() is not None:
                        raise RuntimeError(f"qemu exited with {self.process.returncode}: {self.output()}")
                    if time.monotonic() > deadline:
                        raise RuntimeError(f"qemu didn't connect {console.name}: {self.output()}")
                console.attach(sock)
        except BaseException:
            self.shutdown()
            raise
        finally:
            for _, listener in listeners:
                listener.close()

    def running(self):
        return self.process is not None and self.process.poll() is None

    def shutdown(self, timeout=10):
        """Stop qemu, kill it if it won't terminate."""
        if self.running():
            self.process.terminate()
            try:
                self.process.wait(timeout)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()

        for console in self.consoles.values():
            console.close()

        if self.log:
            self.log.close()
            self.log = None

        if self.tmpdir:
            shutil.rmtree(self.tmpdir, ignore_errors=True)
            self.tmpdir = None


def main():
    parser = argparse.ArgumentParser(description="boot an image under qemu and shut it down again")
    parser.add_argument("--qemu", required=True, help="qemu-system-* binary")
    parser.add_argument("--kernel", required=True)
    parser.add_argument("--initramfs", help="cpio.gz to boot from")
    parser.add_argument("--rootfs", help="disk image to boot from instead")
    parser.add_argument("--append", default="", help="extra kernel command line")
    parser.add_argument("--nic", help="network card model to put on the user mode network")
    parser.add_argument("--until", default="Run /init as init process",
                        help="console text that means the boot worked")
    parser.add_argument("--timeout", type=float, default=60)
    args = parser.parse_args()

    q = Qemu(args.qemu, args.kernel, initramfs=args.initramfs, rootfs=args.rootfs,
             append=args.append, nic=args.nic)
    try:
        q.boot()
        print(q.console.read_until(args.until, args.timeout), end="")
        print("\n--- booted, shutting down")
        return 0
    except Timeout as e:
        print(e.output, end="")
        print(f"\n--- {e}")
        return 1
    finally:
        q.shutdown()


if __name__ == "__main__":
    sys.exit(main())
