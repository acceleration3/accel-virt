#!/usr/bin/env bash
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
    local copies
    mapfile -t copies < <(ninja -C "$dir" -t targets all | sed -n 's|^\(include/[A-Za-z0-9_]*\.h\): CUSTOM_COMMAND$|\1|p')
    if [ ${#copies[@]} -ne 0 ]; then ninja -C "$dir" -j"$JOBS" "${copies[@]}"; fi
    ninja -C "$dir" -j"$JOBS"
    meson install -C "$dir" --quiet
}

apply_series() {
    local dir=$1 patches=("${@:2}") scratch files present expected current
    scratch=$(mktemp -d)
    files=$(for patch in "${patches[@]}"; do git -C "$dir" apply --numstat "$patch" | cut -f3; done | sort -u)
    GIT_INDEX_FILE="$scratch/expected" git -C "$dir" read-tree HEAD
    for patch in "${patches[@]}"; do GIT_INDEX_FILE="$scratch/expected" git -C "$dir" apply --cached "$patch"; done
    expected=$(GIT_INDEX_FILE="$scratch/expected" git -C "$dir" write-tree)
    GIT_INDEX_FILE="$scratch/current" git -C "$dir" read-tree HEAD
    present=$(for file in $files; do
        if [ -e "$dir/$file" ] || git -C "$dir" cat-file -e "HEAD:$file" 2>/dev/null; then echo "$file"; fi
    done)
    # shellcheck disable=SC2086
    GIT_INDEX_FILE="$scratch/current" git -C "$dir" add -- $present
    current=$(GIT_INDEX_FILE="$scratch/current" git -C "$dir" write-tree)
    rm -rf "$scratch"
    [ "$expected" = "$current" ] && return
    for file in $files; do
        if git -C "$dir" cat-file -e "HEAD:$file" 2>/dev/null; then
            git -C "$dir" checkout HEAD -- "$file"
        else
            rm -f "$dir/$file"
        fi
    done
    for patch in "${patches[@]}"; do git -C "$dir" apply "$patch"; done
}

apply_series "$ROOT/third_party/dxvk" "$ROOT"/tools/dxvk-patches/*.patch

apply_series "$ROOT/third_party/vkd3d-proton" "$ROOT"/tools/vkd3d-proton-patches/*.patch

apply_series "$ROOT/third_party/vkd3d-proton/subprojects/dxil-spirv" "$ROOT"/tools/dxil-spirv-patches/*.patch

build dxvk "$ROOT/third_party/dxvk" -Denable_d3d8=false -Denable_d3d9=true -Denable_d3d10=false
build vkd3d-proton "$ROOT/third_party/vkd3d-proton"
ls -1 "$PREFIX/lib"/*.so
