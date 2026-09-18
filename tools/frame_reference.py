#!/usr/bin/env python3
"""Whether a frame is the one this repository committed (feature 048, US2).

The frame comes from the device's scanout - a QMP screendump, not a guest-side
screenshot - because a guest screenshot goes through the guest's own
composition, which is the thing under test (FR-004). What is compared is every
channel of every pixel, within the step a GPU is allowed to round by, and a
mismatch keeps three files: the capture, the reference and a difference image,
which is what makes a failure readable a week later.
"""
import pathlib

MAGIC = b"P6"
MAX_HEADER = 64
# What a matching pixel is dimmed to in a difference image, so the shape of the
# frame stays visible behind what changed.
DIM = 3
DIFFERENT = (255, 0, 255)


class PpmImage:
    """One binary PPM: its size and its pixels.

    QEMU's screendump writes P6 with a maxval of 255, and so do the references
    beside this repository's specs. Anything else answers a size of (0, 0),
    which every caller treats as "not an image" rather than raising: these
    files come from a QEMU that may have died halfway through writing one.
    """

    def __init__(self, path):
        self.path = pathlib.Path(path)
        self.width = 0
        self.height = 0
        self.pixels = b""
        self.read()

    def read(self):
        try:
            raw = self.path.read_bytes()
        except OSError:
            return
        fields = raw[:MAX_HEADER].split(maxsplit=4)
        if len(fields) < 4 or fields[0] != MAGIC:
            return
        try:
            width, height, maxval = int(fields[1]), int(fields[2]), int(fields[3])
        except ValueError:
            return
        if width <= 0 or height <= 0 or maxval != 255:
            return
        # The pixels start after the fourth field's own whitespace, which the
        # split above consumed exactly once.
        header = raw.index(fields[3], len(MAGIC)) + len(fields[3]) + 1
        pixels = raw[header:]
        if len(pixels) < width * height * 3:
            return
        self.width, self.height = width, height
        self.pixels = pixels[:width * height * 3]

    def valid(self):
        return self.width > 0 and self.height > 0

    def size(self):
        return (self.width, self.height)


class FrameReference:
    """A captured frame against a committed one."""

    def __init__(self, captured, reference, tolerance):
        self.captured = PpmImage(captured)
        self.reference = PpmImage(reference)
        self.tolerance = tolerance

    def unreadable(self):
        """Which of the two is not an image, as a reason a caller prints."""
        missing = [image.path for image in (self.captured, self.reference) if not image.valid()]
        return f"{', '.join(str(path) for path in missing)} is not a P6 PPM" if missing else None

    def mismatched_size(self):
        if self.captured.size() != self.reference.size():
            return (f"the capture is {self.captured.width}x{self.captured.height} and the reference is "
                    f"{self.reference.width}x{self.reference.height}")
        return None

    def differs(self):
        """How many pixels are outside the tolerance on any channel."""
        found = 0
        capture, reference = self.captured.pixels, self.reference.pixels
        for at in range(0, len(capture), 3):
            for channel in range(3):
                if abs(capture[at + channel] - reference[at + channel]) > self.tolerance:
                    found += 1
                    break
        return found

    def write_difference(self, path):
        """The frame with every differing pixel picked out, so what moved is
        visible rather than counted."""
        capture, reference = self.captured.pixels, self.reference.pixels
        out = bytearray(len(capture))
        for at in range(0, len(capture), 3):
            different = any(abs(capture[at + channel] - reference[at + channel]) > self.tolerance
                            for channel in range(3))
            for channel in range(3):
                out[at + channel] = (DIFFERENT[channel] if different
                                     else capture[at + channel] // DIM)
        pathlib.Path(path).write_bytes(
            f"P6\n{self.captured.width} {self.captured.height}\n255\n".encode() + bytes(out))
        return path
