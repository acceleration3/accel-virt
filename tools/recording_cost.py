"""What a wire recording costs a no-op round trip (SC-002, feature 065).

The criterion asks for the same round trip both ways, and no single boot can
give it: one run tells the device `record=<path>` and one tells it nothing, so
the number is the difference between two runs. Each av run writes its own side
into the run directory beside qemu.log, and whichever run completes the pair
says the difference.

The round trip kept is the warm median of the 31 the transport scenario times
after the first, not the first itself: a single cold round trip on this host
swings by milliseconds, which is orders of magnitude more than a `write` per
batch, and a difference of two of them would be a number nobody could believe.

The record is at most two lines, `off <microseconds>` and `on <microseconds>`.
A run replaces its own side rather than adding to it, so the pair is always the
two most recent boots.
"""
import re

SIDE_LINE = re.compile(r"^(off|on)\s+(\d+)$")
SIDES = ("off", "on")
# A no-op round trip past a second is not one: the av scenario fails its own
# run at 50 ms. A line past this is a typo, and a typo read as a measurement
# would be a cost nobody paid.
MAX_MICROSECONDS = 1000000
HEADER = ("# What a no-op round trip cost this host with a wire recording and without,\n"
          "# in microseconds (SC-002, feature 065). Written by run_guest_suite.py's av\n"
          "# scenario; a side is replaced by the next run that measures it.\n")


class RecordingCost:
    """The two no-op round trips SC-002 compares, and what a run does to them."""

    def __init__(self, path):
        self.path = path
        self.measured_ = None

    @property
    def measured(self):
        """The microseconds held for each side; a side never measured is absent."""
        if self.measured_ is None:
            self.measured_ = self.read()
        return self.measured_

    def read(self):
        try:
            text = self.path.read_text()
        except OSError:
            return {}
        held = {}
        for number, line in enumerate(text.splitlines(), 1):
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue
            match = SIDE_LINE.match(stripped)
            if match is None:
                raise ValueError(f"{self.path}:{number}: {stripped!r} is not an `off <microseconds>` or "
                                 f"`on <microseconds>` line")
            side, microseconds = match.group(1), int(match.group(2))
            if microseconds > MAX_MICROSECONDS:
                raise ValueError(f"{self.path}:{number}: {microseconds} us is no no-op round trip")
            if side in held:
                raise ValueError(f"{self.path}:{number}: a second `{side}`; the record holds one of each")
            held[side] = microseconds
        return held

    def keep(self, side, microseconds):
        """Write this run's side, leaving what the other run measured alone."""
        if side not in SIDES:
            raise ValueError(f"{self.path}: {side!r} is neither side of the comparison")
        held = dict(self.measured)
        held[side] = microseconds
        self.path.write_text(HEADER + "".join(f"{name} {held[name]}\n" for name in SIDES if name in held))
        self.measured_ = held

    def cost(self):
        """What the recording added to a round trip, or None while one of the
        two runs has not happened."""
        held = self.measured
        if len(held) != len(SIDES):
            return None
        return held["on"] - held["off"]
