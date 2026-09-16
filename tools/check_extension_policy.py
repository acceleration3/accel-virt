#!/usr/bin/env python3
"""Gate for constitution VI on the ICDs (feature 045, research.md §9; feature
050): an ICD reports an extension only through a row of its policy table, and
every row names the test that proves the extension through the ICD - a
TEST(suite, case) that exists under tests/ - and an extension the generator
implemented for that API. No test, no row; no row, no report.

Each ICD is one directory src/icd/<api>/ holding source (src/icd/common/ is what
they share and holds no table) with exactly one `reported_extensions`: an array of
ExtensionPolicy::Row, or a null pointer while nothing is reported. Anything in
the array that is not a row this can read fails, so a row cannot hide from it.

  check_extension_policy.py [repository root]
"""
import os
import re
import sys

TABLE_DIRECTORY = "src/icd"
SHARED_DIRECTORY = "common"
# Where each API's generator lists what it implemented, and the array naming it.
IMPLEMENTED_LISTS = {
    "vulkan": ("src/proto/generated/vulkan/vk_supported_extensions.h", "kImplementedDeviceExtensions"),
}
TESTS_DIRECTORY = "tests"
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
ARRAY = re.compile(r"ExtensionPolicy::Row\s+reported_extensions\s*\[\s*\]\s*=\s*\{(?P<rows>.*?)\}\s*;", re.S)
EMPTY = re.compile(r"ExtensionPolicy::Row\s*\*\s*reported_extensions\s*=\s*nullptr\s*;")
ROW = re.compile(r'\s*\{\s*"(?P<name>[^"]*)"\s*,\s*(?P<version>\d+)u?\s*,\s*"(?P<test>[^"]*)"\s*\}\s*,?')
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

    def icds(self):
        """Each ICD's directory under src/icd, by API name: one holding source.
        Headers alone - hooks a generated tree calls before the ICD exists - are
        no ICD yet, and have nothing to report."""
        base = os.path.join(self.root, TABLE_DIRECTORY)
        if not os.path.isdir(base):
            return []
        return sorted(name for name in os.listdir(base)
                      if name != SHARED_DIRECTORY and os.path.isdir(os.path.join(base, name))
                      and any(True for _source in self.sources(os.path.join(TABLE_DIRECTORY, name), (".cpp",))))

    def tables(self, directory):
        """Every definition of a table in `directory`: (where, rows text or None for empty)."""
        found = []
        for relative, code in self.sources(directory, (".cpp", ".hpp")):
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

    def implemented(self, api):
        """The names the generator implemented for `api`, or None without a list."""
        if api not in IMPLEMENTED_LISTS:
            return None
        relative, array = IMPLEMENTED_LISTS[api]
        path = os.path.join(self.root, relative)
        if not os.path.isfile(path):
            return None
        with open(path) as header:
            match = re.search(re.escape(array) + r"\[\]\s*=\s*\{(?P<names>.*?)\};",
                              COMMENT.sub("", header.read()), re.S)
        return set(re.findall(r'"([^"]+)"', match.group("names"))) if match else set()

    def defined_tests(self):
        names = set()
        for _relative, code in self.sources(TESTS_DIRECTORY, (".cpp",)):
            names.update(f"{suite}.{case}" for suite, case in re.findall(r"\bTEST\(\s*(\w+)\s*,\s*(\w+)\s*\)", code))
        return names

    def violations(self):
        shared = self.tables(os.path.join(TABLE_DIRECTORY, SHARED_DIRECTORY))
        violations = [f"{where}: a reported_extensions table in {TABLE_DIRECTORY}/{SHARED_DIRECTORY} "
                      "belongs to no ICD" for where, _rows in shared]
        icds = self.icds()
        if not icds:
            return violations + [f"extension_policy: no ICD directory in {TABLE_DIRECTORY}"]
        tests = self.defined_tests()
        for api in icds:
            violations += self.icd_violations(api, tests)
        return violations

    def icd_violations(self, api, tests):
        directory = f"{TABLE_DIRECTORY}/{api}"
        tables = self.tables(directory)
        if len(tables) != 1:
            where = ", ".join(relative for relative, _rows in tables) or "nowhere"
            return [f"extension_policy: want one reported_extensions table in {directory}, found {len(tables)}: "
                    f"{where}"]
        where, text = tables[0]
        if text is None:
            return []
        rows, violations = self.rows(where, text)
        implemented = self.implemented(api)
        if implemented is None:
            if api not in IMPLEMENTED_LISTS:
                return violations + [f"extension_policy: no generated list is known for {api}"]
            return violations + [f"extension_policy: no {IMPLEMENTED_LISTS[api][0]}"]
        listed = IMPLEMENTED_LISTS[api][0]
        seen = set()
        for name, version, test in rows:
            if name in seen:
                violations.append(f"{where}: {name} is named twice")
            seen.add(name)
            if name not in implemented:
                violations.append(f"{where}: {name} is not in {listed}")
            if version == 0:
                violations.append(f"{where}: {name} has spec version 0")
            if not TEST_NAME.match(test):
                violations.append(f"{where}: {name} names '{test}', not a test as suite.case")
            elif test not in tests:
                violations.append(f"{where}: {name} names {test}, which no TEST under {TESTS_DIRECTORY}/ defines")
        return violations

    def reported(self):
        """(api, rows reported) for every ICD whose table can be read."""
        counts = []
        for api in self.icds():
            tables = self.tables(f"{TABLE_DIRECTORY}/{api}")
            rows = 0 if len(tables) != 1 or tables[0][1] is None else len(self.rows(*tables[0])[0])
            counts.append((api, rows))
        return counts

def main(argv=None):
    arguments = sys.argv[1:] if argv is None else argv
    check = PolicyCheck(arguments[0] if arguments else ".")
    violations = check.violations()
    for violation in violations:
        print(violation, file=sys.stderr)
    if violations:
        return 1
    counts = check.reported()
    detail = ", ".join(f"{api} {rows}" for api, rows in counts)
    print(f"extension_policy: ok, {sum(rows for _api, rows in counts)} reported ({detail})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
