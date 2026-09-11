# Implementation Plan: WDDM 2.1 additions

**Branch**: `018-wddm21-additions` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

The adapter reports WDDM 2.1. `DriverCapabilities` answers ControlModeBehavior
with nothing satisfied and `Edid` the monitor's link capabilities for
UpdateMonitorLinkInfo. Four overlay and allocation-property slots stay NULL. The
harness learns `SetModeBehavior` in `FakeDxgadapter::initialize` and
`DXGMONITOR::_UpdateLinkInfo` in `FakeDxgmonitor`.

## Phases

1. Records and km proofs; harness: mode behavior at initialize, `FakeDxgmonitor`.
2. Tests then WDDM 2.1, ControlModeBehavior, UpdateMonitorLinkInfo; entry
   points; two slots.
3. Scenario; docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| III | Pass - both answers come from classes owning the state they report |
| IV | Pass - fake dxgkrnl applies the RE'd handling of both answers |
| VI | Pass - WDDM 2.1 makes nothing mandatory that is not implemented |
| VII | Pass - both DDIs PASSIVE |
