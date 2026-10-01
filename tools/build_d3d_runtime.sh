#!/usr/bin/env bash
# Builds the host's Direct3D - DXVK (Direct3D 11, DXGI) and vkd3d-proton
# (Direct3D 12) - natively for Linux from the submodules in third_party/, and
# installs the shared objects into one directory, build-d3d/lib, which is what
# the executor library loads them from (AV_D3D_PATH; src/device/shim/
# d3d_runtime.hpp).
#
#   tools/build_d3d_runtime.sh            # configure if needed, build, install
#   AV_D3D_PATH=$PWD/build-d3d/lib ...   # what the device and the tests read
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

# A submodule with its series of patches on top of the pinned commit. The
# series is judged whole - the files it touches, as they are, against the pinned
# commit with every patch applied, both built in a scratch index - since one
# patch reverse-checked alone fails once a later one edits beside it. Files
# already as the series makes them are left alone (a rebuild stays
# incremental); otherwise they are put back as pinned and the series applied.
apply_series() {
    local dir=$1 patches=("${@:2}") scratch files expected current
    scratch=$(mktemp -d)
    files=$(for patch in "${patches[@]}"; do git -C "$dir" apply --numstat "$patch" | cut -f3; done | sort -u)
    GIT_INDEX_FILE="$scratch/expected" git -C "$dir" read-tree HEAD
    for patch in "${patches[@]}"; do GIT_INDEX_FILE="$scratch/expected" git -C "$dir" apply --cached "$patch"; done
    expected=$(GIT_INDEX_FILE="$scratch/expected" git -C "$dir" write-tree)
    GIT_INDEX_FILE="$scratch/current" git -C "$dir" read-tree HEAD
    # shellcheck disable=SC2086
    GIT_INDEX_FILE="$scratch/current" git -C "$dir" add -- $files
    current=$(GIT_INDEX_FILE="$scratch/current" git -C "$dir" write-tree)
    rm -rf "$scratch"
    [ "$expected" = "$current" ] && return
    # shellcheck disable=SC2086
    git -C "$dir" checkout HEAD -- $files
    for patch in "${patches[@]}"; do git -C "$dir" apply "$patch"; done
}

# The pinned DXVK with what this project needs of it that upstream does not
# enable: tools/dxvk-patches.
apply_series "$ROOT/third_party/dxvk" "$ROOT"/tools/dxvk-patches/*.patch

# The pinned vkd3d-proton with the host-pointer heap the executor imports guest
# memory through, and shared textures and fences as fds: tools/vkd3d-proton-patches.
apply_series "$ROOT/third_party/vkd3d-proton" "$ROOT"/tools/vkd3d-proton-patches/*.patch

# Direct3D 8, 9 and 10 stay out: the guest's user-mode driver serves 11 and 12.
build dxvk "$ROOT/third_party/dxvk" -Denable_d3d8=false -Denable_d3d9=false -Denable_d3d10=false
build vkd3d-proton "$ROOT/third_party/vkd3d-proton"
ls -1 "$PREFIX/lib"/*.so
