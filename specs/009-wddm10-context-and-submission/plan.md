# Implementation Plan: WDDM 1.0 contexts and packet submission

**Branch**: `009-wddm10-context-and-submission` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

Eight slots. Contexts live in an `ObjectPool`; Render and Patch are
request objects over their argument blocks with every offset checked; one
`ExecutionNode` queues submitted fences under a spin lock and a loopback DPC
completes them through `InterruptNotification`, which calls
`DxgkCbNotifyInterrupt` inside `DxgkCbSynchronizeExecution`. BuildPagingBuffer
answers the three aperture operations. Deliberately small: 017 retires Patch.

## Design

- The synchronize routine is a kernel callback: `AvSynchronizedNotify` in
  `src/core/ddi/entry_points.cpp`, one line, forwarding to the
  `InterruptNotification` behind its context (constitution III's C ABI exception).
- `DxgkrnlInterface` gains `synchronize_execution`, `notify_interrupt`,
  `notify_dpc`; the fake adapter implements them, running the routine at device
  IRQL and recording each notification.
- Lock ranks: context table 105 (between devices and allocations), execution
  node 200 (spin lock, innermost).
- DestroyContext receives only the context: `Context` remembers its `Device`,
  which remembers its `Adapter`.
- `FakeVidSch` (harness) drives the table: render, patch, submit at DISPATCH,
  the DPC at DISPATCH, and checks every notification.

## Phases

1. Records (CREATECONTEXT, RENDER, ALLOCATIONLIST, PATCHLOCATIONLIST, PATCH,
   SUBMITCOMMAND, PREEMPTCOMMAND, QUERYCURRENTFENCE, BUILDPAGINGBUFFER,
   NOTIFY_INTERRUPT_DATA), callback typing, interface methods.
2. Contexts: `Context`, `ContextTable`, Create/DestroyContext.
3. Render and Patch: `RenderRequest`, `PatchRequest`, hostile tables, fuzz.
4. Execution: `InterruptNotification`, `ExecutionNode`, Submit, Preempt,
   QueryCurrentFence, DPC; `PagingRequest`.
5. Harness and scenarios: `FakeVidSch`, render core rule progress, docs, gate,
   merge.

## Constitution Check

| Principle | Status |
|---|---|
| III | Pass - request objects own their argument blocks; one-line synchronize callback |
| IV | Pass - hostile tables and a fuzz target for Render/Patch/Submit |
| V | Pass - every UMD-derived offset and index checked, overflow-safe |
| VI | Pass - no capability changes |
| VII | Pass - Submit, Preempt, DPC under a spin lock only; the mock enforces it |
