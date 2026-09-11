# Implementation Plan: WDDM 1.0 capability reporting

**Branch**: `007-wddm10-capability-reporting` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

Slots 17, 34 and 35 join the table. QueryAdapterInfo bounds each request's
buffers, answers `DRIVERCAPS` and `WDDMDEVICECAPS` from one capability owner,
and declines every other type with `STATUS_NOT_IMPLEMENTED`. The fake dxgkrnl
issues the `DRIVERCAPS` query in the recorded start order and applies
`DXGADAPTER::Initialize`'s capability rules, so every later feature that turns
a capability on is checked against the DDIs it registered.

## Design

- **DDI records** (`src/core/ddi/`, offsets proven in
  `src/km/ddi_record_layout.cpp`): `DxgkargQueryAdapterInfoRecord` (0x30),
  `DxgkDriverCapsRecord` (0x250, bitfield unions as their `Value` word),
  `DxgkWddmDeviceCapsRecord` (4), `DxgkargEscapeRecord` (0x30),
  `DxgkargCollectDbgInfoRecord` (0x20), `DxgkargCollectDbgInfoExtRecord` (0x20).
- **`AdapterInfoQuery`** wraps one argument block: `type()`,
  `require_input(bytes)`, `write_output(bytes, count)` - a null pointer or a
  declared size below `count` is `invalid_parameter` and writes nothing.
- **`DriverCapabilities`** owns every capability value; at this level
  `WDDMVersion` = `DXGKDDI_WDDMv1` and `HighestAcceptableAddress` = all ones.
  Later features add members here as they implement capabilities.
- **`DebugReport`** is the adapter's state at the moment of collection
  (started, powered, negotiated features), written through `WireWriter` so a
  short buffer gets whole fields only.
- **`Adapter`** gains `query_adapter_info`, `escape`, `collect_dbg_info`;
  `NtStatus` gains `not_implemented` (0xC0000002).
- **Harness**: `FakeDxgadapter` (dxgkrnl's `DXGADAPTER`) issues queries
  through slot 17, fails the test on a status outside dxgkrnl's accepted four,
  and validates `DRIVERCAPS` with one `CapabilityRule` per
  `DXGADAPTER::Initialize` check. `FakeDxgkrnlFdo::start_device` runs it after
  QueryChildRelations and stops the device when it refuses.

Not modelled here: the render and display cores `DXGADAPTER::Initialize`
creates after validation (a render adapter's `CalibrateGpuClock` /
`SetStablePowerState` requirement, "no display or render capabilities"); they
arrive with 008 and 011, which create them.

## Phases

1. **Records** - the six records with km proofs; `NtStatus::not_implemented`.
2. **QueryAdapterInfo** - tests then `AdapterInfoQuery`, `DriverCapabilities`,
   `Adapter::query_adapter_info`, entry point and slot; hostile sweep; fuzz
   target `fuzz_adapter_info` with corpus.
3. **Escape and CollectDbgInfo** - tests then `DebugReport`,
   `Adapter::escape`, `Adapter::collect_dbg_info`, entry points and slots.
4. **Harness and scenarios** - `CapabilityRule`, `FakeDxgadapter`, FDO start
   step, capability-rule tests (each rule refuses a crafted answer), scenario
   over every query type; docs corrected; gate; merge.

## Constitution Check

| Principle | Status |
|---|---|
| II | Pass - decisions in core; km adds layout proofs only |
| III | Pass - three object classes with state; entry points one line |
| IV | Pass - tests first per phase; argument blocks hostile-swept and fuzzed |
| V | Pass - every size checked before a read or write |
| VI | Pass - reports WDDM 1.0 and no capability; the fake dxgkrnl enforces it |
| VII | Pass - QueryAdapterInfo/Escape passive; CollectDbgInfo allocation- and lock-free |
