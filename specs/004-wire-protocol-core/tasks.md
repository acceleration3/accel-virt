---
description: "Task list for feature 004: wire protocol core"
---

# Tasks: Wire protocol core

## Phase 1: Byte I/O (foundational)

- [X] T001 Add `Status::malformed` to `src/core/status.hpp`
- [X] T002 Write `tests/unit/test_wire_reader.cpp` and `tests/unit/test_wire_writer.cpp`: little-endian u8/u16/u32/u64 at exact offsets; read past end latches `malformed` and later reads return 0 without advancing; null pointer with length treated as empty; `expect_zero` latches on a non-zero byte; `copy_bytes` bounds; writer past capacity latches `too_large`, writes nothing past capacity (GuardedBuffer), later writes no-ops; `invariants_hold` after every call. Must fail to compile
- [X] T003 Implement `src/proto/wire_reader.{hpp,cpp}` and `src/proto/wire_writer.{hpp,cpp}`. T002 passes

**Checkpoint**: commit.

## Phase 2: Structures (US1, US2, US4)

- [X] T004 [P] Tests for `ProtocolVersion` (compatibility, negotiation) and `GpuIdentity`/`CapsetInfo` (offsets of every field, round trip, each malformed rule including name NUL rules and unknown feature bits tolerated, atomic decode, hostile sweep) in `tests/unit/test_protocol_version.cpp`, `test_gpu_identity.cpp`, `test_capset_info.cpp`
- [X] T005 [P] Tests for `ContextSetup` (offsets, round trip, malformed rules, `validate_against(CapsetInfo)` not_supported cases, hostile sweep) in `tests/unit/test_context_setup.cpp`
- [X] T006 [P] Tests for `RingHeader`, `SubmissionDescriptor`, `FencePage` (offsets, round trip, malformed rules, order bounds 12/24, `SubmissionDescriptor::validate_against(RingHeader)`, hostile sweeps) in `tests/unit/test_ring_header.cpp`, `test_submission_descriptor.cpp`, `test_fence_page.cpp`
- [X] T007 Implement `src/proto/protocol_version`, `sub_api.hpp`, `gpu_identity`, `capset_info`, `context_setup`, `ring_header`, `submission_descriptor`, `fence_page`. T004-T006 pass

**Checkpoint**: commit.

## Phase 3: Trackers (US3)

- [X] T008 Tests for `RingProgress` and `FenceProgress` in `tests/unit/test_ring_progress.cpp`, `test_fence_progress.cpp`: every acceptance scenario of US3, counters near 2^64, last good observation kept on rejection, invariants after every call
- [X] T009 Implement `src/proto/ring_progress.{hpp,cpp}`, `src/proto/fence_progress.{hpp,cpp}`. T008 passes

## Phase 4: Fuzzing and polish

- [X] T010 `tests/fuzz/fuzz_proto.cpp` (selector byte, decode, round-trip trap) with seed corpus `tests/corpus/fuzz_proto/` holding one valid encoding per structure
- [X] T011 `tools/gate.sh`; merge to `master`
