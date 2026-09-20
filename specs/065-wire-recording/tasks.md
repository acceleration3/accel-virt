---
description: "Tasks: wire recordings - capture, inspect and replay (065)"
---

# Tasks: wire recordings - capture, inspect and replay

**Input**: plan.md, spec.md, research.md, data-model.md, contracts/rvkrecord.md

**Tests**: written first and failing first (constitution IV). Every branch,
invariants after every mutation, and a hostile table plus a libFuzzer target
for the reader, because a recording read back is bytes this side did not
write. Commit at every **Checkpoint** once `ctest --test-dir build` passes.

**The one interpretation**: FR-005's "decoded arguments" is met by naming every
command from the generated id tables and decoding the transport commands in
full; a generated per-command argument printer is a change to `tools/codegen/`
and is recorded in research.md §5 as the follow-on. Nothing else in the spec is
reinterpreted.

## Phase 1: Foundational - the format

- [X] T001 [P] `RecordingHeader` (object) in `src/device/recording/recording_header.{hpp,cpp}`: the 64-byte file header of data-model.md - magic `RVKR` = `0x524B5652`, u16 major (1, another is refused), u16 minor (0, a higher one is accepted and read as far as it is known), u16 wire_major, u16 wire_minor, u32 kinds, u64 started, u64 limit (0 none, otherwise never below 64), u32 wraps, u32 vendor_id, u32 device_id, u32 reserved (0, rejected when not), u8[16] gpu_uuid; `encode`/`decode` returning `Status`, unchanged unless ok. Tests `tests/unit/device/recording/test_recording_header.cpp`: a round trip, and the hostile table - zero length, truncation at every field boundary, bad magic, a major of 0 and of 2, a non-zero reserved field, a limit of 63, and a byte-flip sweep of a valid header
- [X] T002 [P] `RecordHeader` (object) in `src/device/recording/record_header.{hpp,cpp}`: the 32-byte record header - magic `RVKD` = `0x444B5652`, u16 kind (1..7; another is refused), u16 reserved (0), u32 context, u32 length (at most `max_payload` = 16 MiB), u64 timestamp, u64 fence; `span()` is `32 + length` and `fits(uint64_t remaining)` says whether the record is wholly within what is left. Tests `tests/unit/device/recording/test_record_header.cpp`: the same hostile table, kind 0 and kind 8, a length of `max_payload` and of `max_payload + 1`, and `span()` at the length boundary
- [X] T003 [P] `RecordKinds` (object) in `src/device/recording/record_kinds.{hpp,cpp}`: the mask of kept kinds, `keeps(RecordKind)`, `mask()`, `parse(const char *list)` over the comma-separated names `setup,batch,reply,fence,fault,blob,blob_bytes` returning `Status` (an unknown name is `invalid_parameter` and leaves the mask alone), and the default - every kind but `blob_bytes`. Tests `tests/unit/device/recording/test_record_kinds.cpp`: the default, each name, a list with an unknown name, an empty list, a list of only separators, and the invariant
- [X] T004 [P] `RecordingLimit` (object) in `src/device/recording/recording_limit.{hpp,cpp}`: the ceiling, the bytes written and whether it wraps; `verdict(uint64_t bytes)` answers `fits`, `stop` or `wrap` for the next record, `took(uint64_t bytes)` and `wrapped()` move it, overflow-checked. A ceiling of 0 is no ceiling. Tests `tests/unit/device/recording/test_recording_limit.cpp`: a record that exactly reaches the ceiling, one that passes it by a byte, a record larger than the whole ceiling (`stop` even when wrapping - there is nowhere to put it), a wrap resetting the count to the header's size, `bytes + written` overflowing `uint64_t`, and the invariant after every mutation
- [X] T005 The format's normative tables in `docs/protocol.md`: a "Wire recordings" section after the fence-queue section carrying the file header, the record header and the kind table of data-model.md, in the document's own style (FR-002)

**Checkpoint**: commit. Nothing records yet; the bytes are defined and refuse everything malformed.

## Phase 2: Foundational - the file

