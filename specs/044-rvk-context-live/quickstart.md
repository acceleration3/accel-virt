# Quickstart: rvk contexts in a real guest

## Linux

```bash
cmake --build build && ctest --test-dir build -E tools_tests -j16
python3 tools/check_generated.py          # the freestanding emitter output is what is committed
ctest --test-dir build-fuzz -R "fuzz_executor_(opencl|opengl)_smoke"
ctest --test-dir build-coverage -R coverage_gate
```

## A real guest

```bash
cmake --build build-qemu                  # librvk_executor.so, unsanitized
AV_GUEST_IMAGE=~/.local/share/libvirt/images/win11.qcow2 \
  tools/guest/run_guest_suite.py --rvk --executor build-qemu/librvk_executor.so
```

Expected: `verdict.txt` names the executor and the host GPU; `report.txt` has
`ok transport.nop`, a `transport.latency` measurement, every `vulkan.*`,
`opencl.*`, `opengl.*` and `isolation.*` step ok; `rvkgpu.log` shows one
`DMA_FAULTED` for the isolation scenario's faulted context and no adapter reset.

## The gate

`AV_GUEST_IMAGE=... tools/gate.sh` boots the guest twice: the suite with the echo
library, then `--rvk` with the real one.
