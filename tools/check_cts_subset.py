#!/usr/bin/env python3
import os
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from check_layer_requirements import DEVICE_ROWS, INSTANCE_ROWS, NAME  # noqa: E402
from cts_subset import CtsSubset  # noqa: E402

POLICY_TABLE = os.path.join("src", "icd", "vulkan", "icd_root.cpp")
SUBSET = os.path.join("tools", "cts_subset.txt")

class SubsetCheck:
    def __init__(self, root):
        self.root = pathlib.Path(root)

    def reported(self):
        text = (self.root / POLICY_TABLE).read_text()
        found = set()
        for pattern in (DEVICE_ROWS, INSTANCE_ROWS):
            table = pattern.search(text)
            if table is not None:
                found.update(NAME.findall(table.group("rows")))
        return found

    def violations(self):
        reported = self.reported()
        if not reported:
            return [f"no policy table in {POLICY_TABLE}"], []
        subset = CtsSubset(self.root / SUBSET)
        try:
            covered = set(subset.extensions())
        except ValueError as refused:
            return [str(refused)], []
        missing = sorted(name for name in reported if name not in covered)
        stale = sorted(name for name in covered if name not in reported)
        return ([f"{name} is reported by the ICD and no group in {SUBSET} exercises it" for name in missing] +
                [f"{name} has a group in {SUBSET} and the ICD does not report it" for name in stale],
                sorted(reported))


def main(argv=None):
    arguments = sys.argv[1:] if argv is None else argv
    check = SubsetCheck(arguments[0] if arguments else pathlib.Path(__file__).resolve().parent.parent)
    violations, reported = check.violations()
    for violation in violations:
        print(f"cts_subset: {violation}", file=sys.stderr)
    if violations:
        return 1
    print(f"cts_subset: ok, {len(reported)} reported extension(s) covered")
    return 0


if __name__ == "__main__":
    sys.exit(main())
