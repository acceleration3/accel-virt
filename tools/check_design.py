#!/usr/bin/env python3
"""Gate for constitution Principle III: every unit of behaviour is a class that
owns state. Reports static member functions and classes with no data members
in src/core, src/device and src/proto.

  check_design.py [repository root]
"""
import sys

from header_scan import declared_classes


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "."
    violations = []
    for declared in declared_classes(root):
        for method in declared.static_methods:
            violations.append(f"{declared.header}: {declared.name}::{method} is static")
        if not declared.has_state and not declared.is_contract():
            violations.append(f"{declared.header}: class {declared.name} has no data members")
    for violation in violations:
        print(violation, file=sys.stderr)
    if violations:
        sys.exit(1)
    print("design_rules: ok")


if __name__ == "__main__":
    main()
