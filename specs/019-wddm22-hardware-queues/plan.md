# Implementation Plan: WDDM 2.2 hardware contexts and queues

**Branch**: `019-wddm22-hardware-queues` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

The adapter reports WDDM 2.2 and answers the display caps extension. It gains
the hardware queue model dxgkrnl drives once hardware scheduling is on:
`HwQueueTable` creates `HwQueue`s on contexts that ask for them,
`HwQueueSubmissions` carries submissions at DISPATCH until the DPC writes each
progress fence and reports MONITORED_FENCE_SIGNALED, and ResetHwEngine drops
what was in flight. Nothing enables hardware scheduling yet; the harness's
`FakeHwQueue` drives the model through the table. Every display slot of 2.2
stays NULL.

## Phases

1. WDDM 2.2 and the display caps extension (harness first).
2. Records and km proofs; `FakeHwQueue`.
3. `HwQueue`, `HwQueueTable`, `HwQueueSubmissions`, MONITORED_FENCE_SIGNALED,
   HwQueueSupported contexts; the four DDIs; entry points; slots.
4. Scenario; docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| III | Pass - each queue an object, the table and the in-flight ring system classes |
| IV | Pass - the harness drives queues the way DXGHWQUEUE and VidSch do |
| VI | Pass - hardware scheduling is not claimed |
| VII | Pass - submission and completion at DISPATCH under a spin lock, no allocation; create and destroy PASSIVE |
