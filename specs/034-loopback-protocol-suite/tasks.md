---
description: "Task list for feature 034: loopback protocol test suite"
---

# Tasks: Loopback protocol test suite

Tests come first in every task and must fail before the code they cover exists.

## Phase 1: The driver's requests and answers

- [X] T001 [US1] Tests then `RvkRequestWriter` in src/proto/virtio_gpu/rvk_request_writer.{hpp,cpp}: capset info, capset, create and destroy context, guest blob of one entry, unref, setup submit
- [X] T002 [US3] Tests then `GpuResponseReader` in src/proto/virtio_gpu/gpu_response_reader.{hpp,cpp}; hostile table; fuzz_gpu_response with corpus

**Checkpoint**: commit.

## Phase 2: Capset and context setup

- [ ] T003 [US1] [US3] Tests then `CapsetQuery` in src/core/rvk/
- [ ] T004 [US1] Tests then `ContextMemory` in src/core/rvk/ (allocation-failure sweep)
- [ ] T005 [US1] [US3] Tests then `ContextRequestSequence` in src/core/rvk/: setup, teardown, partial teardown, refusals

**Checkpoint**: commit.

## Phase 3: Submission and completion

- [ ] T006 [US2] [US3] Tests then `RingSubmitter` in src/core/rvk/
- [ ] T007 [US1] [US3] Tests then `FenceObserver` in src/core/rvk/
- [ ] T008 [US1] [US2] [US3] Tests then `GuestContext` in src/core/rvk/; `LockRank::rvk_context`; fuzz_guest_context with corpus

**Checkpoint**: commit.

## Phase 4: Loopback

- [ ] T009 [US1] Harness: `MockHost` guest memory translator; `LoopbackDevice` in tests/harness/loopback/; CMake builds tests/loopback/test_*.cpp
- [ ] T010 [US1] [US2] [US3] tests/loopback/: capset and versions, a context end to end, backpressure, faults and recovery, reset, hostile guest, hostile device
- [ ] T011 Docs: docs/testing.md (L4 as built), docs/protocol.md (the driver's sequence), README; `tools/gate.sh`; merge; push
