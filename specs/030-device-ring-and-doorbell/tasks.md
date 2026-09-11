---
description: "Task list for feature 030: submission ring and doorbell"
---

# Tasks: QEMU device: submission ring and doorbell

Tests come first in every task and must fail before the code they cover exists.

## Phase 1: Protocol

- [X] T001 [US3] Tests then the doorbell index in `ContextSetup` (offset 24, `validate_against` refuses an index at or above `max_contexts`) in src/proto/context/context_setup.{hpp,cpp}; update 029 callers and tests
- [X] T002 [P] [US1] Tests then `BatchHeader` (16 bytes: `RVKB`, u32 length, u64 fence id >= 1; `span` rounded to 8; `fits` a ring order) in src/proto/submission/batch_header.{hpp,cpp}; hostile table; seeds in fuzz_proto

**Checkpoint**: commit.

## Phase 2: Reading a ring

- [X] T003 [US1] Tests then `BlobWindow` in src/device/memory/blob_window.{hpp,cpp}; `GuestPageMapping::window`, `HostMemoryBlock` keeps its device view, `BlobResource::window`, `BlobTable::window`
- [X] T004 [US1] Tests then `RingWindow` in src/device/ring/ring_window.{hpp,cpp}: header snapshot, wrapped copies, head publication, reads bounded by the window
- [X] T005 [US1] [US2] Tests then `RingConsumer` in src/device/ring/ring_consumer.{hpp,cpp}: prepare, attach, service over complete, partial, wrapped and hostile rings, log once on stop; fuzz_ring with corpus

**Checkpoint**: commit.

## Phase 3: Doorbells

- [ ] T006 [US1] Tests then `Host::connect_doorbell`/`disconnect_doorbell` in `Host`, `TableHost`, `rvk_device.h`, `MockHost` (`ring_doorbell`, violations for a double connect, an unknown disconnect, an event destroyed while connected, a connection left at destruction), `MockHostOps`
- [ ] T007 [US1] Tests then `Doorbell` in src/device/threading/doorbell.{hpp,cpp}; `WorkItem` without completion; `Worker` serving its `RingConsumer` after posted work and disconnecting before join
- [ ] T008 [US3] Tests then `DeviceContext::bind` (pin, prepare, connect, attach, all-or-nothing), member order, `DeviceContextTable::holds_doorbell`, `ContextCommands` refusing a held doorbell; `VirtioGpuRvk` reset

**Checkpoint**: commit.

## Phase 4: QEMU

- [ ] T009 [US1] Glue: eventfd events, doorbell BAR 0 with shared memory capability 0x72, connect/disconnect in src/device/qemu/
- [ ] T010 [US1] [US2] [US3] qtest suite tests/qemu/test_virtio_gpu_ring.py; ring headers in tests/qemu/test_virtio_gpu_contexts.py
- [ ] T011 Docs: docs/protocol.md (doorbells, setup field, batches, consumption), docs/testing.md, README layout; `tools/gate.sh`; merge; push

## Dependencies

T001, T002 before T005. T003 before T004 before T005. T006 before T007 before T008. T008 before T009, T010.
