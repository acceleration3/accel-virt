# Implementation Plan: QEMU device skeleton, object model and threading

**Branch**: `026-device-skeleton-and-threading` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

`src/device` gains its core: a `Host` contract (threads, events, mutexes, the
main loop, the GPU catalog) with `TableHost` over the C operations table the QEMU
glue fills and `MockHost` for tests; `GpuBinding` over `DeviceUuid`; per-context
`Worker`s under a `DeviceContextTable`; `RvkDevice`, held by the module's
`DeviceModule` and reached through one-line `extern "C"` entry points
(`rvk_device.h`). `src/device/qemu` holds the C glue and the meson patch;
`tools/fetch_qemu.sh` pins the QEMU source by hash, `tools/build_qemu.sh` builds
`qemu-system-x86_64` with the modules against `librvk_device.a`, and a qtest smoke
test joins the gate. `tools/host/list_gpus.py` discovers GPUs.

## Layout

```
src/device/host/       Host, TableHost
src/device/gpu/        DeviceUuid, GpuBinding
src/device/support/    TextBuffer
src/device/threading/  WorkItem, Worker
src/device/context/    DeviceContext, DeviceContextTable
src/device/module/     RvkDevice, DeviceInstance, DeviceModule
src/device/abi/        rvk_device.h (C), entry points
src/device/qemu/       virtio-gpu-rvk.{c,h}, virtio-gpu-pci-rvk.c, host operations, qemu.patch
tests/harness/device/  MockHost
```

## Phases

1. `Host`, `MockHost`, `DeviceUuid`, `TextBuffer`, `GpuBinding` (tests first).
2. `WorkItem`, `Worker`, `DeviceContext`, `DeviceContextTable` with real threads.
3. `RvkDevice`, `TableHost`, `DeviceInstance`, `DeviceModule`, the C ABI;
   `librvk_device.a` and `rvk_device.pc` from CMake.
4. The C glue, meson patch, fetch and build scripts, the qtest smoke test in the
   gate; `list_gpus.py`; docs and kb corrections; constitution amendment for the
   module root; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| II | Pass - decisions in `src/device`, glue translation only |
| III | Pass - every row an object or system class; one amendment: `DeviceModule` static storage, as the kernel shim's root |
| IV | Pass - `MockHost` is adversarial: contract violations, failure injection, leak checks, real threads under sanitizers |
| V | Pass - `host-gpu` text is parsed, never trusted |
| VI | Pass - no virtio feature claimed |
