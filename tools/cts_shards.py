#!/usr/bin/env python3
"""Which cases each guest run is given (feature 048, US1).

A full mustpass run is hours; the runner's steps are bounded, and a step that
runs past its timeout fails the run rather than reporting it. So the subset is
cut into shards that fit one step, and each shard's log is pulled before the
next starts - which also means a guest that bugchecks loses one shard's tail
rather than the run.
"""
import re

CASE_NAME = re.compile(r"^[A-Za-z0-9_.-]+$")
MAX_NAME = 512


class CtsShards:
    """One subset file, cut into the lists one guest step runs."""

    def __init__(self, path, per_shard):
        if per_shard < 1:
            raise ValueError(f"a shard of {per_shard} case(s) runs nothing")
        self.path = path
        self.per_shard = per_shard
        self.cases_ = None

    @property
    def cases(self):
        if self.cases_ is None:
            self.cases_ = self.read()
        return self.cases_

    def read(self):
        """The subset, in the order it was written. Unlike a `.qpa` - which
        arrives from a guest - this file is committed here, so a line that is
        not a case name is a mistake to refuse rather than data to distrust."""
        try:
            text = self.path.read_text()
        except OSError as missing:
            raise ValueError(f"{self.path}: {missing}") from missing
        found = []
        for number, line in enumerate(text.splitlines(), 1):
            name = line.strip()
            if not name or name.startswith("#"):
                continue
            if len(name) > MAX_NAME or CASE_NAME.match(name) is None:
                raise ValueError(f"{self.path}:{number}: {name!r} is not a case name")
            found.append(name)
        return found

    def shards(self):
        cases = self.cases
        return [cases[at:at + self.per_shard] for at in range(0, len(cases), self.per_shard)]

    def after(self, shard, name):
        """What is left of `shard` past the case that died: a resumed run never
        runs the case that killed it again, and never starts the shard over."""
        if name is None or name not in shard:
            return []
        return shard[shard.index(name) + 1:]
