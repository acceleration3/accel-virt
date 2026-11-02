#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
[ $# -eq 2 ] || { echo "usage: build_qemu.sh <build directory> <av_device.pc directory>" >&2; exit 2; }
build=$(realpath -m "$1")
pkgconfig=$(realpath "$2")
glue="$root/src/device/qemu"

"$root/tools/fetch_qemu.sh" >/dev/null
pristine="$root/third_party/qemu/qemu-11.1.1"
source="$build/source"
stamp=$(sha256sum "$glue/qemu-build.patch" | cut -d' ' -f1)

if [ ! -f "$source/.av-patch" ] || [ "$(cat "$source/.av-patch")" != "$stamp" ]; then
    rm -rf "$source" "$build/qemu"
    mkdir -p "$build"
    cp -a "$pristine" "$source"
    patch -p1 -s -d "$source" < "$glue/qemu-build.patch"
    echo "$stamp" > "$source/.av-patch"
fi
install -C -m 644 "$glue/virtio-gpu-av.h" "$source/include/hw/virtio/virtio-gpu-av.h"
for file in virtio-gpu-av.c virtio-gpu-av-host.c virtio-gpu-av-renderer.c virtio-gpu-pci-av.c; do
    install -C -m 644 "$glue/$file" "$source/hw/display/$file"
done

if [ ! -f "$build/qemu/build.ninja" ]; then
    mkdir -p "$build/qemu"
    (cd "$build/qemu" && PKG_CONFIG_PATH="$pkgconfig" "$source/configure" \
        --target-list=x86_64-softmmu --enable-modules --enable-kvm --disable-docs --disable-user \
        --disable-tools --disable-rust --disable-debug-info -Dav=enabled >configure.log) \
        || { tail -30 "$build/qemu/configure.log" >&2; exit 1; }
fi
PKG_CONFIG_PATH="$pkgconfig" ninja -C "$build/qemu" >"$build/qemu/ninja.log" \
    || { grep -E "error|FAILED" -A5 "$build/qemu/ninja.log" | head -60 >&2; exit 1; }
echo "qemu: $build/qemu/qemu-system-x86_64"
