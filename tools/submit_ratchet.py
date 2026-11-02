import re

RATIO_LINE = re.compile(r"^ratio\s+(\d+)$")
MAX_RATIO = 1000000


class SubmitRatchet:
    def __init__(self, path):
        self.path = path
        self.recorded_ = None

    @property
    def recorded(self):
        if self.recorded_ is None:
            self.recorded_ = self.read()
        return self.recorded_

    def read(self):
        try:
            text = self.path.read_text()
        except OSError:
            return 0
        found = 0
        for number, line in enumerate(text.splitlines(), 1):
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue
            match = RATIO_LINE.match(stripped)
            if match is None:
                raise ValueError(f"{self.path}:{number}: {stripped!r} is not a `ratio <hundredths>` line")
            ratio = int(match.group(1))
            if ratio == 0 or ratio > MAX_RATIO:
                raise ValueError(f"{self.path}:{number}: {ratio} is no ratio of two submission paths")
            if found:
                raise ValueError(f"{self.path}:{number}: a second ratio; the record holds one")
            found = ratio
        return found

    def lost(self, ratio):
        return max(self.recorded - ratio, 0)

    def gained(self, ratio):
        return max(ratio - self.recorded, 0)

    def line(self, ratio):
        return f"ratio {ratio}\n"
