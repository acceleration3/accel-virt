#!/usr/bin/env bash
# The guest probes tools/frame_pacing.py pushes and runs: wgc_probe.exe (a
# Windows.Graphics.Capture workload that reports every frame it gets) and
# etw_schema.exe (a manifest provider's events and maps, as the guest's TDH
# describes them); and d3d9_probe.exe, a 32-bit Direct3D 9 device made the way
# a Source engine game makes one, presenting a known colour (docs/shared-surfaces.md). llvm-mingw (https://github.com/mstorsjo/llvm-mingw, found on
# PATH or in /opt/llvm-mingw): its MinGW-w64 headers carry the WinRT capture,
# Direct3D11 interop and TDH declarations the Windows kit here does not, and a
# distribution's GCC MinGW lacks the interop one.
#
#   tools/guest/probes/build.sh [output directory]
set -euo pipefail
root=$(cd "$(dirname "$0")/../../.." && pwd)
source_dir=$root/tools/guest/probes
output=${1:-$root/build/guest-probes}
mkdir -p "$output"
PATH=$PATH:/opt/llvm-mingw/bin
command -v x86_64-w64-mingw32-clang++ >/dev/null || { echo "llvm-mingw is not installed (x86_64-w64-mingw32-clang++)" >&2; exit 1; }
x86_64-w64-mingw32-clang++ -O2 -std=c++17 -municode -Wall -Wno-unknown-pragmas -static \
    "$source_dir/wgc_probe.cpp" -o "$output/wgc_probe.exe" \
    -ld3d11 -ldxgi -ldwmapi -lwinmm -lruntimeobject -luser32 -lgdi32
x86_64-w64-mingw32-clang -O2 -std=c11 -municode -Wall -static \
    "$source_dir/etw_schema.c" -o "$output/etw_schema.exe" -ltdh -lole32
i686-w64-mingw32-clang++ -O2 -std=c++17 -municode -Wall -static \
    "$source_dir/d3d9_probe.cpp" -o "$output/d3d9_probe.exe" -ld3d9 -luser32 -lgdi32
echo "$output/wgc_probe.exe $output/etw_schema.exe $output/d3d9_probe.exe"
