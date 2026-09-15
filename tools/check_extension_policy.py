#!/usr/bin/env python3
"""Gate for constitution VI on the Vulkan ICD (feature 045, research.md §9): the
ICD reports a device extension only through a row of its policy table, and
every row names the test that proves the extension through the ICD - a
TEST(suite, case) that exists under tests/ - and an extension the generator
implemented, listed in vk_supported_extensions.h. No test, no row; no row, no
report.

The table is the one `reported_extensions` in src/icd: an array of
ExtensionPolicy::Row, or a null pointer while nothing is reported. Anything in
the array that is not a row this can read fails, so a row cannot hide from it.

  check_extension_policy.py [repository root]
"""
import os
import re
import sys

TABLE_DIRECTORY = "src/icd"
SUPPORTED = "src/proto/generated/vulkan/vk_supported_extensions.h"
TESTS_DIRECTORY = "tests"
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
ARRAY = re.compile(r"ExtensionPolicy::Row\s+reported_extensions\s*\[\s*\]\s*=\s*\{(?P<rows>.*?)\}\s*;", re.S)
EMPTY = re.compile(r"ExtensionPolicy::Row\s*\*\s*reported_extensions\s*=\s*nullptr\s*;")
ROW = re.compile(r'\s*\{\s*"(?P<name>[^"]*)"\s*,\s*(?P<version>\d+)u?\s*,\s*"(?P<test>[^"]*)"\s*\}\s*,?')
IMPLEMENTED = re.compile(r"kImplementedDeviceExtensions\[\]\s*=\s*\{(?P<names>.*?)\};", re.S)
TEST_NAME = re.compile(r"^(\w+)\.(\w+)$")


class PolicyCheck:
    """Judges the policy table of one repository tree."""

    def __init__(self, root):
        self.root = root

    def sources(self, directory, suffixes):
        for base, _dirs, files in sorted(os.walk(os.path.join(self.root, directory))):
            for name in sorted(files):
                if name.endswith(suffixes):
                    path = os.path.join(base, name)
                    with open(path) as source:
                        yield os.path.relpath(path, self.root), COMMENT.sub("", source.read())

    def tables(self):
        """Every definition of the table: (where, rows text or None for empty)."""
        found = []
        for relative, code in self.sources(TABLE_DIRECTORY, (".cpp", ".hpp")):
            found += [(relative, match.group("rows")) for match in ARRAY.finditer(code)]
            found += [(relative, None) for _match in EMPTY.finditer(code)]
        return found

    def rows(self, where, text):
        """The rows of a table's text, and a violation for anything unreadable."""
        rows, violations, at = [], [], 0
        while text[at:].strip():
            match = ROW.match(text, at)
            if not match:
                violations.append(f"{where}: unreadable row: {text[at:].strip()[:60]}")
                break
            rows.append((match.group("name"), int(match.group("version")), match.group("test")))
            at = match.end()
        return rows, violations

    def implemented(self):
        path = os.path.join(self.root, SUPPORTED)
        if not os.path.isfile(path):
            return None
        with open(path) as header:
            match = IMPLEMENTED.search(COMMENT.sub("", header.read()))
        return set(re.findall(r'"([^"]+)"', match.group("names"))) if match else set()

    def defined_tests(self):
        names = set()
        for _relative, code in self.sources(TESTS_DIRECTORY, (".cpp",)):
            names.update(f"{suite}.{case}" for suite, case in re.findall(r"\bTEST\(\s*(\w+)\s*,\s*(\w+)\s*\)", code))
        return names

    def violations(self):
        tables = self.tables()
        if len(tables) != 1:
            where = ", ".join(relative for relative, _rows in tables) or "nowhere"
            return [f"extension_policy: want one reported_extensions table in {TABLE_DIRECTORY}, found {len(tables)}: "
                    f"{where}"]
        where, text = tables[0]
        if text is None:
            return []
        rows, violations = self.rows(where, text)
        implemented = self.implemented()
        if implemented is None:
            return violations + [f"extension_policy: no {SUPPORTED}"]
        tests = self.defined_tests()
        seen = set()
        for name, version, test in rows:
            if name in seen:
                violations.append(f"{where}: {name} is named twice")
            seen.add(name)
            if name not in implemented:
                violations.append(f"{where}: {name} is not in {SUPPORTED}")
            if version == 0:
                violations.append(f"{where}: {name} has spec version 0")
            if not TEST_NAME.match(test):
                violations.append(f"{where}: {name} names '{test}', not a test as suite.case")
            elif test not in tests:
                violations.append(f"{where}: {name} names {test}, which no TEST under {TESTS_DIRECTORY}/ defines")
        return violations

    def reported(self):
        tables = self.tables()
        return 0 if len(tables) != 1 or tables[0][1] is None else len(self.rows(*tables[0])[0])


def main(argv=None):
    arguments = sys.argv[1:] if argv is None else argv
    check = PolicyCheck(arguments[0] if arguments else ".")
    violations = check.violations()
    for violation in violations:
        print(violation, file=sys.stderr)
    if violations:
        return 1
    print(f"extension_policy: ok, {check.reported()} reported")
    return 0


if __name__ == "__main__":
    sys.exit(main())