- [X] T006 Four operations on `Host` (`src/device/host/host.hpp`): `Status create_recording(const char *path, void *&file)` (main loop), `Status append_recording(void *file, const uint8_t *bytes, size_t size)` (any thread; writes and flushes), `Status truncate_recording(void *file)` (any thread), `void close_recording(void *file)`; and their `RvkHostOps` members in `src/device/abi/rvk_device.h`, forwarded by `TableHost` in `src/device/abi/table_host.{hpp,cpp}`. Tests `tests/unit/device/abi/test_table_host.cpp`: each forwards, and a table whose call refuses becomes the right `Status`
- [X] T007 `MockHost` (`tests/harness/host/mock_host.{hpp,cpp}`) implements the four over memory: the bytes appended to each open recording, readable by a test through `recording_bytes(path)`/`recording_length(path)`, with `fail_recording_create_at`, `fail_recording_append_at` and `fail_recording_truncate_at` for the failure sweep, and a destructor that fails the test on a recording never closed. Its own tests in `tests/unit/harness/test_mock_host.cpp`
- [X] T008 The four `RvkHostOps` members in `src/device/qemu/virtio-gpu-rvk-host.c`: `open(path, O_WRONLY|O_CREAT|O_TRUNC, 0644)` with the descriptor biased so NULL is never a file, `qemu_write_full` then `fdatasync`, `ftruncate` then `lseek`, and `qemu_close`. `PosixRecordingFile` is not built: with the glue doing this in C there is nothing left for it to own (plan.md)

**Checkpoint**: commit. A recording can be opened and written; nothing writes one.

## Phase 3: US1 - a run leaves a recording (P1)

**Goal**: the device records what crosses the wire, flushed before the executor sees it, bounded, off by default.

**Independent Test**: the device core over `MockHost` records a context's setup, three batches, a reply and a fault; the bytes read back hold each record in order with the payload the executor received.

