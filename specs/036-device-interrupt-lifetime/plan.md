# Implementation Plan: QEMU device: fence interrupts that follow the driver's lifetime

**Branch**: `036-device-interrupt-lifetime` | **Date**: 2026-09-12 | **Spec**: [spec.md](spec.md)

## Summary

`src/device/module/` gains `DriverStatus`, the object that remembers the last
status byte the guest wrote and matches the fence interrupts to it: connected
while `DRIVER_OK` stands, released the moment the guest clears it, and released
once on a reset. `VirtioGpuRvk::status` becomes a forward to it, and a context
whose fences could not interrupt the guest is refused rather than created. The
qemu configuration gains the MSI-X sequences a restarting Windows display
driver performs, and the loopback suite the mid-flight clear.

## Decisions

- **The status byte is state, so it is a class.** `DriverStatus` owns the last
  byte and the transition; `FenceInterrupts` keeps owning the connection and
  its lock. Neither takes what it owns as a parameter.
- **Connected iff `DRIVER_OK`.** A write that clears the bit disconnects, which
  is what 035's live run needed: `viogpudo` stopped with status 11 and no
  reset, so nothing was released and the next driver's MSI-X toggle registered
  the same eventfd twice. A write that repeats the bit asks the host nothing.
- **Status 0 disconnects.** It is the reset the guest writes before QEMU calls
  `reset`, and the release is idempotent, so the pair disconnects exactly once.
  This inverts the "status 0 comes before the reset: nothing" case in
  `test_virtio_gpu_rvk.cpp`, which encoded the behaviour this feature fixes.
- **The realized guard stays in `VirtioGpuRvk`.** `FenceInterrupts` has no lock
  before `realize`, so an unrealized device is never fed a status; otherwise
  every status write on an unrealized device would log a refusal.
- **A context needs an interrupt.** `create_context` answers `invalid_state`
  while the interrupts are down, so a guest that never set `DRIVER_OK`, or a
  host that could not route (US3), gets a status the driver reports instead of
  a context whose fences no one delivers. The 2D path is untouched.
- **The core models no vector and no mask.** Masking, per-vector routing and
  the pending bits belong to QEMU's `msix.c` and `virtio-pci.c`, which FR-006
  forbids patching; the core decides only when to connect. US2 is therefore
  qtest-level, over the MSI-X table the guest programs.
- **`MockHost` already refuses a double connect** (`HostViolation::fence_misuse`),
  so FR-003 is a gate in every existing device test, not new harness state.

## Phases

1. `DriverStatus` with its L1 table over every status, transition and refusal;
   `VirtioGpuRvk` forwards to it; `create_context` refuses while disconnected.
2. qemu: MSI-X enable toggled under `DRIVER_OK`, the 035 restart sequence
   (`DRIVER_OK`, clear, MSI-X off and on, `DRIVER_OK`), a fence delivered after
   each, and open descriptors and KVM routes unchanged across a toggle sweep.
3. Loopback: `DRIVER_OK` cleared while a worker raises; docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| II | Pass - the decision is in `src/device`; `src/device/qemu` keeps only the two host calls it already has |
| III | Pass - `DriverStatus` owns the status byte and its dependencies are injected; no static method, no new free function |
| IV | Pass - a table test drives every status and transition, invariants after each, refusals from `MockHost`; no new byte decoding, so no new fuzz target |
| V | Pass - the status byte is guest-authored and every value is accepted without arithmetic on it |
| VIII | Pass - no vector or mask modelled, no capability added; the connection is the one `FenceInterrupts` already owns |
