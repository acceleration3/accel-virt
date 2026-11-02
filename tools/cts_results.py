#!/usr/bin/env python3
import re

BEGIN_CASE = "#beginTestCaseResult"
END_CASE = "#endTestCaseResult"
TERMINATE_CASE = "#terminateTestCaseResult"
END_SESSION = "#endSession"
RESULT = re.compile(r'<Result\s+StatusCode="([^"]*)"')
CASE_NAME = re.compile(r"^[A-Za-z0-9_.-]+$")
MAX_NAME = 512
PASSING = ("Pass", "QualityWarning", "CompatibilityWarning", "Waiver")
NOT_SUPPORTED = "NotSupported"
UNFINISHED = "Crash"


class QpaResults:
    def __init__(self, path):
        self.path = path
        self.cases = {}
        self.unfinished = None
        self.session_ended = False
        self.read()

    def read(self):
        try:
            text = self.path.read_text(errors="replace")
        except OSError:
            return
        open_case = None
        status = None
        for line in text.splitlines():
            if line.startswith(BEGIN_CASE):
                self.record(open_case, UNFINISHED)
                open_case, status = self.named(line[len(BEGIN_CASE):]), None
                continue
            if line.startswith(TERMINATE_CASE):
                reason = line[len(TERMINATE_CASE):].strip()
                self.record(open_case, reason or UNFINISHED)
                open_case, status = None, None
                continue
            if line.startswith(END_CASE):
                self.record(open_case, status if status is not None else UNFINISHED)
                open_case, status = None, None
                continue
            if line.startswith(END_SESSION):
                self.session_ended = True
                continue
            found = RESULT.search(line)
            if found is not None and open_case is not None:
                status = found.group(1)
        if open_case is not None:
            self.unfinished = open_case
            self.record(open_case, UNFINISHED)

    def named(self, text):
        name = text.strip()
        if not name or len(name) > MAX_NAME or CASE_NAME.match(name) is None:
            return None
        return name

    def record(self, name, status):
        if name is not None:
            self.cases[name] = status

    def passed(self, name):
        return self.cases.get(name) in PASSING

    def not_supported(self, name):
        return self.cases.get(name) == NOT_SUPPORTED

    def passing(self):
        return sorted(name for name in self.cases if self.passed(name))


class SuiteComparison:
    def __init__(self, guest, host):
        self.guest = guest
        self.host = host
        self.guest_only_failures = []
        self.matching_failures = []
        self.guest_only_passes = []
        self.unjudged = []
        self.compared = 0
        self.judge()

    def judge(self):
        for name in sorted(set(self.guest.cases) | set(self.host.cases)):
            in_guest = name in self.guest.cases
            in_host = name in self.host.cases
            if not in_guest or not in_host:
                self.unjudged.append(name)
                continue
            if self.host.not_supported(name) and not self.guest.not_supported(name):
                self.unjudged.append(name)
                continue
            self.compared += 1
            if self.guest.not_supported(name) and self.host.not_supported(name):
                continue
            guest_passed = self.guest.passed(name)
            host_passed = self.host.passed(name)
            if host_passed and not guest_passed:
                self.guest_only_failures.append(name)
            elif guest_passed and not host_passed:
                self.guest_only_passes.append(name)
            elif not guest_passed and not host_passed:
                self.matching_failures.append(name)


class CtsRatchet:
    def __init__(self, path):
        self.path = path
        self.names_ = None

    @property
    def names(self):
        if self.names_ is None:
            self.names_ = self.read()
        return self.names_

    def read(self):
        try:
            text = self.path.read_text()
        except OSError:
            return []
        found = set()
        for number, line in enumerate(text.splitlines(), 1):
            name = line.strip()
            if not name or name.startswith("#"):
                continue
            if len(name) > MAX_NAME or CASE_NAME.match(name) is None:
                raise ValueError(f"{self.path}:{number}: {name!r} is not a case name")
            found.add(name)
        return sorted(found)

    def lost(self, results):
        return [name for name in self.names if not results.passed(name)]

    def gained(self, results):
        recorded = set(self.names)
        return [name for name in results.passing() if name not in recorded]

    def rewritten(self, results):
        return sorted(set(self.names) | set(results.passing()))
