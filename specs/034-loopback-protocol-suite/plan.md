# Implementation Plan: Loopback protocol test suite

**Branch**: `034-loopback-protocol-suite` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

`src/proto` gains `RvkRequestWriter` and `GpuResponseReader`, the driver's half
of the virtio-gpu context requests. `src/core/rvk/` gains the driver side of
the rvk protocol: `CapsetQuery`, `ContextMemory`, `ContextRequestSequence`,
`RingSubmitter`, `FenceObserver` and `GuestContext` composing them. The harness
gains a guest-memory translator in `MockHost` and `LoopbackDevice`, and
`tests/loopback/` runs guest contexts against the device core in one process.

## Decisions

- **Pull-style requests.** Sequences write their next request into a caller's
  buffer and accept answers in order; no virtual link, no allocation.
- **Memory.** Three contiguous blocks per context from `Platform`: ring
  `32 + 2^order` rounded to a page, fence page and reply slot one page each;
  the fresh ring header is written before any request. Each becomes a guest
  blob of one entry.
- **Ids.** The caller chooses the context id, three resource ids and the
  doorbell page; the driver will allocate them (M3).
- **Locking.** `GuestContext` holds a `SpinLock` (a new `LockRank::rvk_context`)
  across submit and serve, since a DPC and a submission can meet.
- **Hostile device.** A lie fails the context (`failed` state): submit refuses,
  serve reports invalid_state; teardown still releases what the device holds.
- **Head.** The submitter reads the ring's head field and judges it with
  `RingProgress` against its own tail before deciding backpressure.
- **Loopback delivery.** The test waits for `MockHost::raised(line)` to move,
  then serves the context on its own thread, like a DPC.

## Phases

1. Proto: `RvkRequestWriter`, `GpuResponseReader`; fuzz_gpu_response.
2. Core: `CapsetQuery`, `ContextMemory`, `ContextRequestSequence`.
3. Core: `RingSubmitter`, `FenceObserver`, `GuestContext`; fuzz_guest_context.
4. Harness and loopback: `MockHost` translator, `LoopbackDevice`, CMake,
   `tests/loopback/`; docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| II | Pass - protocol decisions in core objects; nothing platform-specific |
| III | Pass - every class owns state; no virtual link or singleton |
| IV | Pass - unit tests per class with hostile answers and pages, fuzz, and the loopback suite |
| V | Pass - every device-written byte copied and judged |
| VII | Pass - IRQL ceilings declared and checked by MockPlatform |
| VIII | Pass - no DDI changes and no capability reported until M3 uses them |
