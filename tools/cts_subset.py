#!/usr/bin/env python3
import fnmatch
import re

PATTERN_LINE = re.compile(r"^(?P<pattern>\S+)\s+for:\s*(?P<extensions>.+)$")
CORE = "core"


class CtsSubset:
    def __init__(self, path):
        self.path = path
        self.groups_ = None

    @property
    def groups(self):
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
        return sorted({name for _pattern, names in self.groups for name in names if name != CORE})

    def matching(self, names):
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
        return [pattern for pattern, extensions in self.groups if not extensions]
