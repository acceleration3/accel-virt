#!/usr/bin/env python3
"""Fetch the Vulkan loader and vulkaninfo for Windows x64, pinned by URL and hash."""
import argparse
import hashlib
import io
import pathlib
import shutil
import sys
import urllib.error
import urllib.request
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_PINS = ROOT / "tools" / "vulkan_runtime_pins.txt"
DEFAULT_INTO = ROOT / "third_party" / "vulkan-runtime"
ARCHIVE = "archive"
WANTED = ("vulkan-1.dll", "vulkaninfo.exe")


class Refused(Exception):
    pass


def pins_from(path):
    archive = None
    files = {}
    for number, line in enumerate(path.read_text().splitlines(), 1):
        text = line.strip()
        if not text or text.startswith("#"):
            continue
        parts = text.split()
        if len(parts) != 3:
            raise Refused(f"{path}:{number}: want '<name> <source> <sha256>', got {text!r}")
        name, source, digest = parts
        if name == ARCHIVE:
            archive = (source, digest)
        elif name in WANTED:
            files[name] = (source, digest)
        else:
            raise Refused(f"{path}:{number}: {name} is not a file this fetches")
    if archive is None:
        raise Refused(f"{path}: no archive pin")
    for name in WANTED:
        if name not in files:
            raise Refused(f"{path}: no pin for {name}")
    return archive[0], archive[1], files


def sha256_of(raw):
    return hashlib.sha256(raw).hexdigest()


def problems_on_disk(into, files):
    problems = {}
    for name, (_inside, digest) in files.items():
        path = into / name
        if not path.is_file():
            problems[name] = "missing"
            continue
        got = sha256_of(path.read_bytes())
        if got != digest:
            problems[name] = f"sha256 {got}, want {digest}"
    return problems


def download(url, digest, cache):
    archive = cache / f"vulkan-runtime-{digest}.zip"
    if archive.is_file():
        return archive
    partial = archive.with_suffix(".part")
    request = urllib.request.Request(url, headers={"User-Agent": "accel-virt-fetch"})
    try:
        with urllib.request.urlopen(request) as source, partial.open("wb") as out:
            shutil.copyfileobj(source, out)
    except (urllib.error.URLError, OSError) as error:
        partial.unlink(missing_ok=True)
        raise Refused(f"{url}: {error}") from error
    partial.rename(archive)
    return archive


def verified_files(archive, digest, files):
    raw = archive.read_bytes()
    got = sha256_of(raw)
    if got != digest:
        archive.unlink(missing_ok=True)
        raise Refused(f"archive sha256 {got}, want {digest}")
    try:
        with zipfile.ZipFile(io.BytesIO(raw)) as bundle:
            members = {}
            for member in bundle.infolist():
                parts = member.filename.split("/", 1)
                if len(parts) == 2:
                    members[parts[1]] = member
            extracted = {}
            for name, (inside, file_digest) in files.items():
                member = members.get(inside)
                if member is None:
                    raise Refused(f"the archive holds no {inside} for {name}")
                content = bundle.read(member)
                got = sha256_of(content)
                if got != file_digest:
                    raise Refused(f"{name}: sha256 {got}, want {file_digest}")
                extracted[name] = content
    except zipfile.BadZipFile as error:
        raise Refused(f"the archive is not a zip this can read: {error}") from error
    return extracted


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--pins", type=pathlib.Path, default=DEFAULT_PINS)
    parser.add_argument("--into", type=pathlib.Path, default=DEFAULT_INTO)
    parser.add_argument("--cache", type=pathlib.Path, default=ROOT / "third_party" / ".cache",
                        help="where the downloaded archive is kept between fetches")
    parser.add_argument("--check", action="store_true",
                        help="only say whether the pinned files are present; exit 1 if not")
    arguments = parser.parse_args(argv)

    try:
        url, digest, files = pins_from(arguments.pins)
    except Refused as refused:
        print(f"vulkan-runtime: {refused}", file=sys.stderr)
        return 1
    problems = problems_on_disk(arguments.into, files)
    if arguments.check:
        for name in files:
            verdict = problems.get(name, "ok")
            print(f"vulkan-runtime: {arguments.into / name}: {verdict}",
                  file=sys.stderr if name in problems else sys.stdout)
        return 1 if problems else 0
    if not problems:
        print(f"vulkan-runtime: {arguments.into} already holds every pinned file")
        return 0

    try:
        arguments.cache.mkdir(parents=True, exist_ok=True)
        extracted = verified_files(download(url, digest, arguments.cache), digest, files)
    except Refused as refused:
        print(f"vulkan-runtime: {refused}", file=sys.stderr)
        return 1
    arguments.into.mkdir(parents=True, exist_ok=True)
    for name, content in extracted.items():
        partial = arguments.into / f"{name}.part"
        partial.write_bytes(content)
        partial.rename(arguments.into / name)
        print(f"vulkan-runtime: {arguments.into / name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
