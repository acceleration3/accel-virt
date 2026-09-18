#!/usr/bin/env bash
# The full verdict: every gate, in every build configuration. ASan, profile
# instrumentation and libFuzzer each need every object built their own way, so
# each configuration has its own build directory and its own ctest run.
#
#   default   build/           unit tests (ASan+UBSan), design_rules, symbol_tests,
#                              driver_image, self_contained, and the tools
#                              modules (label tools, the guest runner sharded)
#   coverage  build-coverage/  the unit tests again for their profiles, and
#                              coverage_gate
#   fuzz      build-fuzz/      the fuzz_<target>_smoke runs only (label fuzz):
#                              its unit tests are the default configuration's
#   qemu      build-qemu/      QEMU patched with the device, and qemu_smoke
#   guest     build-guest/     the in-guest suite in the image $AV_GUEST_IMAGE
#                              names on the echo executor, skipped when it
#                              names none
#   guest real build-guest-real/  one boot on the real executor for the rvk
#                              scenarios, the Vulkan ICD through the Khronos
#                              loader (when tools/fetch_vulkan_runtime.py has
#                              run) and the OpenGL ICD through opengl32.dll
#
# AV_GATE_JOBS sets the parallelism of every build and ctest run (default: the
# processor count); a host short of memory runs the gate with fewer.
set -uo pipefail
cd "$(dirname "$0")/.."

JOBS="${AV_GATE_JOBS:-$(nproc)}"
CONFIGURATIONS=(
    "default build all"
    "coverage build-coverage all -DAV_COVERAGE=ON"
    "fuzz build-fuzz fuzz -DAV_FUZZ=ON"
    "qemu build-qemu all -DAV_QEMU=ON"
)

started=$SECONDS
for entry in "${CONFIGURATIONS[@]}"; do
    read -r name directory selection options <<< "$entry"
    echo "== gate: $name ($directory)"
    # shellcheck disable=SC2086 # options is a word list on purpose
    if ! cmake -S . -B "$directory" -G Ninja -DCMAKE_CXX_COMPILER=clang++ $options >/dev/null; then
        echo "gate: $name failed to configure" >&2
        exit 1
    fi
    if ! cmake --build "$directory" -j"$JOBS"; then
        echo "gate: $name failed to build" >&2
        exit 1
    fi
    labels=()
    if [ "$selection" != all ]; then
        labels=(-L "$selection")
    fi
    if ! ctest --test-dir "$directory" --output-on-failure -j"$JOBS" "${labels[@]}"; then
        echo "gate: $name failed" >&2
        exit 1
    fi
    echo "gate: $name passed after $((SECONDS - started)) s"
done
if [ -n "${AV_GUEST_IMAGE:-}" ]; then
    echo "== gate: guest ($AV_GUEST_IMAGE)"
    if ! python3 -B tools/guest/run_guest_suite.py --image "$AV_GUEST_IMAGE" --work build-guest; then
        echo "gate: guest failed" >&2
        exit 1
    fi
    echo "gate: guest passed after $((SECONDS - started)) s"
    # The rvk scenarios and both ICDs need the real executor on a host GPU, and
    # the suite's timeout scenario the echo library's stall, so they are a
    # second boot - one for all three, since each is a process of its own that
    # leaves the machine as it found it.
    if [ ! -f build-qemu/librvk_executor.so ]; then
        echo "gate: guest real skipped: build-qemu/librvk_executor.so was not built" >&2
        exit 1
    fi
    real=(--rvk --opengl)
    if python3 -B tools/fetch_vulkan_runtime.py --check >/dev/null 2>&1; then
        real+=(--vulkan)
    else
        echo "gate: guest vulkan skipped: the Vulkan runtime is not fetched (tools/fetch_vulkan_runtime.py)" >&2
    fi
    echo "== gate: guest real (${real[*]}, build-qemu/librvk_executor.so)"
    if ! python3 -B tools/guest/run_guest_suite.py --image "$AV_GUEST_IMAGE" --work build-guest-real "${real[@]}" \
            --executor build-qemu/librvk_executor.so; then
        echo "gate: guest real failed" >&2
        exit 1
    fi
    echo "gate: guest real passed after $((SECONDS - started)) s"
    # The conformance run is a boot of its own: it is hours of CTS rather than
    # minutes of scenarios, and it only runs at all when the build is pinned
    # and fetched (feature 048, FR-006).
    if python3 -B tools/fetch_cts.py --check >/dev/null 2>&1; then
        echo "== gate: guest cts (build-qemu/librvk_executor.so)"
        if ! python3 -B tools/guest/run_guest_suite.py --image "$AV_GUEST_IMAGE" --work build-guest-cts --cts \
                --executor build-qemu/librvk_executor.so; then
            echo "gate: guest cts failed" >&2
            exit 1
        fi
        echo "gate: guest cts passed after $((SECONDS - started)) s"
    else
        echo "gate: guest cts skipped: the Vulkan CTS is not fetched (tools/fetch_cts.py); "\
             "tools/cts_pins.txt says why it is empty" >&2
    fi
else
    echo "gate: guest skipped: AV_GUEST_IMAGE names no prepared image (docs/testing.md, L5)"
    echo "gate: a feature does not merge on a skipped guest (feature 037, FR-006)"
fi
echo "gate: every configuration passed in $((SECONDS - started)) s"
