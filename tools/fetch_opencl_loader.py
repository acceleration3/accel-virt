#!/usr/bin/env python3
"""Fetch the Khronos OpenCL ICD loader's source, pinned by URL and hash.

The same standard as the Khronos registries' fetch: a tag, a SHA-256, and
nothing unpacked when they disagree. What lands in third_party/opencl-loader/ is
the subset of the loader that builds OpenCL.dll for a Windows guest - the
loader's own sources and the Windows half of its platform layer - which the
build then cross-compiles with clang-cl.

Source rather than a binary because Khronos publishes no binary: the releases of
KhronosGroup/OpenCL-ICD-Loader carry no assets, only the auto-generated source
archives (feature 052, research §2).

The pins are data, in tools/opencl_loader_pins.txt:

    <name> <url> <sha256>

which is what lets the tests drive this against a local archive through file://
rather than the network. --check says whether what is on disk is what was
pinned.
"""
import argparse
import hashlib
import pathlib
import sys
import tarfile
import urllib.error
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_PINS = ROOT / "tools" / "opencl_loader_pins.txt"
DEFAULT_INTO = ROOT / "third_party" / "opencl-loader"
LOADER = "loader"

# What the archive has to yield, and where each file goes in the fetched tree.
# The list is closed: an archive that does not hold what its name promises is
# refused here, rather than failing much later in a cross-build whose error
# names a missing header instead of a bad pin.
#
# cllayerinfo.c and loader/linux/ are left behind - one is a separate tool, the
# other the platform half this does not build.
WANTED = (
    "loader/icd.c",
    "loader/icd.h",
    "loader/icd_dispatch.c",
    "loader/icd_dispatch.h",
    "loader/icd_dispatch_generated.c",
    "loader/icd_envvars.h",
    "loader/icd_platform.h",
    "loader/icd_version.h",
    "loader/windows/adapter.h",
    "loader/windows/icd_windows.c",
    "loader/windows/icd_windows.h",
    "loader/windows/icd_windows_apppackage.c",
    "loader/windows/icd_windows_apppackage.h",
    "loader/windows/icd_windows_dxgk.c",
    "loader/windows/icd_windows_dxgk.h",
    "loader/windows/icd_windows_envvars.c",
    "loader/windows/icd_windows_hkr.c",
    "loader/windows/icd_windows_hkr.h",
    "loader/windows/OpenCL.def",
)

# The loader's CMake writes this from icd_cmake_config.h.in. Neither name is a
# Windows function, so on this target both are simply absent.
CMAKE_CONFIG = "loader/icd_cmake_config.h"
CMAKE_CONFIG_TEXT = ("/* Written by tools/fetch_opencl_loader.py, standing in for what the\n"
                     "   loader's own CMake configures. Neither getenv is Windows'. */\n"
                     "/* #undef HAVE_SECURE_GETENV */\n"
                     "/* #undef HAVE___SECURE_GETENV */\n")


class Refused(Exception):
    """A pin that cannot be honoured; the message names what."""


def pins_from(path):
    """(url, sha256) for the loader archive."""
    found = None
    for number, line in enumerate(path.read_text().splitlines(), 1):
        text = line.strip()
        if not text or text.startswith("#"):
            continue
        parts = text.split()
        if len(parts) != 3:
            raise Refused(f"{path}:{number}: want '<name> <url> <sha256>', got {text!r}")
        name, url, digest = parts
        if name != LOADER:
            raise Refused(f"{path}:{number}: {name} is not a thing this fetches")
        if found is not None:
            raise Refused(f"{path}:{number}: a second {LOADER} pin")
        found = (url, digest)
    if found is None:
        raise Refused(f"{path}: no {LOADER} pin")
    return found


def sha256_of(raw):
    return hashlib.sha256(raw).hexdigest()


