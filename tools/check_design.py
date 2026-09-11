#!/usr/bin/env python3
"""Gate for constitution Principle III: every unit of behaviour is a class that
owns state. Reports static member functions and classes with no data members,
and any new or delete expression - objects are constructed in place only by
ObjectStorage and ObjectPool - in src/core, src/device, src/proto and src/guest.

  check_design.py [repository root]
"""
import os
import re
import sys

from header_scan import SOURCE_DIRECTORIES, declared_classes

IN_PLACE_CONSTRUCTORS = ("src/core/support/object_storage.hpp", "src/core/support/object_pool.hpp")
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
NEW_EXPRESSION = re.compile(r"(?<![\w.])new\s*[\(\w:]")


def sources(root):
    for directory in SOURCE_DIRECTORIES:
        for base, _dirs, files in sorted(os.walk(os.path.join(root, directory))):
            for name in sorted(files):
                if name.endswith((".cpp", ".hpp")):
                    path = os.path.join(base, name)
                    yield path, os.path.relpath(path, root)


def allocation_expressions(root):
    """Comments are blanked line-preservingly so a line number still points at the code."""
    found = []
    for path, relative in sources(root):
        with open(path) as source:
            code = COMMENT.sub(lambda m: "\n" * m.group(0).count("\n"), source.read())
        for number, line in enumerate(code.split("\n"), 1):
            if NEW_EXPRESSION.search(line) and relative not in IN_PLACE_CONSTRUCTORS:
                found.append(f"{relative}:{number}: new expression outside {' and '.join(IN_PLACE_CONSTRUCTORS)}")
            if re.search(r"\bdelete\b", line) and not re.search(r"=\s*delete\b", line):
                found.append(f"{relative}:{number}: delete expression")
    return found


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else "."
    violations = []
    for declared in declared_classes(root):
        for method in declared.static_methods:
            violations.append(f"{declared.header}: {declared.name}::{method} is static")
        if declared.is_record and declared.method_count:
            violations.append(f"{declared.header}: record {declared.name} declares member functions")
        if not declared.has_state and not declared.is_contract():
            violations.append(f"{declared.header}: class {declared.name} has no data members")
    violations += allocation_expressions(root)
    for violation in violations:
        print(violation, file=sys.stderr)
    if violations:
        sys.exit(1)
    print("design_rules: ok")


if __name__ == "__main__":
    main()
