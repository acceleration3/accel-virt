"""What user-mode submission bought, committed (feature 049, US3).

The in-guest benchmark answers both per-submission costs and their ratio on one
boot; only the ratio is worth keeping. The nanoseconds are a property of the
host the run happened on - a busier machine makes both paths slower - and the
ratio is a property of the driver, which is what a change can break.

The record is one line, `ratio <hundredths>`, and a run whose ratio falls below
it fails. Empty, it passes and says so: no guest has measured the two paths
yet, and a record with a number in it would be a claim nobody made.
"""
import re

RATIO_LINE = re.compile(r"^ratio\s+(\d+)$")
# Ten thousand times is past anything a submission path can be; a line past it
# is a typo, not a measurement.
MAX_RATIO = 1000000


class SubmitRatchet:
    """The committed ratio, and what a run's ratio does to it."""

    def __init__(self, path):
        self.path = path
        self.recorded_ = None

    @property
    def recorded(self):
        """The ratio held, in hundredths, or None when nothing is recorded."""
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
        """How far the run falls below what is recorded, 0 when it does not."""
        return max(self.recorded - ratio, 0)

    def gained(self, ratio):
        """How far it rises above it; the whole ratio when nothing is
        recorded, which is a gain over having measured nothing."""
        return max(ratio - self.recorded, 0)

    def line(self, ratio):
        """The line that records `ratio`."""
        return f"ratio {ratio}\n"
