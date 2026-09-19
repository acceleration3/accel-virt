# Feature Specification: Wire recordings: capture, inspect and replay

**Feature Branch**: `065-wire-recording`

**Created**: 2026-09-14

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 065 · **Status**: `implement` · **Depends on**: 042, 063

The device records what crosses the wire - each context's setup, every batch
as the executor received it, replies, fences, faults and blob lifetimes - to a
file on the host, flushed so a crash keeps what led to it, switched on per run
and bounded in size. A host tool prints a recording with command names and
decoded arguments, and replays it into an executor library, with the Khronos
validation layers or another GPU, so a rendering mistake, a crash or a
validation error is reproduced without a guest.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### Note

Requested 2026-09-14 for debug analysis of rendering mistakes, crashes and
validation. Numbered last because it was identified last; it depends only on
the executor and the generated wire, so the roadmap can schedule it right
after 044.

### Design rules — binding on this feature

Constitution Principle III and [`CLAUDE.md`](../../CLAUDE.md). Restated here because it is checked at review, not assumed:

- **Every unit of behaviour is a class that owns state.** No static member functions. No stateless class used as a namespace. No free function outside the two ABI boundaries (`DxgkDdi*` entry points in `src/core/ddi/`, and `extern "C"` entry points), each one line with no logic.
- If functions thread the same handle, buffer, bounds or platform through their parameters, **that parameter is a member**. Make it a class and inject its dependencies at construction.
- Name the classes this feature adds before writing them, and say which kind each is: an **object class** (one instance per real thing) or a **system class** (a long-lived service owning a collection).
- Dependencies injected at construction. No global, no singleton, no file-scope state.
- Data members private, trailing underscore. Classes `PascalCase`, methods and locals `snake_case`. No `Manager`, `Helper`, `Utils`, `Handler`, `Impl`, `process()`, `handle()`.
- One class per header, named for it. A header declares; it does not define. One responsibility per class, statable without an "and".
- Errors are `av::Status` values. No exceptions, no RTTI, no C++ standard library, no `new`/`delete` (placement new only in `ObjectStorage`). Cleanup is RAII via `ScopeExit`, never repeated per branch.
- Fallible construction is two-phase: trivial constructor, `initialize(...)` returning `Status`.
- Every public method declares its IRQL ceiling via `platform_.require_irql_at_most(...)`.

### Classes this feature introduces

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `RecordingHeader` | object | magic and version, the wire version recorded, the kinds kept, the ceiling, the wrap count, the start clock, the host GPU's identity | The first bytes of a recording |
| `RecordHeader` | object | kind, context, fence, payload length, timestamp | The framing of one record |
| `RecordKinds` | object | the mask of record kinds kept | Which kinds a recording keeps |
| `RecordingLimit` | object | the ceiling, the bytes written, whether it wraps | What happens when the next record would pass the ceiling |
| `WireRecorder` | system | `Host&`, the open file, the header, the limit, the kinds, a mutex, what it refused | Writes what crosses the wire to the recording |
| `RecordingReader` | system | the bytes and their length, the file header, the read offset, where the damage is | Walks a recording's records, stopping at the first damage |
| `RecordingFilter` | object | a context, a sub-API, a fence range, a command-name fragment | Whether a record is shown |
| `RecordedCommand` | object | one command's header and payload inside a batch record | One command of a recorded batch |
| `RecordingPrinter` | system | the reader, the filter, the line buffer, the byte budget | Prints a recording as lines a person reads |
| `ReplayContext` | object | a recorded context id, its executor, its sub-API, its last answer | One recorded context, replayed on a library |
| `RecordingReplay` | system | `ExecutorLibrary&`, the replay contexts, the GPU asked for, the findings | Replays a recording into an executor library |
| `PosixRecordingFile` | object | a file descriptor | A recording file on this host |
| `MappedFile` | object | the mapping and its length | A file's bytes, mapped read-only |
| `PosixExecutorModule` | object | the opened library and its table | An executor library loaded by path |

### Test requirements

Constitution Principle III applies in full. Concretely, this feature is not done until:

- every branch of every function it adds to `src/core`, `src/device` or `src/proto` is covered, or marked `AV_UNTESTABLE` with a justification;
- each stateful module it adds exports an invariant function that tests call after **every** mutation;
- every function it adds that reads bytes from outside its own address space has a hostile-input table test and a libFuzzer target with a committed corpus;
- the allocation-failure sweep passes: with each allocation site failed in turn, no crash and no leak;
- IRQL annotations are present and the mock's IRQL checker is satisfied;
- `tools/check_symbol_tests.py` reports no untested exported symbol.

### Reference material

Knowledge base:

