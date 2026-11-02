#!/usr/bin/env python3
"""Fetch the Khronos OpenCL ICD loader's source, pinned by URL and hash."""
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

CMAKE_CONFIG = "loader/icd_cmake_config.h"
CMAKE_CONFIG_TEXT = ("/* Written by tools/fetch_opencl_loader.py, standing in for what the\n"
                     "   loader's own CMake configures. Neither getenv is Windows'. */\n"
                     "/* #undef HAVE_SECURE_GETENV */\n"
                     "/* #undef HAVE___SECURE_GETENV */\n")


class Refused(Exception):
    pass


def pins_from(path):
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
    (arguments.into / "PINNED").write_text(digest + "\n")
    print(f"opencl-loader: {arguments.into} ({len(extracted) + 1} files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
