import re

SIDE_LINE = re.compile(r"^(off|on)\s+(\d+)$")
SIDES = ("off", "on")
MAX_MICROSECONDS = 1000000
HEADER = ("# What a no-op round trip cost this host with a wire recording and without,\n"
          "# in microseconds (SC-002, feature 065). Written by run_guest_suite.py's av\n"
          "# scenario; a side is replaced by the next run that measures it.\n")


class RecordingCost:
    def __init__(self, path):
        self.path = path
        self.measured_ = None

    @property
    def measured(self):
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
        if side not in SIDES:
            raise ValueError(f"{self.path}: {side!r} is neither side of the comparison")
        held = dict(self.measured)
        held[side] = microseconds
        self.path.write_text(HEADER + "".join(f"{name} {held[name]}\n" for name in SIDES if name in held))
        self.measured_ = held

    def cost(self):
        held = self.measured
        if len(held) != len(SIDES):
            return None
        return held["on"] - held["off"]
