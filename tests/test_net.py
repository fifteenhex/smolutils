# SPDX-License-Identifier: GPL-3.0-or-later
"""The network tools. With a network (run.py --nic) dhcpc, ping, resolv and tftp are tried for
real against what qemu's user mode network offers, without one only the loopback and the
argument checks can be tested. IPv6 is tested once the tools speak it. resolv is tried against
whatever DNS the host has, so that has to work."""

import os
import socket
import struct

from base import EX_NOINPUT, EX_UNAVAILABLE, EX_USAGE, SmolTest, Tool
from tools import IFACE, Cat, Dhcpc


class Ip(Tool):
    name = "ip"

    def links(self):
        """{name: flags} from ip link"""
        links = {}
        for line in self.ok("link").splitlines():
            if not line.startswith(" "):
                name, rest = line.split(": ", 1)
                links[name] = rest.split()[0]
        return links

    def addrs(self):
        """{name: [address/prefix, ...]} from ip addr"""
        addrs, name = {}, None
        for line in self.ok("addr").splitlines():
            if not line.startswith(" "):
                name = line.split(":", 1)[0]
                addrs[name] = []
            elif line.split()[0] == "inet":
                addrs[name].append(line.split()[1])
        return addrs


class Ping(Tool):
    name = "ping"


class Resolv(Tool):
    name = "resolv"


class Tftp(Tool):
    name = "tftp"

    def get(self, host, remote, local):
        return self.ok("-g", "-r", remote, "-l", local, host)


def routes(shell):
    """[(iface, destination, gateway, flags)] from /proc/net/route, addresses dotted."""
    def dotted(hex_le):
        return socket.inet_ntoa(struct.pack("<I", int(hex_le, 16)))

    table = []
    for line in Cat(shell).ok("/proc/net/route").splitlines()[1:]:
        iface, dst, gw, flags = line.split()[:4]
        table.append((iface, dotted(dst), dotted(gw), int(flags, 16)))
    return table


class TestNet(SmolTest):
    def lease(self):
        """A network, with the interface configured by dhcpc."""
        net = self.network()
        Dhcpc(self.root).lease()
        return net

    def test_loopback_is_up(self):
        self.assertIn("UP", Ip().links()["lo"])
        self.assertIn("127.0.0.1", Ip().ok("addr"))

    def test_dhcpc_configures_the_interface(self):
        net = self.lease()
        self.assertIn("UP", Ip().links()[IFACE])
        self.assertEqual(Ip().addrs()[IFACE], [f"{net.address}/{net.netmask}"])
        RTF_UP, RTF_GATEWAY = 1, 2
        self.assertIn((IFACE, "0.0.0.0", net.gateway, RTF_UP | RTF_GATEWAY), routes(self.user))

    def test_ping(self):
        net = self.lease()
        self.assertIn(f"Reply from {net.gateway}: seq=1", Ping().ok(net.gateway))

    def test_resolv_uses_the_dhcp_server(self):
        self.lease()
        Resolv().rejects("nosuch.invalid", status=EX_NOINPUT)   # what localhost gives depends on the host

    def test_tftp(self):
        net = self.lease()
        with open(os.path.join(net.tftp_dir, "served.txt"), "w") as f:
            f.write("over tftp\n" * 100)        # more than one block
        got = self.tmp("got")
        Tftp().get(net.gateway, "served.txt", got)
        self.assertEqual(Cat().ok(got), ("over tftp\n" * 100).strip())
        self.assertIn("over tftp", Tftp().get(net.gateway, "served.txt", "-"))   # - is stdout
        Tftp().rejects("-g", "-r", "nosuch.txt", "-l", self.tmp("no"), net.gateway, status=EX_NOINPUT)

    def test_ip_arguments(self):
        Ip().rejects("route", status=EX_USAGE)

    def test_ping_arguments(self):
        Ping().rejects(status=EX_USAGE)
        Ping().rejects("a", "b", status=EX_USAGE)

    def test_resolv_arguments(self):
        Resolv().rejects("-z", "host", status=EX_USAGE)
        Resolv().rejects("-m", "x", "host", status=EX_USAGE)

    def test_tftp_arguments(self):
        Tftp().rejects(status=EX_USAGE)
        Tftp().rejects("-g", "host", status=EX_USAGE)

    def test_dhcpc_arguments(self):
        Dhcpc(self.root).rejects("-z", status=EX_USAGE)   # dhcpc is in /sbin
        Dhcpc(self.root).rejects("-i", "nosuchif", status=EX_UNAVAILABLE)

