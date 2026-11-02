#!/usr/bin/env bash
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
x86_64-w64-mingw32-clang++ -O2 -std=c++17 -municode -Wall -Wno-unknown-pragmas -static \
    "$source_dir/d3d9_dirty_present.cpp" -o "$output/d3d9_dirty_present.exe" \
    -ld3d9 -ld3d11 -ldxgi -ldwmapi -lruntimeobject -luser32 -lgdi32
echo "$output/d3d9_dirty_present.exe $output/cross_adapter_probe.exe $output/shared_id_probe.exe $output/wgc_probe.exe $output/etw_schema.exe $output/d3d9_probe.exe $output/dxgi_modes.exe $output/d3d11_cube.exe $output/gl_window.exe $output/d3d11_flip.exe $output/vk_window.exe"
