#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Run the tests against an image under qemu:

    tests/run.py --qemu QEMU --kernel BZIMAGE --initramfs x86_64.cpio.gz [--nic e1000] [test_ls ...]

Test modules can be listed, default is all of them. --nic is needed for the
network tests. The image needs CONFIG_EXITCODES_FINEGRAINED. --summary appends
a markdown table of the results to a file, for the github step summary.
"""

import argparse
import os
import sys
import unittest

here = os.path.dirname(os.path.abspath(__file__))


class Result(unittest.TextTestResult):
    """Counts outcomes per test module as well as printing them."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.outcomes = {}

    def record(self, test, outcome):
        # A failed setUpClass comes as "setUpClass (test_x.TestX)"
        module = test.id().split("(")[-1].split(".")[0]
        self.outcomes.setdefault(module, {"passed": 0, "failed": 0, "skipped": 0})[outcome] += 1

    def addSuccess(self, test):
        super().addSuccess(test)
        self.record(test, "passed")

    def addFailure(self, test, err):
        super().addFailure(test, err)
        self.record(test, "failed")

    def addError(self, test, err):
        super().addError(test, err)
        self.record(test, "failed")

    def addSkip(self, test, reason):
        super().addSkip(test, reason)
        self.record(test, "skipped")

    def summary(self):
        """The counts as a markdown table, then why anything failed or was skipped."""
        width = max(len(m) for m in self.outcomes)
        lines = [f"| {'module':<{width}} |  passed |  failed | skipped |",
                 f"|{'-' * (width + 2)}|---------|---------|---------|"]
        for module, counts in sorted(self.outcomes.items()):
            lines.append(f"| {module:<{width}} | {counts['passed']:>7} | {counts['failed']:>7} | {counts['skipped']:>7} |")
        lines.append("")
        for test, _ in self.failures + self.errors:
            lines.append(f"- failed: {test.id()}")
        for test, reason in self.skipped:
            lines.append(f"- skipped: {test.id()}: {reason}")
        return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", required=True)
    parser.add_argument("--kernel", required=True)
    image = parser.add_mutually_exclusive_group(required=True)
    image.add_argument("--initramfs", help="cpio.gz to boot from")
    image.add_argument("--rootfs", help="disk image to boot from instead")
    parser.add_argument("--append", default="", help="extra kernel command line, for things like gpio_mockup.gpio_mockup_ranges=-1,8")
    parser.add_argument("--init", default="", help="extra options for init, after the --, like smolinit.insmod=/lib/modules/e1000.ko.xz")
    parser.add_argument("--nic", help="network card model (e1000, virtio-net-pci) for a network")
    parser.add_argument("--summary", help="append a markdown table of the results to this file")
    parser.add_argument("-v", "--verbose", action="store_true")
    parser.add_argument("tests", nargs="*", help="test modules to run, like test_ls")
    args = parser.parse_args()

    os.environ["SMOL_QEMU"] = args.qemu
    os.environ["SMOL_KERNEL"] = args.kernel
    if args.initramfs:
        os.environ["SMOL_INITRAMFS"] = args.initramfs
    if args.rootfs:
        os.environ["SMOL_ROOTFS"] = args.rootfs
    os.environ["SMOL_APPEND"] = args.append
    os.environ["SMOL_INIT"] = args.init
    if args.nic:
        os.environ["SMOL_NIC"] = args.nic

    sys.path.insert(0, here)
    loader = unittest.TestLoader()
    if args.tests:
        suite = loader.loadTestsFromNames(args.tests)
    else:
        suite = loader.discover(here, pattern="test_*.py")

    result = unittest.TextTestRunner(verbosity=2 if args.verbose else 1, resultclass=Result).run(suite)
    if args.summary:
        with open(args.summary, "a") as f:
            f.write(result.summary())
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
