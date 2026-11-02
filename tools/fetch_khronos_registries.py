#!/usr/bin/env python3
"""Fetch the Khronos registries the generators read, pinned by revision and hash."""
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

WANTED = {
    "vulkan": (("registry/vk.xml", "vk.xml"),
               ("include/vulkan/vulkan_core.h", "include/vulkan/vulkan_core.h"),
               ("include/vulkan/vk_platform.h", "include/vulkan/vk_platform.h"))
              + tuple((f"include/vk_video/{name}", f"include/vk_video/{name}") for name in (
                  "vulkan_video_codec_av1std.h", "vulkan_video_codec_av1std_decode.h",
                  "vulkan_video_codec_av1std_encode.h", "vulkan_video_codec_h264std.h",
                  "vulkan_video_codec_h264std_decode.h", "vulkan_video_codec_h264std_encode.h",
                  "vulkan_video_codec_h265std.h", "vulkan_video_codec_h265std_decode.h",
                  "vulkan_video_codec_h265std_encode.h", "vulkan_video_codecs_common.h")),
    "opengl": (("xml/gl.xml", "gl.xml"),
               ("xml/wgl.xml", "wgl.xml")),
    "opencl": (("xml/cl.xml", "cl.xml"),),
    "opencl_headers": tuple((f"CL/{name}", f"include/CL/{name}") for name in (
        "cl.h", "cl_d3d10.h", "cl_d3d11.h", "cl_dx9_media_sharing.h", "cl_egl.h",
        "cl_ext.h", "cl_function_types.h", "cl_gl.h", "cl_gl_ext.h", "cl_half.h",
        "cl_icd.h", "cl_layer.h", "cl_platform.h", "cl_version.h")),
}


class Refused(Exception):
    pass


def pins_from(path):
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
    marker = into / "VERSION"
    return marker.is_file() and marker.read_text() == marker_text(pins)


def marker_text(pins):
    return "".join(f"{name} {digest}\n" + "".join(f"  {destination}\n"
                                                  for _inside, destination in WANTED[name])
                   for name, _url, digest in sorted(pins))


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