- [X] T009 [US1] `WireRecorder` (system) in `src/device/recording/wire_recorder.{hpp,cpp}`: `Host&`, the open file, the `RecordingHeader`, the `RecordingLimit`, the `RecordKinds`, a mutex and the count of records refused. `open(path, limit, kinds, wrap, identity, wire_version)` on the main loop, `close()`, `recording()`, and one `record_*` per kind, each returning at once when no file is open. Every write takes the mutex, asks the limit, wraps or stops, appends the record header then the payload in one call, and logs once - never per record - when it first stops or first wraps. Tests `tests/unit/device/recording/test_wire_recorder.cpp` written first: a closed recorder writes nothing and touches no host call; an open one writes header then records; the limit stops and logs once; wrapping rewrites the header with `wraps` incremented and the file back to 64 bytes; a `create_recording` that refuses leaves recording off, logs the path once, and `open` answers ok so no run is refused (the spec's directory-not-writable edge case); an `append_recording` that refuses stops recording and logs once; a kind the mask excludes is not written; invariants after every mutation
- [X] T010 [US1] The recording properties reach the core: `RvkRecordingOptions` (`path`, `limit_bytes`, `kinds`, `rotate`) in `src/device/abi/rvk_device.h`, a parameter of `rvk_device_create`; `VirtioGpuRvk` (`src/device/module/virtio_gpu_rvk.{hpp,cpp}`) owns the `WireRecorder`, opens it at `realize` once the GPU is bound - so the header carries the bound GPU's identity and the negotiated wire version - and closes it on destruction; `DeviceModule`/`DeviceInstance` pass the options through, and `src/device/qemu/virtio-gpu-rvk.c` gains the `record`, `record-limit`, `record-kinds` and `record-wrap` properties (contracts/rvkrecord.md). Tests `tests/unit/device/module/test_virtio_gpu_rvk.cpp`: a device realized with a path records, one without does not, a bad `record-kinds` fails `realize` with the reason in its message, and an unrealized device records nothing
- [X] T011 [US1] Context setup is recorded: `DeviceContext::bind` (`src/device/context/device_context.cpp`) records kind 1 with the encoded `ContextSetup` and the context's id once the binding succeeded, never for a bind that failed. Tests `tests/unit/device/context/test_device_context.cpp`: the record's bytes decode back to the setup that was bound, and a refused bind leaves the recording empty
- [X] T012 [US1] Batches, replies and faults are recorded: `BatchExecutor` (`src/device/execution/batch_executor.{hpp,cpp}`) records kind 2 - the staged payload exactly as the executor will receive it, with the batch's fence - **before** the first command runs (US1 scenario 3); kind 3 when it writes the reply slot, carrying the answer bytes; kind 5 when a run fails, carrying the `FaultCode`. The recorder is injected at construction, so a batch's record costs one branch when recording is off. Tests `tests/unit/device/execution/test_batch_executor.cpp`: the batch record is in the recording before the mock executor's first `execute` returns (the mock asserts it while it runs), a reply record matches the bytes in the slot, a fault record carries the code, and with recording off no host call is made and the payload is not copied twice
- [X] T013 [US1] Fence completions are recorded: `FenceReporter` (`src/device/fence/fence_reporter.{hpp,cpp}`) records kind 4 with the completed fence and its context. Tests `tests/unit/device/fence/test_fence_reporter.cpp`
- [X] T014 [US1] Blob lifetimes are recorded: a blob created or released in `src/device/control/blob_commands.cpp` and an import or release in `src/device/execution/blob_imports.cpp` record kind 6 with action, memory type, blob id, offset and size; when `blob_bytes` is in the mask, an import also records kind 7 - u64 blob id, u64 offset, then at most `max_payload - 16` bytes of the imported range, with a single log line and no record when it is larger. Tests `tests/unit/device/execution/test_blob_imports.cpp` and `tests/unit/device/control/test_blob_commands.cpp`
- [X] T015 [US1] Ordering and concurrency: `tests/unit/device/recording/test_wire_recorder.cpp` gains a case where several threads record at once and every record read back is whole and in the order the recorder took it, and `tests/unit/device/module/test_virtio_gpu_rvk.cpp` gains the US1 independent test - a setup, three batches, a reply and a fault read back in submission order across two contexts, each with a timestamp that does not go backwards
- [X] T016 [US1] The allocation- and refusal-failure sweep for everything Phase 3 added: every `MockHost` recording call failed in turn leaves no crash, no leak, and a device that still runs

**Checkpoint**: commit. A run leaves a recording; with recording off nothing changes.

## Phase 4: US2 - a recording is readable (P1), the reader

**Goal**: a fuzzed reader that yields every whole record before the damage and names where the damage is.

**Independent Test**: a recording cut at any offset yields every whole record before the cut and reports the cut's offset; the reader never crashes under ASan and UBSan.

- [X] T017 [US2] `RecordingReader` (system) in `src/device/recording/recording_reader.{hpp,cpp}`: the bytes and their length (both the caller's), the decoded `RecordingHeader`, the read offset and the damage offset with its reason. `open()` decodes the file header; `next(RecordHeader&, const uint8_t *&payload)` answers `ok`, `not_found` at a clean end, or `malformed` with `damage_offset()` and `damage_reason()` set. Every length is checked against the bytes the reader owns and every offset addition is overflow-checked (constitution V). Tests `tests/unit/device/recording/test_recording_reader.cpp` written first: a recording of each kind walks; truncation at every field boundary of the file header and of every record; a record length one past the end; a length of `max_payload + 1`; an unknown kind; a header claiming a major nobody wrote; zero bytes; a file of exactly 64 bytes; a byte-flip sweep of a valid recording; and the invariant after every `next`
- [X] T018 [US2] `tests/fuzz/fuzz_recording_reader.cpp`: the input is a recording, walked to its end or its damage, every payload read. Seed corpus committed under `tests/corpus/fuzz_recording_reader/` - an empty file, a header alone, one of each record kind, a wrapped recording, a truncated one, and a recording holding a real Vulkan batch

**Checkpoint**: commit. `ctest --test-dir build-fuzz -R fuzz_recording_reader_smoke` passes.

## Phase 5: US2 - a recording is readable (P1), the printer

**Goal**: `rvkrecord print` shows each record with its command names and filters.

**Independent Test**: a recording made by the device core prints with command names and decoded transport arguments; each filter keeps only what it names; a corrupted recording prints up to the damage and says where it is.

- [X] T019 [P] [US2] `RecordedCommand` (object) in `src/device/recording/recorded_command.{hpp,cpp}`: one command inside a batch record - its `CommandHeader`, its payload within the record, and `name()` from the generated table for its API (`vk_command_names.h`, `cl_command_names.h`, `gl_command_names.h`; `transport` ids named here because this repository owns them). Tests `tests/unit/device/recording/test_recorded_command.cpp`: a Vulkan id names `vkCreateInstance`, an unknown id names `unknown`, a payload shorter than its header is refused, and a command whose size passes the batch's end is refused
- [X] T020 [P] [US2] `RecordingFilter` (object) in `src/device/recording/recording_filter.{hpp,cpp}`: a context (`any_context`), a sub-API (`any_api`), a fence range and a command-name fragment; `keeps(const RecordHeader&)` and `keeps(const RecordedCommand&)`. Tests `tests/unit/device/recording/test_recording_filter.cpp`: each filter alone and in combination, a fence range whose end is below its start, an empty fragment, and a fragment longer than any name
- [X] T021 [US2] `RecordingPrinter` (system) in `src/device/recording/recording_printer.{hpp,cpp}`: the reader, the filter, a `TextBuffer&` the caller owns and the `--bytes` budget; `print()` walks the reader and appends the lines of contracts/rvkrecord.md - the setup's fields, the batch's commands one indented line each, the reply's length, the fence, the fault's code, the blob's action and range, the decoded transport arguments, the hex of at most `--bytes` payload bytes, and the damage line last. Tests `tests/unit/device/recording/test_recording_printer.cpp` written first: the exact lines for a recording of every kind; a batch whose commands are framed wrong printing what it could and naming the framing; filters; `--bytes` at 0, at less than a payload and at more; a line buffer too small cutting rather than overrunning; and the damage line with its offset
- [X] T022 [US2] The round trip: `tests/unit/device/recording/test_recording_printer.cpp` prints a recording that `WireRecorder` wrote in Phase 3 over `MockHost` - not one a test hand-built - so a change to either side that breaks the other fails here

**Checkpoint**: commit. A recording is readable by a person.

## Phase 6: US3 - a recording replays (P1)

**Goal**: `rvkrecord replay` drives a recording into an executor library and names every answer that differs.

**Independent Test**: a recording replays into `MockExecutorOps` and into the real library, contexts created in recorded order, and a deliberately altered answer is reported by fence and command.

- [X] T023 [P] [US3] `ReplayContext` (object) in `src/device/recording/replay_context.{hpp,cpp}`: a recorded context id, its `RvkExecutor*`, its sub-API, and the last answer it produced with its length; `created()`, `execute(...)` through the library, `compare(const uint8_t *recorded, size_t length)`. Tests `tests/unit/device/recording/test_replay_context.cpp`: an answer that matches, one that differs in length, one that differs in a byte, an answer larger than the buffer, and the invariant
- [X] T024 [US3] `RecordingReplay` (system) in `src/device/recording/recording_replay.{hpp,cpp}`: `ExecutorLibrary&`, a fixed table of `ReplayContext`, the GPU UUID asked for (the recording's own when none is given), the findings and their count. `run(RecordingReader&, const RecordingFilter&)` creates an executor per setup record, runs every command of every batch record in order, compares each reply record, reports `unreplayable` for a context that imported guest memory, which this replay cannot place (contracts/rvkrecord.md), and destroys every executor before it returns. Tests `tests/unit/device/recording/test_recording_replay.cpp` written first: a clean recording replays with no finding; an altered answer is one finding naming the fence and the command index; a library that refuses `create` is a finding, not a crash; a library that answers `RVK_UNKNOWN` or `RVK_DEVICE_LOST` stops that context and says so; more contexts than the table holds is a finding; a batch whose commands are framed wrong is a finding; and the recording that crashed its host stops at the same command (a mock library that refuses to return past command N)
- [X] T025 [US3] `tests/loopback/test_recording_replay.cpp`: the loopback rig runs a real guest-core-to-device-core rvk scenario with recording on, then `RecordingReplay` replays the recording into `MockExecutorOps` and the replies match - the record-then-replay round trip over the real wire

**Checkpoint**: commit. A recording replays without a guest.

## Phase 7: the tool, the run and the record

- [X] T026 [P] `MappedFile` (object) in `src/device/shim/mapped_file.{hpp,cpp}`: a file mapped read-only with `mmap`, its bytes and length, unmapped with the object; an empty or unreadable file leaves it holding nothing
- [X] T027 [P] `PosixExecutorModule` (object) in `src/device/shim/posix_executor_module.{hpp,cpp}`: `dlopen(path, RTLD_NOW|RTLD_LOCAL)`, the table `rvk_executor_ops` answers, closed with the object; null when it does not open or answers nothing
- [X] T028 `RecordingToolRequest` (object) in `src/device/recording/recording_tool_request.{hpp,cpp}`: what the tool was asked to do, refusing every argument it does not understand with a reason; then `main` in `src/device/shim/recording_tool.cpp`: parses `print`/`replay`, the filters and the flags of contracts/rvkrecord.md, maps the recording, builds the reader, the filter and the printer or the replay, writes the lines to standard output and answers 0, 1 or 2. The `rvkrecord` target in `CMakeLists.txt`, linking `av_core` and `${CMAKE_DL_LIBS}`
- [X] T029 `tests/tools/test_rvkrecord.py`: the built tool over recordings written by the unit tests' fixtures - a clean recording prints and exits 0, a truncated one prints what it can, names the offset and exits 1, a usage error exits 2, and `replay` against `librvk_echo_executor.so` reports no difference. Registered as a `tools_test_rvkrecord` ctest
- [X] T030 `run_guest_suite.py --record` (`tools/guest/run_guest_suite.py`): sets `record=<work>/recording.rvk` and `record-limit` on the `-device virtio-gpu-rvk-pci` argument, and the run's summary names the file beside `qemu.log`. Tests in `tests/tools/test_run_guest_suite.py`: the property is on the command line with `--record` and absent without it
- [X] T031 SC-002: the no-op round trip of feature 044 measured with recording off and on, reported by the rvk scenario, so the cost of a recording is a number rather than a claim *As built:* two boots of the rvk scenario into one work directory on the real guest, an AMD RX 9070 XT host: **424 us with recording off, 6792 us with it on - a recording costs 6368 us**, written to `recording-cost.txt`. The warm median of 31 timed round trips, not the cold first one, which measured 899 us in the same boot and swings by milliseconds. SC-002's first half holds: 424 us is inside the spread the off side has always shown (2066 us on a busy host earlier the same day), and far inside the runner's 50 ms limit. Its second half is the point and is now answered. The number is worse than it looks and better than it reads: a no-op is the cheapest command there is, so a per-record cost lands entirely on it - 16x here is the worst case, not the typical one. Whether 6.4 ms per record is a synchronous write worth batching is feature 062's question, not this one's; SC-002 asked only that the cost stop being a claim.
- [X] T032 The documentation the change makes wrong: `README.md`'s layout gains `src/device/recording/` and the `rvkrecord` tool, `docs/testing.md` gains the recording's place in the layers, and `docs/trace.md` is left alone - a recording is not a trace
- [X] T033 The full gate: `tools/gate.sh` - `design_rules`, `symbol_tests`, `scope_marks`, `self_contained`, `driver_image`, the tools modules, 100% branch coverage of `src/device/recording/` with the `AV_UNTESTABLE` ratchet unmoved, and the 60-second fuzz smoke

**Checkpoint**: commit. The feature is done.

## Dependencies

- Phase 1 (the format) blocks everything.
- Phase 2 (the file) blocks Phase 3; T006 blocks T007 and T008.
- Phase 3 (capture) blocks Phase 4's fixtures (T022, T025 read what it wrote).
- Phase 4 (the reader) blocks Phases 5 and 6.
- Phases 5 and 6 are independent of each other once Phase 4 is done.
- Phase 7 depends on 5 and 6.

## Parallel opportunities

- T001-T004 are four files and four tests, independent.
- T019 and T020 are independent; T023 is independent of both.
- T026 and T027 are independent.

## What changed against the plan

- `PosixRecordingFile` was not built (T008): the QEMU glue does the four file
  calls in C, leaving it nothing to own.
- `ContextApi` (T021) and `RecordingToolRequest` (T028) were added: the first is
  what lets `--api` keep a context's replies and fences, the second is the
  decision of what the tool was asked to do, which belongs in core rather than
  in `main`.
- Replay reports a context that imported guest memory as unreplayable rather
  than importing the `blob_bytes` snapshot (T024): placing a snapshot needs
  page-aligned host memory the replay would have to own, and the recording
  carries the bytes for a replay that can. `contracts/rvkrecord.md` says so.
- A second fuzz target was added after the tests were audited against
  principle IV: `tests/fuzz/fuzz_recording_printer.cpp`. The reader frames
  records, but a batch record's payload is the guest's own framed commands, and
  the printer is what walks those - bytes it did not write, which IV.3 asks for
  a target of. Its corpus is the reader's.
- SC-002 (T031) is a measurement no unit test can make, so the feature builds
  the thing that makes it and the guest run supplies the number. The rvk
  scenario reads the transport scenario's warm median, keeps it in
  `recording-cost.txt` in the work directory under the side that run was, and
  the boot that completes the pair says what a recording cost
  (`tools/recording_cost.py`, `RecordingCost`; `tests/tools/test_recording_cost.py`
  and the four `recording cost` cases of `tests/tools/test_run_guest_suite.py`).
  The median is what is kept rather than the first round trip, which is cold and
  swings by milliseconds - orders of magnitude past one `write` a batch. The two
  commands are in quickstart.md, and T031 above holds what they answered. That a
  recording costs one branch when it is off is asserted by
  `batch_executor_run.with_recording_off_it_asks_the_host_for_no_recording_at_all`.
