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
`opencl.*`, `opengl.*` and `isolation.*` step ok; the host log shows the
isolation scenario's context faulted with code 6, and the guest shows no adapter
reset and no bugcheck. The driver never reports `DMA_FAULTED`: dxgmms2.sys `VidSchDdiNotifyInterruptWorker` @140031b80 bugchecks 0x119/0xD on `DXGK_INTERRUPT_DMA_FAULTED` from a GPU MMU adapter.

## The gate

`AV_GUEST_IMAGE=... tools/gate.sh` boots the guest twice: the suite with the echo
library, then `--rvk` with the real one.
