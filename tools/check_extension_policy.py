#!/usr/bin/env python3
import os
import re
import sys

TABLE_DIRECTORY = "src/icd"
SHARED_DIRECTORY = "common"
NO_EXTENSIONS = ("d3d", "vendor")
IMPLEMENTED_LISTS = {
    "vulkan": ("src/proto/generated/vulkan/vk_supported_extensions.h", "kImplementedDeviceExtensions"),
    "opengl": ("src/proto/generated/opengl/gl_tier.h", "kCarriedExtensions"),
    "opencl": ("src/proto/generated/opencl/cl_supported_extensions.h", "kCarriedExtensions"),
}
WINDOW_SYSTEM_API = "opengl"
WGL_ARRAY = re.compile(r"ExtensionPolicy::Row\s+reported_wgl_extensions\s*\[\s*\]\s*=\s*\{(?P<rows>.*?)\}\s*;", re.S)
WGL_NAME = re.compile(r"^WGL_\w+$")
TESTS_DIRECTORY = "tests"
EXECUTOR_SHIM_DIRECTORY = "src/executor/shim"
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
ARRAY = re.compile(r"ExtensionPolicy::Row\s+reported_extensions\s*\[\s*\]\s*=\s*\{(?P<rows>.*?)\}\s*;", re.S)
EMPTY = re.compile(r"ExtensionPolicy::Row\s*\*\s*reported_extensions\s*=\s*nullptr\s*;")
ROW = re.compile(r'\s*\{\s*"(?P<name>[^"]*)"\s*,\s*(?P<version>\d+)u?\s*,\s*"(?P<test>[^"]*)"'
                 r'(\s*,\s*"(?P<host>[^"]*)")?\s*\}\s*,?')
TEST_NAME = re.compile(r"^(\w+)\.(\w+)$")
ANSWERED_HERE = "-"


class PolicyCheck:
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
        base = os.path.join(self.root, TABLE_DIRECTORY)
        if not os.path.isdir(base):
            return []
        return sorted(name for name in os.listdir(base)
                      if name != SHARED_DIRECTORY and os.path.isdir(os.path.join(base, name))
                      and any(True for _source in self.sources(os.path.join(TABLE_DIRECTORY, name), (".cpp",))))

    def wgl_tables(self, directory):
        return [(relative, match.group("rows")) for relative, code in self.sources(directory, (".cpp", ".hpp"))
                for match in WGL_ARRAY.finditer(code)]

    def tables(self, directory):
        found = []
        for relative, code in self.sources(directory, (".cpp", ".hpp")):
            found += [(relative, match.group("rows")) for match in ARRAY.finditer(code)]
            found += [(relative, None) for _match in EMPTY.finditer(code)]
        return found

    def rows(self, where, text):
        rows, violations, at = [], [], 0
        while text[at:].strip():
            match = ROW.match(text, at)
            if not match:
                violations.append(f"{where}: unreadable row: {text[at:].strip()[:60]}")
                break
            rows.append((match.group("name"), int(match.group("version")), match.group("test"),
                         match.group("host")))
            at = match.end()
        return rows, violations

    def implemented(self, api):
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
            violations += self.wgl_violations(api, tests)
        return violations

    def wgl_violations(self, api, tests):
        directory = f"{TABLE_DIRECTORY}/{api}"
        tables = self.wgl_tables(directory)
        if api != WINDOW_SYSTEM_API:
            return [f"{where}: a reported_wgl_extensions table outside {TABLE_DIRECTORY}/{WINDOW_SYSTEM_API}"
                    for where, _rows in tables]
        if len(tables) > 1:
            return [f"extension_policy: want at most one reported_wgl_extensions table in {directory}, "
                    f"found {len(tables)}"]
        violations = []
        for where, text in tables:
            rows, unreadable = self.rows(where, text)
            violations += unreadable + self.row_violations(where, rows, tests, None)
        return violations

    def device_uses(self, extension):
        shim = os.path.join(self.root, EXECUTOR_SHIM_DIRECTORY)
        if not os.path.isdir(shim):
            return False
        for base, _directories, names in os.walk(shim):
            for name in names:
                if not name.endswith((".cpp", ".hpp")):
                    continue
                with open(os.path.join(base, name)) as source:
                    if extension in source.read():
                        return True
        return False

    def row_violations(self, where, rows, tests, implemented):
        violations = []
        seen = set()
        for name, version, test, host in rows:
            if name in seen:
                violations.append(f"{where}: {name} is named twice")
            seen.add(name)
            if implemented is None and not WGL_NAME.match(name):
                violations.append(f"{where}: {name} is not a WGL extension")
            elif implemented is not None and name not in implemented[1]:
                violations.append(f"{where}: {name} is not in {implemented[0]}")
            if (host is not None and not host.startswith(ANSWERED_HERE) and implemented is not None
                    and host not in implemented[1] and not self.device_uses(host)):
                violations.append(f"{where}: {name} is answered over {host}, which is in neither "
                                  f"{implemented[0]} nor {EXECUTOR_SHIM_DIRECTORY}")
            if version == 0:
                violations.append(f"{where}: {name} has spec version 0")
            if not TEST_NAME.match(test):
                violations.append(f"{where}: {name} names '{test}', not a test as suite.case")
            elif test not in tests:
                violations.append(f"{where}: {name} names {test}, which no TEST under {TESTS_DIRECTORY}/ defines")
        return violations

    def icd_violations(self, api, tests):
        if api in NO_EXTENSIONS:
            return []
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
        return violations + self.row_violations(where, rows, tests, (IMPLEMENTED_LISTS[api][0], implemented))

    def reported(self):
        counts = []
        for api in self.icds():
            tables = self.tables(f"{TABLE_DIRECTORY}/{api}")
            rows = 0 if len(tables) != 1 or tables[0][1] is None else len(self.rows(*tables[0])[0])
            wgl = self.wgl_tables(f"{TABLE_DIRECTORY}/{api}")
            counts.append((api, rows, len(self.rows(*wgl[0])[0]) if wgl else 0))
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
    detail = ", ".join(f"{api} {rows}" + (f" + {wgl} wgl" if wgl else "") for api, rows, wgl in counts)
    print(f"extension_policy: ok, {sum(rows + wgl for _api, rows, wgl in counts)} reported ({detail})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
