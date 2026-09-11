# Implementation Plan: WDDM 1.0 present and scanout

**Branch**: `012-wddm10-present-and-scanout` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

Present, SetVidPnSourceAddress and SetVidPnSourceVisibility, and a synthetic
vsync. `PresentRequest` encodes a `PresentCommand` and its sub-rectangles into
the DMA buffer with patch locations for the allocations it names, multipass
when the buffer is short. `SourceScanout` keeps each source's primary address
and visibility under a spin lock. `SyntheticVsync` owns a `PeriodicTimer` - a
new platform facility, `ExAllocateTimer` in the kernel and a test-driven clock
in the mock - and reports CRTC_VSYNC with the scanout address through its own
`InterruptNotification`.

## Phases

1. Platform timer: contract, `MockPlatform` (create/start/stop/destroy rules,
   `run_timers`), `KernelPlatform`; `PeriodicTimer`.
2. Records and km proofs; `PresentCommand` (proto, documented); `PresentRequest`.
3. `SourceScanout`, `DisplaySources` extension, `InterruptNotification::report_vsync`,
   `SyntheticVsync`; adapter DDIs, entry points, the timer callback, three slots.
4. Scenarios (render core created), fuzz_present, docs, gate, merge, push.

## Constitution Check

| Principle | Status |
|---|---|
| III | Pass - timer and scanout are objects; the timer callback is a one-line extern "C" entry point |
| IV | Pass - mock timer enforces IRQL and leaks; fuzz target for Present |
| V | Pass - every Present field and rectangle checked |
| VI | Pass - no capability changes |
| VII | Pass - tick at DISPATCH takes one spin lock, allocates nothing |
