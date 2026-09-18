#!/usr/bin/env python3
"""What a CTS run did, and what two runs say about each other (feature 048).

The Vulkan CTS is dEQP, and dEQP writes one `.qpa` log per run: a stream of
records, each opened by a `#`-prefixed line. Three of them decide everything
here - a case beginning, a case's `<Result StatusCode=...>`, and a case that was
terminated without one. A run killed by a crash, a TDR or a guest bugcheck
leaves its log open at the case that killed it, which is exactly the case a
resumed run must start *after*, so the log answers that too.

The log arrives from a guest over an agent, so it is read as bytes somebody
else wrote: a name no case could have is dropped rather than recorded, and a
status nobody defines is kept as written and is never silently a pass.
"""
import re

BEGIN_CASE = "#beginTestCaseResult"
END_CASE = "#endTestCaseResult"
TERMINATE_CASE = "#terminateTestCaseResult"
END_SESSION = "#endSession"
RESULT = re.compile(r'<Result\s+StatusCode="([^"]*)"')
CASE_NAME = re.compile(r"^[A-Za-z0-9_.-]+$")
MAX_NAME = 512
# What dEQP itself counts as not-a-failure. NotSupported is deliberately absent:
# it is neither a pass nor a failure, and it matches only another NotSupported.
PASSING = ("Pass", "QualityWarning", "CompatibilityWarning", "Waiver")
NOT_SUPPORTED = "NotSupported"
# What a case that ended without a result, or was terminated without a reason,
# is recorded as: the run did not finish it.
UNFINISHED = "Crash"


class QpaResults:
    """One `.qpa` log's cases, and where its run stopped."""

    def __init__(self, path):
        self.path = path
        self.cases = {}
        self.unfinished = None
        self.session_ended = False
        self.read()

    def read(self):
        """Every record the log holds. A file that is not a log, or one that
        stops mid-record, answers what it held rather than raising: a truncated
        log is the normal shape of a run that died."""
        try:
            text = self.path.read_text(errors="replace")
        except OSError:
            return
        open_case = None
        status = None
        for line in text.splitlines():
            if line.startswith(BEGIN_CASE):
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
            # The case the run died in: recorded, and named so a resumed run
            # knows where to start again.
            self.unfinished = open_case
            self.record(open_case, UNFINISHED)

    def named(self, text):
        """The case name a record opened with, or None for one no case could
        have."""
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
    """A guest run and a host run of the same subset, and what one says the
    other did not.

    The verdict of feature 048 is here: a case that passes on the host and does
    not in the guest is this driver's failure, and one that fails on both is
    not.
    """

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
                # A case one side never ran is not a comparison; a subset that
                # ran on one side only is what that usually means.
                self.unjudged.append(name)
                continue
            if self.host.not_supported(name) and not self.guest.not_supported(name):
                # The host would not run it and the guest did: the two runs
                # were not the same subset, whatever the guest answered.
                self.unjudged.append(name)
                continue
            self.compared += 1
            if self.guest.not_supported(name) and self.host.not_supported(name):
                # Neither side has the extension the case needs - a match, and
                # nothing this driver did.
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
    """What the guest passed, committed: a record that may only grow.

    It needs no host run, which is the point - a change that loses a case fails
    on the day it is made, whether or not anyone ran the host side. An empty
    record passes, because no guest has run the CTS yet and a record that said
    otherwise would be a claim nobody made.
    """

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
        """Every recorded case the run does not pass - including one it never
        ran, because a subset that stopped running a case proves nothing about
        it."""
        return [name for name in self.names if not results.passed(name)]

    def gained(self, results):
        recorded = set(self.names)
        return [name for name in results.passing() if name not in recorded]

    def rewritten(self, results):
        """The record a gaining run commits: what it passed, and what was
        already there."""
        return sorted(set(self.names) | set(results.passing()))
