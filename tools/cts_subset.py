#!/usr/bin/env python3
"""Which CTS cases this repository runs, and which extension each group is
there for (feature 048, US1 and FR-005).

The file holds patterns rather than case names: one dEQP group is thousands of
cases and the list changes with every release, so the runner asks the CTS for
its own case list and keeps what these match. The `for:` tag on each line is
what holds the subset to the ICD: every extension a policy row reports has to
be exercised by a group here, and tools/check_cts_subset.py fails the build
naming one that is not.
"""
import fnmatch
import re

PATTERN_LINE = re.compile(r"^(?P<pattern>\S+)\s+for:\s*(?P<extensions>.+)$")
CORE = "core"


class CtsSubset:
    """One subset file: its patterns, and what each is for."""

    def __init__(self, path):
        self.path = path
        self.groups_ = None

    @property
    def groups(self):
        """(pattern, [extension, ...]) in file order."""
        if self.groups_ is None:
            self.groups_ = self.read()
        return self.groups_

    def read(self):
        try:
            text = self.path.read_text()
        except OSError as missing:
            raise ValueError(f"{self.path}: {missing}") from missing
        found = []
        for number, line in enumerate(text.splitlines(), 1):
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue
            matched = PATTERN_LINE.match(stripped)
            if matched is None:
                raise ValueError(f"{self.path}:{number}: want '<pattern> for: <extension>[, ...]'")
            extensions = [name.strip() for name in matched.group("extensions").split(",")]
            for name in extensions:
                if name != CORE and not name.startswith("VK_"):
                    raise ValueError(f"{self.path}:{number}: {name!r} is neither an extension nor '{CORE}'")
            found.append((matched.group("pattern"), extensions))
        return found

    def extensions(self):
        """Every extension some group is for."""
        return sorted({name for _pattern, names in self.groups for name in names if name != CORE})

    def matching(self, names):
        """The cases of `names` some pattern keeps, in the order given and each
        once: what one run of this subset is."""
        kept = []
        seen = set()
        for name in names:
            if name in seen:
                continue
            if any(fnmatch.fnmatchcase(name, pattern) for pattern, _extensions in self.groups):
                kept.append(name)
                seen.add(name)
        return kept

    def unmatched(self):
        """Patterns that kept nothing is a question only a fetched CTS can
        answer; what this answers is which extensions have no group at all."""
        return [pattern for pattern, extensions in self.groups if not extensions]
