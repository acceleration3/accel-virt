#!/usr/bin/env python3
import pathlib

MAGIC = b"P6"
MAX_HEADER = 64
DIM = 3
DIFFERENT = (255, 0, 255)


class PpmImage:
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
    def __init__(self, captured, reference, tolerance):
        self.captured = PpmImage(captured)
        self.reference = PpmImage(reference)
        self.tolerance = tolerance

    def unreadable(self):
        missing = [image.path for image in (self.captured, self.reference) if not image.valid()]
        return f"{', '.join(str(path) for path in missing)} is not a P6 PPM" if missing else None

    def mismatched_size(self):
        if self.captured.size() != self.reference.size():
            return (f"the capture is {self.captured.width}x{self.captured.height} and the reference is "
                    f"{self.reference.width}x{self.reference.height}")
        return None

    def differs(self):
        found = 0
        capture, reference = self.captured.pixels, self.reference.pixels
        for at in range(0, len(capture), 3):
            for channel in range(3):
                if abs(capture[at + channel] - reference[at + channel]) > self.tolerance:
                    found += 1
                    break
        return found

    def write_difference(self, path):
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
