---
description: "Task list for feature 031: fence pages and interrupt injection"
---

# Tasks: QEMU device: fence pages and interrupt injection

Tests come first in every task and must fail before the code they cover exists.

## Phase 1: Reporting

- [X] T001 [US2] Tests then the fence notifier contract in `Host`, `TableHost`, `rvk_device.h`, `MockHost` (`fail_fence_connect_at`, `raised(line)`, violations), `MockHostOps`
- [X] T002 [US2] Tests then `FenceInterrupts` in src/device/fence/fence_interrupts.{hpp,cpp}: initialize, start, stop, raise gated by the mutex, from worker threads
- [X] T003 [US1] Tests then `FenceWindow` and `FenceReporter` in src/device/fence/: prepare writes the fresh page, complete, signal once, withdraw

**Checkpoint**: commit.

## Phase 2: Completion

- [ ] T004 [US1] [US2] Tests then `RingConsumer` completing each batch through its reporter and signalling once per service; fuzz_ring checks the page
- [ ] T005 [US1] [US2] Tests then `ContextBinding::fence_window`, `DeviceContext` bind order and withdrawal, `DeviceContextTable` and `VirtioGpuRvk` owning `FenceInterrupts`, `status`, reset order, realize failure; `rvk_device_status`

**Checkpoint**: commit.

## Phase 3: QEMU

- [ ] T006 [US2] Glue: fence queues, set_status, notifier ops, unrealize in src/device/qemu/
- [ ] T007 [US1] [US2] qtest: MSI-X helpers in tests/qemu/virtio_qtest.py; tests/qemu/test_virtio_gpu_fences.py
- [ ] T008 Docs: docs/protocol.md, docs/testing.md, README layout; `tools/gate.sh`; merge; push
