# Implementation Plan: WDDM 2.0 GPU virtual addressing, paging queues, monitored fences

**Branch**: `017-wddm20-gpu-va-and-paging` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

The adapter becomes a WDDM 2.0 GPU MMU adapter. `GpuMmuGeometry` answers the
page table queries; `DriverCapabilities` claims 2.0, MultiEngineAware,
VirtualAddressing and GpuMmu, FlipOnVSyncMmIo, and answers PHYSICALADAPTERCAPS.
`PageTableEntry` and `PageTableUpdate` write VidMm's entries into CPU-mapped page
tables; `PagingCommandWriter` carries the GPU-VA paging operations to the device.
Contexts gain a `RootPageTable`; SubmitCommandVirtual joins the node. The
scanout address becomes atomic so flips can arrive at device IRQL. The harness
learns WDDM 2.0's rules (`FakeDxgadapter`, `FakeVidMm`, `FakeGpuMmu`). Monitored
fences arrive with hardware queues and are not part of WDDM 2.0's mandatory set.

## Phases

1. Records and km proofs.
2. Harness: WDDM 2.0 rules in `FakeDxgadapter`, `FakeGpuMmu`, the implicit
   segment in `FakeVidMm`, ValidateWDDMCaps gating.
3. Platform atomics; `DisplaySources` scanout at device IRQL.
4. `GpuMmuGeometry`, `DriverCapabilities` 2.0 claims, adapter info queries, node
   metadata, allocation segments.
5. `PageTableEntry`, `PageTableUpdate`, `PagingCommandWriter`, `PagingRequest` 2.0
   operations; fuzz_paging.
6. `RootPageTable`, contexts, processes, SubmitCommandVirtual, SetStablePowerState;
   slots; scenarios; docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| III | Pass - geometry, entry, update, command and root are objects with state |
| IV | Pass - fake dxgkrnl and VidMm enforce the RE'd rules; fuzz for paging |
| V | Pass - every paging field bounded before writing |
| VI | Pass - each claim implemented in this feature or before |
| VII | Pass - scanout atomics at device IRQL; SubmitCommandVirtual PASSIVE |
