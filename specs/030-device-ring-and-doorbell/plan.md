# Implementation Plan: QEMU device: submission ring and doorbell

**Branch**: `030-device-ring-and-doorbell` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

`src/proto` gains `BatchHeader` and the setup's doorbell index. The device
reads a bound context's ring from its worker: `BlobWindow` copies bytes out of
a pinned blob (guest pages or a host block), `RingWindow` turns ring counters
into wrapped byte copies and publishes head, and `RingConsumer` judges each
header snapshot with `RingProgress` and consumes complete batches. The worker's
wake event doubles as the doorbell: `Host::connect_doorbell` registers it as the
ioeventfd of the context's page in a new 64-page BAR 0, located by a shared
memory capability with id 0x72. Setup validates the ring header, connects the
doorbell and posts the ring's attachment to the worker; destroy and reset
disconnect the doorbell before the worker is joined and the blobs unpinned.

## Technical Context

C++23 freestanding (`-fno-exceptions -fno-rtti`, no standard library) in
`src/proto` and `src/device`; C glue in `src/device/qemu`; Python qtest suites.
Tests: the L1 registry under ASan+UBSan with `MockHost`'s real pthreads;
`fuzz_ring` and `fuzz_proto` under libFuzzer; the `qemu_virtio_gpu` ctest.

## Decisions

- **Wake source.** The host's events become eventfds in the glue
  (`EventNotifier`, waited with `poll`), so one registration makes a doorbell
  store signal the event the worker already waits on. `MockHost` records the
  connection and a test rings it with `ring_doorbell(index)`.
- **Attachment crosses threads through the work queue.** `RingConsumer::prepare`
  runs on the main loop during setup (validates the header, seeds
  `RingProgress`); the posted attach item flips `attached_` on the worker, so the
  worker only reads the window after the queue's mutex has published it. The
  worker drains posted work before serving the ring, so a doorbell that raced
  the attachment is not lost. The attach item has no completion.
- **Teardown order.** `DeviceContext` declares binding, ring, worker in that
  order: the worker is destroyed first, and `Worker::stop` disconnects the
  doorbell before it joins and destroys its event; the binding unpins last.
- **Head is the device's.** Snapshots are judged as
  `RingHeader(order, tail, device_head)`; the header's head field is only ever
  written. At setup the field must be 0.
- **Violations stop, log once.** A stopped consumer ignores doorbells;
  033 turns the stop into a fault.
- **No payload copy.** 032 copies batches before decoding.
- **Setup refusals** stay `ERR_INVALID_PARAMETER` (029's table), including a
  held doorbell, found by `DeviceContextTable`.

## Phases

1. Proto: `ContextSetup` doorbell, `BatchHeader`; tests, `fuzz_proto` seeds.
2. Memory and ring: `BlobWindow` (`GuestPageMapping`, `HostMemoryBlock`,
   `BlobResource`, `BlobTable::window`), `RingWindow`, `RingConsumer`; `fuzz_ring`.
3. Doorbell and worker: `Host` contract, `TableHost`, `MockHost`,
   `MockHostOps`; `Doorbell`, `WorkItem`, `Worker`; `DeviceContext` bind,
   `DeviceContextTable`, `ContextCommands`.
4. Glue: eventfd events, doorbell BAR and capability, connect/disconnect;
   `rvk_device.h` constants; qtest ring suite and the context suite's ring
   headers; `docs/protocol.md`, `docs/testing.md`; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| II | Pass - consumption and every check in core; the glue registers eventfds and maps the BAR |
| III | Pass - window, ring, consumer and doorbell are object classes with state; no static or free functions beyond one-line `extern "C"` routines |
| IV | Pass - every rule unit tested on a mock worker thread, hostile ring table and `fuzz_ring`, and against the real device |
| V | Pass - every read bounded by the pinned blob; every snapshot judged against the device's own history |
| VII | Pass - main loop and worker sides stated per method and checked by `MockHost` |
| VIII | Pass - no executor interface: nothing consumes payloads until 032 |
