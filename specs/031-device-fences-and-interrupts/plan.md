# Implementation Plan: QEMU device: fence pages and interrupt injection

**Branch**: `031-device-fences-and-interrupts` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

`FenceReporter` gives each context its completion reporting: `FenceWindow`
publishes a `FencePage` into the pinned fence blob at setup and after every
batch `RingConsumer` consumes, and one interrupt per service goes to
`FenceInterrupts`, the device's gate over the fence queues' guest notifiers.
`Host` gains connecting, disconnecting and raising them; the glue adds a fence
virtqueue per doorbell page, hands the driver's status to the core so the
notifiers connect at `DRIVER_OK`, and releases them in reset after the contexts.

## Technical Context

As 030. `MockHost` gains the notifiers (connect can be failed; raising while
disconnected, or for a line past the queues, is a violation; raises are
counted per line). The qtest harness gains MSI-X table and PBA access.

## Decisions

- **Line = doorbell index.** Fence queue `2 + d`; the setup's doorbell names both.
- **Gate with a mutex.** Raise holds `FenceInterrupts`' mutex; connect and
  disconnect hold it across the host calls.
- **Initialization.** `FenceInterrupts::initialize` creates the mutex at
  realize; a device whose mutex cannot be made fails realize with a message.
- **Status.** `rvk_device_status(device, status)`: `DRIVER_OK` connects once;
  anything else does nothing; reset disconnects. A connect failure is logged
  and leaves interrupts off (the pages still work).
- **Order in setup.** Binding, fence reporter prepared (page written), ring
  prepared, doorbell, attach. Withdrawal of both on failure.

## Phases

1. `FenceWindow`, `FenceReporter`, `FenceInterrupts`; `Host` contract, `TableHost`,
   `MockHost`, `MockHostOps`, `rvk_device.h`.
2. `RingConsumer` completion; `ContextBinding::fence_window`; `DeviceContext`,
   `DeviceContextTable`, `VirtioGpuRvk::status`, reset order; `rvk_device_status`.
3. Glue: fence queues, set_status, notifier ops, unrealize; qtest MSI-X and the
   fence suite; docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| II | Pass - when to write and raise is core; the glue sets notifiers |
| III | Pass - reporter and window objects; interrupts a system class owning its lock |
| IV | Pass - every path unit tested with threads; qtest reads PBA and ISR |
| V | Pass - the page is never read back; lines bounded by the doorbell count |
| VII | Pass - main loop and worker sides checked by `MockHost` |
