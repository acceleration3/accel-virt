# Implementation Plan: WDDM 1.0 PnP, power and adapter lifetime

**Branch**: `006-wddm10-pnp-and-adapter` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

The driver becomes a miniport: core owns the DDI table and `extern "C"` entry
points over layout-proven mirrors; `DriverEntry` constructs the root in static
storage and registers it; adapters bring up the virtio-gpu device with the
005 transport; the fake dxgkrnl drives the real entry points through the
recorded sequences.

## Phases

1. **Skeleton** - constitution 2.1.0; `ObjectStorage`, `NtStatus`,
   `MiniportBinding`, `Miniport`, entry points for all 17 slots (stubs where
   later phases fill in), `DriverEntry` + host debug-port log, fake dxgkrnl
   registers through them.
2. **Mirrors and inputs** - `DXGK_START_INFO`, `DXGKRNL_INTERFACE`,
   `DXGK_DEVICE_INFO`, CM resource descriptors, child descriptor/status,
   device descriptor, with km offset proofs; `DxgkrnlInterface`, `ResourceList`
   (fuzzed).
3. **Adapter lifetime** - `Platform` register I/O, `MappedRegisterWindow`,
   `Adapter` start/stop/remove/power/reset over fake callbacks and the fake
   virtio device.
4. **Interrupts and children** - ISR, DPC, QueryChildRelations/Status/Descriptor.
5. **Scenarios** - fake dxgkrnl bring-up/teardown order and abusive variants.

## Constitution Check

| Principle | Status |
|---|---|
| II | Pass - decisions in core; km: DriverEntry, layout proofs, platform translations |
| III | Amended 2.1.0 (user decision): entry points in core, one binding pointer, `ObjectStorage` the only placement new |
| IV | Pass - per-phase tests first; resource list fuzzed; scenario runner |
| VI | Pass - declares 0x11007; reports nothing beyond what is implemented |
| VII | Pass - ISR/ResetDevice allocation- and lock-free, checked by the mock |
