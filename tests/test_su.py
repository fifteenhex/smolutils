# SPDX-License-Identifier: GPL-3.0-or-later
from base import EX_USAGE, SmolTest, Tool


class Su(Tool):
    name = "su"


class TestSu(SmolTest):
    # becoming root for real is done once by the base for self.root, so only the argument checks here
    def test_arguments(self):
        Su().rejects("bogus", status=EX_USAGE)
        Su().rejects("root", "extra", status=EX_USAGE)
