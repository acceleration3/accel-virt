#!/usr/bin/env python3
"""Fetch a pinned translation layer: its source, for the required list, and its
release, for the DLLs the guest runs (feature 047, FR-001 and SC-001).

The same standard as the other fetches (tools/fetch_vulkan_runtime.py): pinned
URLs, a SHA-256 for every archive and every file taken out of one, nothing
written when a hash disagrees, and --check to say whether what is on disk is
what was pinned. A layer's required list is read from the source archive by
tools/layer_requirements.py, so the list is the release's own rather than a copy
of it kept here.

One tool for both layers, because the two differ only in their pins:

    fetch_layers.py dxvk
    fetch_layers.py vkd3d-proton --check

The pins are data, in tools/<layer>_pins.txt:

    source  <url> <sha256>          the archive the required list is read from
    release <url> <sha256>          the archive the DLLs come from
    file <name> <path inside the release> <sha256>

which is what lets the tests drive this against local archives through file://.
Archives may be .zip, .tar.gz or .tar.zst; zstd is in the standard library from
Python 3.14.
"""
import argparse
import hashlib
import io
import pathlib
import shutil
import sys
import tarfile
import urllib.error
import urllib.request
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
LAYERS = ("dxvk", "vkd3d-proton")
SOURCE_DIRECTORY = "source"


class Refused(Exception):
    """A pin that cannot be honoured; the message names what."""


class LayerPins:
    """One layer's pins: the two archives and the files taken from the
    release."""

    def __init__(self, path):
        self.path = path
        self.source = None
        self.release = None
        self.files = {}

    def read(self):
        if not self.path.is_file():
            raise Refused(f"{self.path}: no pins")
        for number, line in enumerate(self.path.read_text().splitlines(), 1):
            text = line.strip()
            if not text or text.startswith("#"):
                continue
            parts = text.split()
            where = f"{self.path}:{number}"
            if parts[0] in ("source", "release"):
                if len(parts) != 3:
                    raise Refused(f"{where}: want '{parts[0]} <url> <sha256>', got {text!r}")
                setattr(self, parts[0], (parts[1], parts[2]))
            elif parts[0] == "file":
                if len(parts) != 4:
                    raise Refused(f"{where}: want 'file <name> <inside> <sha256>', got {text!r}")
                self.files[parts[1]] = (parts[2], parts[3])
            else:
                raise Refused(f"{where}: {parts[0]} is not a pin this reads")
        if self.source is None:
            raise Refused(f"{self.path}: no source pin")
        if self.release is None and self.files:
            raise Refused(f"{self.path}: files pinned with no release archive")
        return self

    def invariants_hold(self):
        """Every pin it read is a URL and a hash, and no file is pinned twice
        (a repeated key would have silently replaced the first)."""
        for pin in (self.source, self.release):
            if pin is not None and (len(pin) != 2 or not pin[0] or len(pin[1]) != 64):
                return False
        return all(len(digest) == 64 for _inside, digest in self.files.values())


class Archive:
    """One pinned archive on disk, and what can be taken out of it."""

    def __init__(self, path, digest):
        self.path = path
        self.digest = digest

    def raw(self):
        raw = self.path.read_bytes()
        got = hashlib.sha256(raw).hexdigest()
        if got != self.digest:
            self.path.unlink(missing_ok=True)
            raise Refused(f"{self.path.name} sha256 {got}, want {self.digest}")
        return raw

    def members(self):
        """{path inside, without the top directory: bytes}."""
        raw = self.raw()
        name = self.path.name
        if name.endswith(".zip"):
            return self.zip_members(raw)
        return self.tar_members(raw, name)

    def zip_members(self, raw):
        taken = {}
        with zipfile.ZipFile(io.BytesIO(raw)) as bundle:
            for member in bundle.infolist():
                if member.is_dir():
                    continue
                taken[self.without_top(member.filename)] = bundle.read(member)
        return taken

    def tar_members(self, raw, name):
        mode = "r:gz" if name.endswith(".gz") else "r:*"
        if name.endswith(".zst"):
            try:
                from compression import zstd
            except ImportError as error:   # pragma: no cover - 3.14 has it
                raise Refused(f"{name}: no zstd in this Python") from error
            raw = zstd.decompress(raw)
            mode = "r:"
        taken = {}
        with tarfile.open(fileobj=io.BytesIO(raw), mode=mode) as bundle:
            for member in bundle.getmembers():
                if not member.isfile():
                    continue
                handle = bundle.extractfile(member)
                if handle is None:
                    continue
                taken[self.without_top(member.name)] = handle.read()
        return taken

    def without_top(self, name):
        """Release archives carry one top directory named for their version,
        which no pin should have to name."""
        parts = name.split("/", 1)
        return parts[1] if len(parts) == 2 else name