def problems_on_disk(into, digest):
    """{file: what is wrong with it}, empty when the pinned tree is there.

    The archive's hash is what is pinned, so what is checked here is that every
    wanted file is present and that the tree was written from this pin - the
    marker carries the digest the tree came from."""
    problems = {}
    marker = into / "PINNED"
    if not marker.is_file():
        problems["PINNED"] = "missing"
    elif marker.read_text().strip() != digest:
        problems["PINNED"] = f"written from {marker.read_text().strip()}, want {digest}"
    for name in WANTED + (CMAKE_CONFIG,):
        if not (into / name).is_file():
            problems[name] = "missing"
    return problems


def download(url, digest, cache):
    """The archive, cached under the hash it is pinned to, with a partial
    download removed rather than left to be trusted next time."""
    archive = cache / f"opencl-loader-{digest}.tar.gz"
    if archive.is_file():
        return archive
    partial = archive.with_suffix(".part")
    request = urllib.request.Request(url, headers={"User-Agent": "accel-virt-fetch"})
    try:
        with urllib.request.urlopen(request) as source:
            partial.write_bytes(source.read())
    except (urllib.error.URLError, OSError) as error:
        partial.unlink(missing_ok=True)
        raise Refused(f"{url}: {error}") from error
    partial.rename(archive)
    return archive


def verified_files(archive, digest):
    """{file: bytes} for every wanted file, or Refused before anything is
    written. The archive is hashed first, so a tree is never built from bytes
    that are not the pinned ones."""
    raw = archive.read_bytes()
    got = sha256_of(raw)
    if got != digest:
        archive.unlink(missing_ok=True)
        raise Refused(f"archive sha256 {got}, want {digest}")
    try:
        with tarfile.open(archive, "r:gz") as bundle:
            members = {}
            for member in bundle.getmembers():
                parts = member.name.split("/", 1)
                if len(parts) == 2 and member.isfile():
                    members[parts[1]] = member
            extracted = {}
            for name in WANTED:
                member = members.get(name)
                if member is None:
                    raise Refused(f"the archive holds no {name}")
                source = bundle.extractfile(member)
                if source is None:
                    raise Refused(f"the archive's {name} has no contents")
                extracted[name] = source.read()
    except tarfile.TarError as error:
        raise Refused(f"the archive is not a tar this can read: {error}") from error
    return extracted


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--pins", type=pathlib.Path, default=DEFAULT_PINS)
    parser.add_argument("--into", type=pathlib.Path, default=DEFAULT_INTO)
    parser.add_argument("--cache", type=pathlib.Path, default=ROOT / "third_party" / ".cache",
                        help="where the downloaded archive is kept between fetches")
    parser.add_argument("--check", action="store_true",
                        help="only say whether the pinned tree is present; exit 1 if not")
    arguments = parser.parse_args(argv)

    try:
        url, digest = pins_from(arguments.pins)
    except Refused as refused:
        print(f"opencl-loader: {refused}", file=sys.stderr)
        return 1
    problems = problems_on_disk(arguments.into, digest)
    if arguments.check:
        if not problems:
            print(f"opencl-loader: {arguments.into}: ok")
            return 0
        for name in sorted(problems):
            print(f"opencl-loader: {arguments.into / name}: {problems[name]}", file=sys.stderr)
        return 1
    if not problems:
        print(f"opencl-loader: {arguments.into} already holds the pinned tree")
        return 0

    try:
        arguments.cache.mkdir(parents=True, exist_ok=True)
        extracted = verified_files(download(url, digest, arguments.cache), digest)
    except Refused as refused:
        print(f"opencl-loader: {refused}", file=sys.stderr)
        return 1
    for name, content in extracted.items():
        path = arguments.into / name
        path.parent.mkdir(parents=True, exist_ok=True)
        partial = path.with_name(path.name + ".part")
        partial.write_bytes(content)
        partial.rename(path)
    config = arguments.into / CMAKE_CONFIG
    config.write_text(CMAKE_CONFIG_TEXT)
    # Last, so a tree whose write was cut short has no marker and is refetched.
    (arguments.into / "PINNED").write_text(digest + "\n")
    print(f"opencl-loader: {arguments.into} ({len(extracted) + 1} files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