- [`docs/kb/11-implementation-plan.md`](../../docs/kb/11-implementation-plan.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## User Scenarios & Testing *(mandatory)*

Users: whoever is debugging a guest application that renders wrong, a host
driver that crashes inside QEMU, or a stream the Khronos validation layers
would reject - today the only evidence is a guest screenshot and the device's
log, and the bytes that caused it are gone.

### User Story 1 - A run leaves a recording of what crossed the wire (Priority: P1)

**Independent Test**: the device core over MockHost records a context's setup,
three batches, a reply and a fault; the file read back holds each record in
order with the bytes the executor received.

**Acceptance Scenarios**:

1. **Given** a device started with recording on, **When** a guest sets up a
   context and submits batches, **Then** the recording holds the device's
   identity and protocol version, the context's sub-API and setup, and each
   batch's fence and command bytes exactly as the executor library received
   them, in submission order across contexts, with a timestamp each.
2. **Given** a batch that generates a reply, a completed fence, a fault or a
   blob created, imported or released, **When** it happens, **Then** it is a
   record too, naming its context.
3. **Given** the QEMU process crashes inside a host driver, **When** the
   recording is read afterwards, **Then** it holds every batch handed to the
   executor before the crash, up to and including the one it was running.
4. **Given** recording is off (the default), **When** a run happens, **Then**
   nothing is written and the submission path does no extra copy.
5. **Given** a size limit, **When** the recording reaches it, **Then** it stops
   or rotates as configured and says so in the device log, never growing past
   the limit.

### User Story 2 - A recording is readable by a person (Priority: P1)

**Independent Test**: a recording made by the loopback rig prints with command
names and decoded arguments, and a truncated or corrupted recording prints up to
the damage and says where it is.

**Acceptance Scenarios**:

1. **Given** a recording, **When** the inspect tool prints it, **Then** each
   command shows its API, generated command name (vkCreateInstance, glClear,
   clEnqueueNDRangeKernel...) and its decoded arguments, and each reply, fence
   and fault is shown against the batch it belongs to.
2. **Given** filters for a context, a sub-API, a fence range or a command name,
   **When** printing, **Then** only matching records are shown.
3. **Given** a recording cut short or with a corrupted record, **When** it is
   read, **Then** every whole record before the damage is shown and the damage
   is reported with its offset; the reader never crashes (it is a fuzzed parser).

### User Story 3 - A recording replays without a guest (Priority: P1)

**Independent Test**: a recording of the rig's Vulkan and OpenCL scenarios
replays into the executor library on this machine's GPU and produces the same
replies; with the validation layers on, a stream with a deliberate misuse
reports it.

**Acceptance Scenarios**:

1. **Given** a recording, **When** it is replayed into an executor library,
   **Then** each context is set up again and its batches run in the recorded
   order, and each reply is compared with the recorded one, differences named by
   fence and command.
2. **Given** the Khronos validation layers enabled for the replay, **When** a
   recorded Vulkan stream misuses the API, **Then** the validation message is
   reported against the fence and command that caused it.
3. **Given** a recording made on one host GPU, **When** it is replayed on
   another or with another library, **Then** it runs there, so a crash or wrong
   result can be bisected between drivers.
4. **Given** a recording that crashed its host, **When** it is replayed, **Then**
   it stops at the same command, reproducing the crash outside QEMU for a
   debugger.

### Edge Cases

- A hostile guest fills its batches with garbage: the recording holds garbage,
  bounded like any batch; the reader and replayer treat every byte of a
  recording as hostile.
- Host-visible and guest-memory blob contents are not in the wire: the
  recording names the blobs and, when asked, snapshots a blob's bytes at import
  so a replay can recreate it.
- Several contexts submit concurrently from different workers: records are
  ordered by the time the device took them and never interleave inside one record.
- The recording directory is not writable: the device starts with recording off
  and says why; a run is never refused because of it.

## Requirements *(mandatory)*

- **FR-001**: Recording is a device property (`record=<path>`, a size limit, and
  which record kinds to keep), off by default; `run_guest_suite.py --record`
  sets it and the recording lands in the run's work directory beside `qemu.log`.
- **FR-002**: The recording format is defined in `docs/protocol.md` beside the
  wire it records: a versioned file header, then length-prefixed typed records.
- **FR-003**: Writing is on the device's own thread off the executor's hot path,
  flushed per batch before the batch is handed to the executor library.
- **FR-004**: The reader is portable core, fuzzed, and shared by the inspect and
  replay tools; neither tool trusts a recording.
- **FR-005**: The inspect tool names commands from the generated id tables and
  decodes arguments with the generated decoders, never a hand-written table.
- **FR-006**: Replay drives the executor library through the same `rvk_executor.h`
  ABI the device uses.

## Success Criteria *(mandatory)*

- **SC-001**: A guest run with `--record` produces a recording that the inspect
  tool prints and the replay tool replays with identical replies.
- **SC-002**: With recording off, the submission path's cost is unchanged within
  measurement noise; with it on, the no-op latency (feature 044) is reported both ways.
- **SC-003**: The reader survives its fuzz target's smoke run and the hostile-input
  table (truncation at every field, lengths past the file, unknown kinds, versions).

## Assumptions

- Features 042 (executor library and ABI) and 063 (generated ids and decoders)
  are complete; 044's full-stack rig is the first place a recording is made and
  replayed in tests.

## Out of Scope

- Recording inside the guest (the transport or an ICD): the device sees every
  context of every process after validation, and writes where the host can read.
- Presentation and scanout content; host GPU memory contents beyond the blob
  snapshots US edge case describes.
- A graphical viewer: the inspect tool prints text a person or another tool reads.
