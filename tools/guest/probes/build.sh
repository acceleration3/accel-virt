#!/usr/bin/env bash
# The guest probes tools/frame_pacing.py pushes and runs: wgc_probe.exe (a
# Windows.Graphics.Capture workload that reports every frame it gets) and
# etw_schema.exe (a manifest provider's events and maps, as the guest's TDH
# describes them); and d3d9_probe.exe, a 32-bit Direct3D 9 device made the way
# a Source engine game makes one, presenting a known colour (docs/shared-surfaces.md); and
# dxgi_modes.exe, the display modes DXGI lists each output in, format by format; and
# d3d11_cube.exe, a cube map made with each face's initial data and read back; and
# gl_window.exe, an OpenGL window swapping a frame whose orientation shows; and
# d3d11_flip.exe, a window shown through an R8G8B8A8 flip chain, as a window chain is; and
# shared_id_probe.exe, shared textures and the ids avgpu minted for them, read
# in user mode as docs/external-export.md tells a mirror to; and
# cross_adapter_probe.exe, row-major memory made on one avgpu adapter and opened on another (docs/host-memory.md); and
# vk_window.exe, a Vulkan window presenting a frame whose orientation and channel order show,
# its swapchain made again when it resizes. llvm-mingw (https://github.com/mstorsjo/llvm-mingw, found on
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
x86_64-w64-mingw32-clang++ -O2 -std=c++17 -municode -Wall -static \
    "$source_dir/dxgi_modes.cpp" -o "$output/dxgi_modes.exe" -ldxgi
x86_64-w64-mingw32-clang++ -O2 -std=c++17 -municode -Wall -static \
    "$source_dir/d3d11_cube.cpp" -o "$output/d3d11_cube.exe" -ld3d11
x86_64-w64-mingw32-clang++ -O2 -std=c++17 -municode -Wall -static \
    "$source_dir/gl_window.cpp" -o "$output/gl_window.exe" -lopengl32 -luser32 -lgdi32
x86_64-w64-mingw32-clang++ -O2 -std=c++17 -municode -Wall -static \
    "$source_dir/d3d11_flip.cpp" -o "$output/d3d11_flip.exe" -ld3d11 -ldxgi -luser32 -lgdi32
x86_64-w64-mingw32-clang++ -O2 -std=c++17 -municode -Wall -static -I"$root/third_party/dxvk/include/vulkan/include" \
    "$source_dir/vk_window.cpp" -o "$output/vk_window.exe" -luser32 -lgdi32
x86_64-w64-mingw32-clang++ -O2 -std=c++17 -municode -Wall -static \
    "$source_dir/shared_id_probe.cpp" -o "$output/shared_id_probe.exe" -lgdi32 -ld3d11 -ldxgi
x86_64-w64-mingw32-clang++ -O2 -std=c++17 -municode -Wall -static \
    "$source_dir/cross_adapter_probe.cpp" -o "$output/cross_adapter_probe.exe" -ld3d11 -ld3d12 -ldxgi
echo "$output/cross_adapter_probe.exe $output/shared_id_probe.exe $output/wgc_probe.exe $output/etw_schema.exe $output/d3d9_probe.exe $output/dxgi_modes.exe $output/d3d11_cube.exe $output/gl_window.exe $output/d3d11_flip.exe $output/vk_window.exe"
