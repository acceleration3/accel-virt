#!/usr/bin/env python3
import re

CASE_NAME = re.compile(r"^[A-Za-z0-9_.-]+$")
MAX_NAME = 512


class CtsShards:
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
        if name is None or name not in shard:
            return []
        return shard[shard.index(name) + 1:]
