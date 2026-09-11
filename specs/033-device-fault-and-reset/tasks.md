---
description: "Task list for feature 033: fault, reset and recovery"
---

# Tasks: QEMU device: fault, reset and recovery

Tests come first in every task and must fail before the code they cover exists.

## Phase 1: Reporting faults

- [X] T001 [US1] Tests then `FaultCode` in src/proto/submission/fault_code.hpp and `FenceReporter::fault`, `heartbeat`, `faulted` in src/device/fence/

**Checkpoint**: commit.

## Phase 2: Faulting contexts

- [X] T002 [US1] [US3] Tests then `BatchExecutor` taking the reporter: heartbeat per executed command, `fault()` naming the failure
- [X] T003 [US1] [US2] Tests then `RingConsumer` faults for every stop, executor destroyed, interrupt signalled; `MockContextExecution`, fuzz_ring, fuzz_command_stream, context and worker tests

**Checkpoint**: commit.

## Phase 3: QEMU

- [ ] T004 [US1] [US2] [US3] tests/qemu/test_virtio_gpu_faults.py; progress in tests/qemu/test_virtio_gpu_fences.py
- [ ] T005 Docs: docs/protocol.md (codes, progress, recovery), rvk_executor.h (calls return), docs/testing.md; `tools/gate.sh`; merge; push
