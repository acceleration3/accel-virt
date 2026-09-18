#!/usr/bin/env python3
"""Fetch the Vulkan CTS build this repository runs in the guest, pinned by URL
and hash (feature 048, FR-001).

The standard the layers' fetch set: one pinned archive, a SHA-256 for it and
one for every file taken out of it, and nothing written until all of them
agree - so what lands in third_party/vulkan-cts/ is either the pinned set or
what was there before. --check says whether what is on disk is the pinned set,
which is what the guest runner asks before it boots anything.

The pins are data, in tools/cts_pins.txt:

    archive <url> <sha256>
    file <name> <path inside the archive's top directory> <sha256>

and that file is **empty on purpose**: the archive has never been downloaded
here, and a hash written from memory is indistinguishable from a tampered one.
"""
import argparse
import hashlib
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from fetch_layers import Archive, ArchiveCache, Refused  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_PINS = ROOT / "tools" / "cts_pins.txt"
DEFAULT_INTO = ROOT / "third_party" / "vulkan-cts"
DEFAULT_CACHE = ROOT / "third_party" / ".cache"
ARCHIVE = "archive"
FILE = "file"


class CtsPins:
    """The archive the CTS comes from, and the files taken out of it."""

    def __init__(self, path):
        self.path = path
        self.archive = None
        self.files = {}

    def read(self):
        if not self.path.is_file():
            raise Refused(f"{self.path}: no pins")
        for number, line in enumerate(self.path.read_text().splitlines(), 1):
            text = line.strip()
            if not text or text.startswith("#"):
                continue
            parts = text.split()
            if parts[0] == ARCHIVE and len(parts) == 3:
                self.archive = (parts[1], parts[2])
            elif parts[0] == FILE and len(parts) == 4:
                self.files[parts[1]] = (parts[2], parts[3])
            else:
                raise Refused(f"{self.path}:{number}: want 'archive <url> <sha256>' or "
                              f"'file <name> <inside> <sha256>', got {text!r}")
        if self.archive is None:
            raise Refused(f"{self.path}: no archive pin")
        if not self.files:
            raise Refused(f"{self.path}: the archive is pinned and no file is taken out of it")
        return self

    def invariants_hold(self):
        """Every pin is a URL and a 64-character hash, and no file is pinned
        twice under different names."""
        if self.archive is None:
            return False
        url, digest = self.archive
        if "://" not in url or len(digest) != 64:
            return False
        inside = set()
        for _name, (path, file_digest) in self.files.items():
            if not path or len(file_digest) != 64 or path in inside:
                return False
            inside.add(path)
        return True


class CtsFetch:
    """The CTS on disk, and whether it is the pinned build."""

    def __init__(self, pins, into, cache):
        self.pins = pins
        self.into = into
        self.cache = cache

    def problems(self):
        """What is wrong with what is on disk; empty when it is the pinned
        set."""
        problems = {}
        for name, (_inside, digest) in self.pins.files.items():
            path = self.into / name
            if not path.is_file():
                problems[name] = "missing"
                continue
            got = hashlib.sha256(path.read_bytes()).hexdigest()
            if got != digest:
                problems[name] = f"sha256 {got}, want {digest}"
        return problems

    def fetch(self):
        """Everything verified and written, or Refused with nothing written."""
        archive = ArchiveCache(self.cache, "vulkan-cts").get(*self.pins.archive, ARCHIVE)
        members = archive.members()
        taken = {}
        for name, (inside, digest) in self.pins.files.items():
            if inside not in members:
                raise Refused(f"{inside} is not in the CTS archive")
            got = hashlib.sha256(members[inside]).hexdigest()
            if got != digest:
                raise Refused(f"{name} sha256 {got}, want {digest}")
            taken[name] = members[inside]
        self.into.mkdir(parents=True, exist_ok=True)
        for name, raw in taken.items():
            path = self.into / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(raw)
            # The runner starts deqp-vk in the guest; on this side it is only
            # ever hashed, but a build that cannot be run is not a build.
            path.chmod(0o755)
        return sorted(taken)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--pins", type=pathlib.Path, default=DEFAULT_PINS)
    parser.add_argument("--into", type=pathlib.Path, default=DEFAULT_INTO)
    parser.add_argument("--cache", type=pathlib.Path, default=DEFAULT_CACHE)
    parser.add_argument("--check", action="store_true",
                        help="only say whether what is on disk is the pinned set")
    arguments = parser.parse_args(argv)
    try:
        pins = CtsPins(arguments.pins).read()
        if not pins.invariants_hold():
            raise Refused(f"{arguments.pins}: a pin is not a URL and a hash")
        fetch = CtsFetch(pins, arguments.into, arguments.cache)
        if arguments.check:
            problems = fetch.problems()
            for name, what in sorted(problems.items()):
                print(f"vulkan-cts: {name}: {what}", file=sys.stderr)
            if problems:
                return 1
            print(f"vulkan-cts: ok, {len(pins.files)} pinned file(s) in {arguments.into}")
            return 0
        written = fetch.fetch()
        print(f"vulkan-cts: {len(written)} file(s) in {arguments.into}: {', '.join(written)}")
        return 0
    except Refused as refused:
        print(f"vulkan-cts: {refused}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
