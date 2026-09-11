# Implementation Plan: WDDM 1.0 VidPn topology and mode enumeration

**Branch**: `011-wddm10-vidpn-and-modes` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

Nine display DDIs over one source and one target, following `basicdisplay.sys`.
The driver reaches dxgkrnl's VidPn through `DxgkCbQueryVidPnInterface`;
`VidPn`, `SourceModeSet` and `TargetModeSet` wrap the interface tables so every
acquired path and mode is released by scope. A fixed mode list and a
synthesized EDID describe the output. The harness gains `FakeVidPn`, an
in-memory VidPn implementing all four interfaces and failing a test on any
unreleased acquisition.

## Phases

1. Records (interface tables, modes, path, DDI argument blocks, device
   descriptor) and km proofs; `DxgkrnlInterface::query_vidpn_interface`.
2. `DisplayModeList`, `Edid`; QueryDeviceDescriptor answers the EDID.
3. `FakeVidPn` harness and tests.
4. `VidPn`, `SourceModeSet`, `TargetModeSet`; `VidPnSupportCheck`,
   `CofuncModality`, `VidPnCommit`, `MonitorModes`; `DisplaySources` state;
   adapter DDIs and slots.
5. Scenarios, docs, gate, merge, push.

## Constitution Check

| Principle | Status |
|---|---|
| III | Pass - request objects per DDI; interface wrappers own their handles |
| IV | Pass - fake VidPn with leak check; hostile mode and path values |
| V | Pass - ids, formats and sizes from dxgkrnl structures are checked |
| VI | Pass - no capability changes |
| VII | Pass - all nine DDIs PASSIVE; no locks beyond existing |