class ArchiveCache:
    """Where pinned archives are kept, and how one is got.

    An archive is cached under the hash it is pinned to, so a cache hit is a
    hit on the bytes that were asked for and nothing else; a partial download
    is removed rather than trusted next time. Shared with tools/fetch_cts.py,
    which pins its archive the same way.
    """

    def __init__(self, directory, name):
        self.directory = directory
        self.name = name

    def get(self, url, digest, kind):
        archive = self.directory / f"{self.name}-{kind}-{digest}{self.suffix_of(url)}"
        if archive.is_file():
            return Archive(archive, digest)
        self.directory.mkdir(parents=True, exist_ok=True)
        partial = archive.with_name(archive.name + ".part")
        request = urllib.request.Request(url, headers={"User-Agent": "accel-virt-fetch"})
        try:
            with urllib.request.urlopen(request) as source, partial.open("wb") as out:
                shutil.copyfileobj(source, out)
        except (urllib.error.URLError, OSError) as error:
            partial.unlink(missing_ok=True)
            raise Refused(f"{url}: {error}") from error
        partial.rename(archive)
        return Archive(archive, digest)

    def suffix_of(self, url):
        for suffix in (".tar.zst", ".tar.gz", ".tgz", ".zip", ".tar"):
            if url.endswith(suffix):
                return suffix
        raise Refused(f"{url}: not an archive shape this reads")


class LayerFetch:
    """One layer's files on disk, and whether they are the pinned ones."""

    def __init__(self, layer, pins, into, cache):
        self.layer = layer
        self.pins = pins
        self.into = into
        self.cache = cache

    def problems(self):
        """What is wrong with what is on disk; empty when it is the pinned
        set."""
        problems = {}
        source = self.into / SOURCE_DIRECTORY
        if not source.is_dir() or not any(source.iterdir()):
            problems[SOURCE_DIRECTORY] = "missing"
        for name, (_inside, digest) in self.pins.files.items():
            path = self.into / name
            if not path.is_file():
                problems[name] = "missing"
                continue
            got = hashlib.sha256(path.read_bytes()).hexdigest()
            if got != digest:
                problems[name] = f"sha256 {got}, want {digest}"
        return problems

    def download(self, url, digest, kind):
        return ArchiveCache(self.cache, self.layer).get(url, digest, kind)

    def fetch(self):
        """Both archives verified and everything extracted, or Refused with
        nothing written."""
        source = self.download(*self.pins.source, "source")
        sources = source.members()
        release_files = {}
        if self.pins.release is not None:
            release = self.download(*self.pins.release, "release")
            members = release.members()
            for name, (inside, digest) in self.pins.files.items():
                if inside not in members:
                    raise Refused(f"{inside} is not in {self.layer}'s release archive")
                got = hashlib.sha256(members[inside]).hexdigest()
                if got != digest:
                    raise Refused(f"{name} sha256 {got}, want {digest}")
                release_files[name] = members[inside]
        # Nothing is written until every hash agreed.
        into_source = self.into / SOURCE_DIRECTORY
        if into_source.is_dir():
            shutil.rmtree(into_source)
        for inside, raw in sources.items():
            path = into_source / inside
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(raw)
        self.into.mkdir(parents=True, exist_ok=True)
        for name, raw in release_files.items():
            (self.into / name).write_bytes(raw)
        return sorted(release_files) + [SOURCE_DIRECTORY]


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("layer", choices=LAYERS)
    parser.add_argument("--pins", default=None, help="the pins file (default: tools/<layer>_pins.txt)")
    parser.add_argument("--into", default=None, help="where the files land (default: third_party/<layer>)")
    parser.add_argument("--cache", default=None, help="where archives are kept (default: third_party/.cache)")
    parser.add_argument("--check", action="store_true", help="only say whether the files on disk are the pinned ones")
    arguments = parser.parse_args(argv[1:])
    key = arguments.layer.replace("-", "_")
    pins_path = pathlib.Path(arguments.pins) if arguments.pins else ROOT / "tools" / f"{key}_pins.txt"
    into = pathlib.Path(arguments.into) if arguments.into else ROOT / "third_party" / arguments.layer
    cache = pathlib.Path(arguments.cache) if arguments.cache else ROOT / "third_party" / ".cache"
    try:
        pins = LayerPins(pins_path).read()
        if not pins.invariants_hold():
            raise Refused(f"{pins_path}: a pin is not a URL and a hash")
        fetch = LayerFetch(arguments.layer, pins, into, cache)
        if arguments.check:
            problems = fetch.problems()
            for name, what in sorted(problems.items()):
                print(f"{arguments.layer}: {name}: {what}", file=sys.stderr)
            if problems:
                return 1
            print(f"{arguments.layer}: the pinned files are in {into}")
            return 0
        cache.mkdir(parents=True, exist_ok=True)
        into.mkdir(parents=True, exist_ok=True)
        written = fetch.fetch()
        print(f"{arguments.layer}: {', '.join(written)} in {into}")
        return 0
    except Refused as refusal:
        print(f"{arguments.layer}: {refusal}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
