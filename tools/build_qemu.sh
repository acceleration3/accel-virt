#!/usr/bin/env bash
# Build qemu-system-x86_64 with the virtio-gpu-rvk modules linked against the
# device core. The distro binary cannot load them: its compiled-in module table
# has no entry for the new device types, so QEMU itself is rebuilt.
#
#   build_qemu.sh <build directory> <directory holding rvk_device.pc>
#
# The pinned source (tools/fetch_qemu.sh) is copied and patched once per patch
# revision; the glue in src/device/qemu is re-synced on every run, so ninja
# rebuilds only what changed. Run QEMU from <build directory>/qemu with
# QEMU_MODULE_DIR set to that directory.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
[ $# -eq 2 ] || { echo "usage: build_qemu.sh <build directory> <rvk_device.pc directory>" >&2; exit 2; }
build=$(realpath -m "$1")
pkgconfig=$(realpath "$2")
glue="$root/src/device/qemu"

"$root/tools/fetch_qemu.sh" >/dev/null
pristine="$root/third_party/qemu/qemu-11.1.1"
source="$build/source"
stamp=$(sha256sum "$glue/qemu-build.patch" | cut -d' ' -f1)

if [ ! -f "$source/.rvk-patch" ] || [ "$(cat "$source/.rvk-patch")" != "$stamp" ]; then
    rm -rf "$source" "$build/qemu"
    mkdir -p "$build"
    cp -a "$pristine" "$source"
    patch -p1 -s -d "$source" < "$glue/qemu-build.patch"
    echo "$stamp" > "$source/.rvk-patch"
fi
install -C -m 644 "$glue/virtio-gpu-rvk.h" "$source/include/hw/virtio/virtio-gpu-rvk.h"
for file in virtio-gpu-rvk.c virtio-gpu-rvk-host.c virtio-gpu-rvk-renderer.c virtio-gpu-pci-rvk.c; do
    install -C -m 644 "$glue/$file" "$source/hw/display/$file"
done

if [ ! -f "$build/qemu/build.ninja" ]; then
    mkdir -p "$build/qemu"
    (cd "$build/qemu" && PKG_CONFIG_PATH="$pkgconfig" "$source/configure" \
        --target-list=x86_64-softmmu --enable-modules --enable-kvm --disable-docs --disable-user \
        --disable-tools --disable-rust --disable-debug-info -Drvk=enabled >configure.log) \
        || { tail -30 "$build/qemu/configure.log" >&2; exit 1; }
fi
PKG_CONFIG_PATH="$pkgconfig" ninja -C "$build/qemu" >"$build/qemu/ninja.log" \
    || { grep -E "error|FAILED" -A5 "$build/qemu/ninja.log" | head -60 >&2; exit 1; }
echo "qemu: $build/qemu/qemu-system-x86_64"
