# Implementation Plan: WDDM 1.0 cursor, and declining the legacy surface

**Branch**: `013-wddm10-cursor-and-legacy` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

The first virtio-gpu requests the driver sends. `GpuRequestWriter` encodes the
2D and cursor requests; `GpuCommandQueue` replaces each bare `VirtQueue` in
`VirtioGpuDevice` with slots of contiguous memory the requests and responses
live in until the DPC reclaims them. `CursorImage` draws a pointer shape into
the cursor resource's backing; `HardwareCursor` creates the resource once per
power-up and sends the transfer, update and move requests. Eight legacy DDIs
decline exactly as dxgkrnl's own defaults do.

## Phases

1. `GpuRequestWriter` and its protocol section; `GpuCommandQueue`; the fake
   virtqueue device answers requests; `VirtioGpuDevice` sends and harvests on
   both queues.
2. `CursorImage` with a fuzz target; `HardwareCursor`.
3. Records and km proofs; pointer capabilities; adapter DDIs, stubs, entry
   points, ten slots; scenario; docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| III | Pass - each queue, image and the cursor is an object; stub entry points are one line |
| IV | Pass - fake device reads what the driver publishes; fuzz for the shape bitmap |
| V | Pass - hostile used elements, responses and shapes are reachable states |
| VI | Pass - pointer caps reported with the implementation |
| VII | Pass - sends take one spin lock per queue at up to DISPATCH; nothing allocates after start |
