#!/usr/bin/env python3
"""Fetch the Khronos registries the generators read, pinned by revision and hash.

The same standard as QEMU's and the Windows kit's fetch: a revision, a SHA-256,
and nothing unpacked when they disagree. What lands in third_party/khronos/ is
the XML the generators parse and the headers their output compiles against.

The pins are data, in tools/khronos_pins.txt, one registry per line:

    <name> <url> <sha256>

which is what lets the tests drive this against local archives through file://
rather than the network. A registry that moved under a regeneration is a wire
change, so every pin names a tag or a revision - never a branch.
"""
import argparse
import hashlib
import pathlib
import shutil
import sys
import tarfile
import tempfile
import urllib.error
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_PINS = ROOT / "tools" / "khronos_pins.txt"
DEFAULT_INTO = ROOT / "third_party" / "khronos"

# What each archive has to yield, and where it goes in the fetched tree. A
# registry whose archive does not hold what its name promises is refused: the
# generators would otherwise fail much later with something less obvious.
WANTED = {
    "vulkan": (("registry/vk.xml", "vk.xml"),
               ("include/vulkan/vulkan_core.h", "include/vulkan/vulkan_core.h"),
               # vulkan_core.h includes it; a header that cannot be compiled is
               # not a header the generated output can be checked against.
               ("include/vulkan/vk_platform.h", "include/vulkan/vk_platform.h")),
    "opengl": (("xml/gl.xml", "gl.xml"),
               ("xml/wgl.xml", "wgl.xml")),
    "opencl": (("xml/cl.xml", "cl.xml"),),
    # cl.xml describes commands over types only the headers declare, and the
    # OpenCL registry and the OpenCL headers are two repositories.
    "opencl_headers": tuple((f"CL/{name}", f"include/CL/{name}") for name in (
        "cl.h", "cl_d3d10.h", "cl_d3d11.h", "cl_dx9_media_sharing.h", "cl_egl.h",
        "cl_ext.h", "cl_function_types.h", "cl_gl.h", "cl_gl_ext.h", "cl_half.h",
        "cl_icd.h", "cl_layer.h", "cl_platform.h", "cl_version.h")),
}


class Refused(Exception):
    """A pin that cannot be honoured; the message names the registry."""


def pins_from(path):
    """Every pin in `path`: (name, url, sha256), comments and blanks dropped."""
    pins = []
    for number, line in enumerate(path.read_text().splitlines(), 1):
        text = line.strip()
        if not text or text.startswith("#"):
            continue
        parts = text.split()
        if len(parts) != 3:
            raise Refused(f"{path}:{number}: want '<name> <url> <sha256>', got {text!r}")
        name, url, digest = parts
        if name not in WANTED:
            raise Refused(f"{path}:{number}: {name} is not a registry this reads")
        pins.append((name, url, digest))
    return pins


def download(name, url, digest, cache):
    """The archive, cached under the hash it is pinned to - a pin that moves
    must not find the archive it replaced - with a partial download removed
    rather than left to be trusted next time."""
    archive = cache / f"{name}-{digest}.tar.gz"
    if archive.is_file():
        return archive
    partial = archive.with_suffix(".part")
    try:
        with urllib.request.urlopen(url) as source, partial.open("wb") as out:
            shutil.copyfileobj(source, out)
    except (urllib.error.URLError, OSError) as error:
        partial.unlink(missing_ok=True)
        raise Refused(f"{name}: {url}: {error}") from error
    partial.rename(archive)
    return archive


def verify(name, archive, digest):
    got = hashlib.sha256(archive.read_bytes()).hexdigest()
    if got != digest:
        archive.unlink(missing_ok=True)
        raise Refused(f"{name}: sha256 {got}, want {digest}")


def unpack(name, archive, into):
    """The files WANTED names, out of the archive's single top directory."""
    with tarfile.open(archive, "r:gz") as tar:
        members = {}
        for member in tar.getmembers():
            if not member.isfile():
                continue
            parts = member.name.split("/", 1)
            if len(parts) == 2:
                members[parts[1]] = member
        for inside, destination in WANTED[name]:
            member = members.get(inside)
            if member is None:
                raise Refused(f"{name}: the archive holds no {inside}")
            source = tar.extractfile(member)
            if source is None:
                raise Refused(f"{name}: {inside} is not a file")
            target = into / destination
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(source.read())


def fetched_already(into, pins):
    """Whether the tree holds exactly these pins; the marker is what says so."""
    marker = into / "VERSION"
    return marker.is_file() and marker.read_text() == marker_text(pins)


def marker_text(pins):
    return "".join(f"{name} {digest}\n" for name, _url, digest in sorted(pins))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--pins", type=pathlib.Path, default=DEFAULT_PINS)
    parser.add_argument("--into", type=pathlib.Path, default=DEFAULT_INTO)
    parser.add_argument("--cache", type=pathlib.Path, default=ROOT / "third_party" / ".cache",
                        help="where downloaded archives are kept between fetches")
    arguments = parser.parse_args(argv)

    try:
        pins = pins_from(arguments.pins)
    except Refused as refused:
        print(f"khronos: {refused}", file=sys.stderr)
        return 1
    if fetched_already(arguments.into, pins):
        print(f"khronos: {arguments.into} already holds every pinned registry")
        return 0

    cache = arguments.cache
    cache.mkdir(parents=True, exist_ok=True)
    # Into a staging tree, so a registry that fails leaves whatever was verified
    # before exactly as it was.
    staging = pathlib.Path(tempfile.mkdtemp(prefix=".khronos.", dir=str(arguments.into.parent)
                                            if arguments.into.parent.is_dir() else None))
    try:
        for name, url, digest in pins:
            archive = download(name, url, digest, cache)
            verify(name, archive, digest)
            unpack(name, archive, staging)
        (staging / "VERSION").write_text(marker_text(pins))
        if arguments.into.exists():
            shutil.rmtree(arguments.into)
        arguments.into.parent.mkdir(parents=True, exist_ok=True)
        staging.rename(arguments.into)
    except Refused as refused:
        print(f"khronos: {refused}", file=sys.stderr)
        return 1
    finally:
        shutil.rmtree(staging, ignore_errors=True)

    for name, _url, _digest in pins:
        for _inside, destination in WANTED[name]:
            print(f"khronos: {arguments.into / destination}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
