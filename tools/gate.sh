#!/usr/bin/env bash
# The full verdict: every gate, in every build configuration. ASan, profile
# instrumentation and libFuzzer each need every object built their own way, so
# each configuration has its own build directory and its own ctest run.
#
#   default   build/           unit tests (ASan+UBSan), design_rules, symbol_tests,
#                              driver_image, self_contained, tools_tests
#   coverage  build-coverage/  the above plus coverage_gate
#   fuzz      build-fuzz/      the above plus fuzz_<target>_smoke
set -uo pipefail
cd "$(dirname "$0")/.."

CONFIGURATIONS=(
    "default build"
    "coverage build-coverage -DAV_COVERAGE=ON"
    "fuzz build-fuzz -DAV_FUZZ=ON"
)

for entry in "${CONFIGURATIONS[@]}"; do
    read -r name directory options <<< "$entry"
    echo "== gate: $name ($directory)"
    # shellcheck disable=SC2086 # options is a word list on purpose
    if ! cmake -S . -B "$directory" -G Ninja -DCMAKE_CXX_COMPILER=clang++ $options >/dev/null; then
        echo "gate: $name failed to configure" >&2
        exit 1
    fi
    if ! cmake --build "$directory"; then
        echo "gate: $name failed to build" >&2
        exit 1
    fi
    if ! ctest --test-dir "$directory" --output-on-failure -j"$(nproc)"; then
        echo "gate: $name failed" >&2
        exit 1
    fi
done
echo "gate: every configuration passed"
