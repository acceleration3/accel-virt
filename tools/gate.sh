#!/usr/bin/env bash
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
    echo "== gate: guest scanout (egl-headless)"
    if ! python3 -B tools/guest/run_guest_suite.py --image "$AV_GUEST_IMAGE" --work build-guest-scanout \
            --display egl-headless; then
        echo "gate: guest scanout failed" >&2
        exit 1
    fi
    echo "gate: guest scanout passed after $((SECONDS - started)) s"
    if [ ! -f build-qemu/libav_executor.so ]; then
        echo "gate: guest real skipped: build-qemu/libav_executor.so was not built" >&2
        exit 1
    fi
    real=(--av --opengl)
    if python3 -B tools/fetch_vulkan_runtime.py --check >/dev/null 2>&1; then
        real+=(--vulkan)
    else
        echo "gate: guest vulkan skipped: the Vulkan runtime is not fetched (tools/fetch_vulkan_runtime.py)" >&2
    fi
    if python3 -B tools/fetch_opencl_loader.py --check >/dev/null 2>&1; then
        real+=(--opencl)
    else
        echo "gate: guest opencl skipped: the OpenCL loader is not fetched (tools/fetch_opencl_loader.py)" >&2
    fi
    echo "== gate: guest real (${real[*]}, build-qemu/libav_executor.so)"
    if ! python3 -B tools/guest/run_guest_suite.py --image "$AV_GUEST_IMAGE" --work build-guest-real "${real[@]}" \
            --executor build-qemu/libav_executor.so; then
        echo "gate: guest real failed" >&2
        exit 1
    fi
    echo "gate: guest real passed after $((SECONDS - started)) s"
    if python3 -B tools/fetch_cts.py --check >/dev/null 2>&1; then
        echo "== gate: guest cts (build-qemu/libav_executor.so)"
        if ! python3 -B tools/guest/run_guest_suite.py --image "$AV_GUEST_IMAGE" --work build-guest-cts --cts \
                --executor build-qemu/libav_executor.so; then
            echo "gate: guest cts failed" >&2
            exit 1
        fi
        echo "gate: guest cts passed after $((SECONDS - started)) s"
    else
        echo "gate: guest cts skipped: the Vulkan CTS is not fetched (tools/fetch_cts.py); "\
             "tools/cts_pins.txt says why it is empty" >&2
    fi
else
    echo "gate: guest skipped: AV_GUEST_IMAGE names no prepared image"
    echo "gate: a feature does not merge on a skipped guest (feature 037, FR-006)"
fi
echo "gate: every configuration passed in $((SECONDS - started)) s"
