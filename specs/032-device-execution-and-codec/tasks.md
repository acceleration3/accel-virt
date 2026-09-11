---
description: "Task list for feature 032: decode and execute"
---

# Tasks: QEMU device: decode and execute

Tests come first in every task and must fail before the code they cover exists.

## Phase 1: Protocol

- [X] T001 [US3] Tests then the reply resource in `ContextSetup` (offset 28; 0 or distinct from ring and fence)
- [X] T002 [P] [US1] Tests then `CommandHeader` (8 bytes; size >= 8 in the low 24 bits, flags top 8 with bit 0 known; api = id >> 16) and `ReplyHeader` (32 bytes: `RVKY`, status, fence, size, required) in src/proto/submission/; fuzz_proto seeds

**Checkpoint**: commit.

## Phase 2: Executors

- [X] T003 [US1] `src/device/abi/rvk_executor.h`; tests then `MockExecutorOps` (harness), `ExecutorLibrary`, `ContextExecutor` in src/device/execution/
- [X] T004 [US1] Tests then `DeviceCapset` from the library, `VirtioGpuRvk`/`DeviceModule`/`rvk_device_create` taking the table, no capset without one

**Checkpoint**: commit.

## Phase 3: Execution

- [X] T005 [US1] [US2] Tests then `CommandStream`, `BatchStaging`, `BatchExecutor`; fuzz_command_stream with corpus
- [X] T006 [US1] [US2] [US3] Tests then `RingConsumer` executing before completing, `Worker` detach, `ContextBinding` reply pin, `DeviceContext` staging and executor, `ContextCommands` reply errors

**Checkpoint**: commit.

## Phase 4: QEMU

- [ ] T007 [US1] Glue: `executor` property, dlopen, finalize order; echo executor library and CMake; qtest device strings
- [ ] T008 [US1] [US2] tests/qemu/test_virtio_gpu_execution.py; smoke realize failures for a bad executor
- [ ] T009 Docs: docs/protocol.md, docs/testing.md, README; `tools/gate.sh`; merge; push
