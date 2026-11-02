#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

which=${1:-all}
jobs=${2:--j8}
tools/fetch_vulkan_samples.sh

configure_and_build() {
    local dir=$1; shift
    cmake -S samples/vulkan -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Release "$@" >/dev/null
    cmake --build "$dir" --target avvksamples -- "$jobs"
}

case $which in
    host|all) configure_and_build build-vulkan-samples/host ;;&
    guest|all)
        if [ ! -f third_party/vulkan-runtime/vulkan-1.dll ]; then
            python3 tools/fetch_vulkan_runtime.py
        fi
        configure_and_build build-vulkan-samples/guest \
            -DCMAKE_TOOLCHAIN_FILE="$PWD/samples/vulkan/mingw_toolchain.cmake" ;;
    host) ;;
    *) echo "usage: $0 [host|guest|all] [-jN]" >&2; exit 2 ;;
esac
