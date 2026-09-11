#!/usr/bin/env bash
# Fetch the QEMU source the virtio-gpu-rvk device is built into, pinned by hash:
# the 11.1.1 release tarball Arch's qemu 11.1.1-1 package builds. It lands in
# third_party/qemu/qemu-11.1.1 unmodified; tools/build_qemu.sh copies it before
# patching, so the pristine tree is reused by every build.
set -euo pipefail
cd "$(dirname "$0")/.."

VERSION=11.1.1
SHA256=079ffbff8a7111bbc89022107cbabf3bbfd614d5fc9d7cc675991196aca12482
URL="https://download.qemu.org/qemu-$VERSION.tar.xz"

CACHE=third_party/.cache
TREE=third_party/qemu/qemu-$VERSION

if [ -f "$TREE/VERSION" ] && [ "$(cat "$TREE/VERSION")" = "$VERSION" ]; then
    echo "qemu: $TREE already at $VERSION"
    exit 0
fi

mkdir -p "$CACHE"
archive="$CACHE/qemu-$VERSION.tar.xz"
if [ ! -f "$archive" ]; then
    echo "qemu: downloading $URL" >&2
    curl -sSfL -o "$archive.part" "$URL"
    mv "$archive.part" "$archive"
fi
got=$(sha256sum "$archive" | cut -d' ' -f1)
if [ "$got" != "$SHA256" ]; then
    rm -f "$archive"
    echo "qemu: qemu-$VERSION.tar.xz sha256 $got, want $SHA256" >&2
    exit 1
fi

mkdir -p third_party/qemu
staging=$(mktemp -d third_party/qemu/.unpack.XXXXXX)
trap 'rm -rf "$staging"' EXIT
tar -xJf "$archive" -C "$staging"
rm -rf "$TREE"
mv "$staging/qemu-$VERSION" "$TREE"
echo "qemu: $TREE"
