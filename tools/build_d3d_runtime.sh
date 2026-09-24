#!/usr/bin/env bash
# Builds the host's Direct3D - DXVK (Direct3D 11, DXGI) and vkd3d-proton
# (Direct3D 12) - natively for Linux from the submodules in third_party/, and
# installs the shared objects into one directory, build-d3d/lib, which is what
# the executor library loads them from (RVK_D3D_PATH; src/device/shim/
# d3d_runtime.hpp).
#
#   tools/build_d3d_runtime.sh            # configure if needed, build, install
#   RVK_D3D_PATH=$PWD/build-d3d/lib ...   # what the device and the tests read
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
PREFIX="$ROOT/build-d3d"
JOBS=${JOBS:-$(nproc)}

command -v meson >/dev/null || { echo "meson not found (uv tool install meson)" >&2; exit 1; }
git -C "$ROOT" submodule update --init --recursive third_party/dxvk third_party/vkd3d-proton

build() {
    local name=$1 source=$2
    shift 2
    local dir="$PREFIX/$name"
    if [ ! -f "$dir/build.ninja" ]; then
        meson setup "$dir" "$source" --buildtype=release --prefix="$PREFIX" --libdir=lib "$@"
    fi
    ninja -C "$dir" -j"$JOBS"
    meson install -C "$dir" --quiet
}

# The pinned DXVK with what this project needs of it that upstream does not
# enable: tools/dxvk-patches, applied once (a patch already in the tree is kept).
for patch in "$ROOT"/tools/dxvk-patches/*.patch; do
    git -C "$ROOT/third_party/dxvk" apply --reverse --check "$patch" 2>/dev/null ||
        git -C "$ROOT/third_party/dxvk" apply "$patch"
done

# The pinned vkd3d-proton with the host-pointer heap the executor imports guest
# memory through: tools/vkd3d-proton-patches, applied the same way.
for patch in "$ROOT"/tools/vkd3d-proton-patches/*.patch; do
    git -C "$ROOT/third_party/vkd3d-proton" apply --reverse --check "$patch" 2>/dev/null ||
        git -C "$ROOT/third_party/vkd3d-proton" apply "$patch"
done

# Direct3D 8, 9 and 10 stay out: the guest's user-mode driver serves 11 and 12.
build dxvk "$ROOT/third_party/dxvk" -Denable_d3d8=false -Denable_d3d9=false -Denable_d3d10=false
build vkd3d-proton "$ROOT/third_party/vkd3d-proton"
ls -1 "$PREFIX/lib"/*.so
