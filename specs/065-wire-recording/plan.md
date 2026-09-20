# Implementation plan: wire recordings - capture, inspect and replay (065)

**Branch**: `065-wire-recording` · **Spec**: spec.md · **Research**: research.md ·
**Data model**: data-model.md · **Contract**: contracts/rvkrecord.md ·
**Quickstart**: quickstart.md

## Summary

Today, when a guest renders wrong or QEMU dies inside a host driver, the
evidence is a screenshot and the device's log; the bytes that caused it are
gone. 065 writes those bytes down.

The device gains a `record=<path>` property. With it set, every context's
setup, every batch as the executor library received it, every reply, completed
fence, fault and blob lifetime becomes a length-prefixed typed record in a file
on the host, flushed before the batch is handed over so a crash keeps what led
to it, and bounded by a ceiling that either stops or wraps. With it unset - the
default - `WireRecorder`'s first line returns and nothing on the submission path
changes.

A host tool, `rvkrecord`, reads the file back. `print` shows each record with
its command names from the generated id tables and its decoded transport
arguments, filtered by context, sub-API, fence or command name, and says where
the damage is when a recording is cut short. `replay` drives the recorded
stream into an executor library through `rvk_executor.h` - the same ABI the
device drives - and names every answer that differs from the recorded one by
fence and command, so a rendering mistake, a crash or a validation-layer
complaint is reproduced on this machine, on another GPU, or under the Khronos
layers, without a guest.

Every byte of a recording is read as hostile: the reader is portable core with a
hostile-input table and a libFuzzer target with a committed corpus.

## Technical context

C++23 freestanding, no standard library, `av::Status` everywhere, ASan and
UBSan in the test build. The work is in `src/device/recording/` (the format, the
recorder, the reader, the printer, the replay - portable core, measured by the
coverage gate), `src/device/shim/` (the file, the mapping, the `dlopen`, and
`main`), `src/device/host/host.hpp` with `src/device/abi/` and
`src/device/qemu/` behind it (four file operations), `src/device/execution/`,
`src/device/context/`, `src/device/memory/` and `src/device/module/` (the six
places a record is taken), `docs/protocol.md` (the normative format) and
`tools/guest/run_guest_suite.py` (`--record`).

Tests: unit tests over `MockHost` and `MockExecutorOps` for every class; a
device-core capture test that binds a context, submits batches, takes a reply
and a fault and reads the recording back; a loopback test that records a real
guest-core-to-device-core run and replays it; a fuzz target on the reader.

## Classes this feature introduces

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `RecordingHeader` | object | magic and version, the wire version recorded, the kinds kept, the ceiling, the wrap count, the start clock, the host GPU's identity | The first bytes of a recording |
| `RecordHeader` | object | kind, context, fence, payload length, timestamp | The framing of one record |
| `RecordKinds` | object | the mask of record kinds kept | Which kinds a recording keeps |
| `RecordingLimit` | object | the ceiling, the bytes written, whether it wraps | What happens when the next record would pass the ceiling |
| `WireRecorder` | system | `Host&`, the open file, the header, the limit, the kinds, a mutex, what it refused | Writes what crosses the wire to the recording |
| `RecordingReader` | system | the bytes and their length, the file header, the read offset, where the damage is | Walks a recording's records, stopping at the first damage |
| `RecordingFilter` | object | a context, a sub-API, a fence range, a command-name fragment | Whether a record is shown |
| `RecordingPrinter` | system | the reader, the filter, the line buffer, the byte budget | Prints a recording as lines a person reads |
| `ReplayContext` | object | a recorded context id, its executor, its sub-API, its last answer | One recorded context, replayed on a library |
| `RecordingReplay` | system | `ExecutorLibrary&`, the replay contexts, the GPU asked for, the findings | Replays a recording into an executor library |
| `RecordedCommand` | object | one command's header and payload inside a batch record | One command of a recorded batch |
| `ContextApi` | object | one recorded context and the API half of the ids it sends | Which API's commands a recorded context speaks |
| `RecordingToolRequest` | object | the action, the recording, the library, the GPU and the byte budget | What `rvkrecord` was asked to do |
| `MappedFile` | object | the mapping and its length | A file's bytes, mapped read-only |
| `PosixExecutorModule` | object | the opened library and its table | An executor library loaded by path |

Two rows changed as the code was written, and both are recorded here rather
than quietly:

- `PosixRecordingFile` is **not** built. With the QEMU glue doing the four file
  calls in C - `open`, `qemu_write_full`, `fdatasync`, `ftruncate` - there is
  nothing left for it to own, and a class with no state is a mistake being
  written down.
- `ContextApi` and `RecordingToolRequest` were not foreseen. The first is what
  lets `--api` keep a context's replies and fences, since only a batch's
  commands carry an API in their ids; the second is what the tool was asked to
  do, which is a decision and therefore belongs in core rather than in `main`.

`RecordingSettings` is deliberately **not** a class either: the device's four
recording properties are three strings and a number that `VirtioGpuRvk::realize`
hands straight to `WireRecorder::open`, `RecordKinds::parse` and
`RecordingLimit`. A row with nothing of its own to remember does not become a
class.

## Phasing

1. **The format.** `RecordingHeader`, `RecordHeader`, `RecordKinds`,
   `RecordingLimit`, encode and decode, with the hostile-input table. The
   normative tables go into `docs/protocol.md`. **Checkpoint.**
2. **The file.** `create_recording`, `append_recording`, `truncate_recording`
   and `close_recording` on `Host`, `RvkHostOps`, `TableHost`, the QEMU glue
   (`PosixRecordingFile`) and `MockHost`, whose recordings are memory a test
   reads back. **Checkpoint.**
3. **Capture.** `WireRecorder`, the `record*` properties on
   `rvk_device_create`, and the six places a record is taken: `DeviceContext`
   binding, `BatchExecutor` before the first command, its reply and its fault,
   `FenceReporter` on completion, and the blob paths. Off by default, and a
   test proves the submission path copies nothing when it is off.
   **Checkpoint.**
4. **The reader.** `RecordingReader`, its hostile-input table and
   `tests/fuzz/fuzz_recording_reader.cpp` with a committed corpus.
   **Checkpoint.**
5. **The printer.** `RecordedCommand`, `RecordingFilter`, `RecordingPrinter`,
   the generated command names, the decoded transport arguments, the filters
   and the damage line. **Checkpoint.**
6. **Replay.** `ReplayContext`, `RecordingReplay` over `ExecutorLibrary`, the
   answer comparison, the blob snapshot import. **Checkpoint.**
7. **The tool and the run.** `MappedFile`, `PosixExecutorModule`, `main`, the
   `rvkrecord` target, `run_guest_suite.py --record` with its `tests/tools`
   test, and the loopback record-then-replay test. **Checkpoint.**

## Constitution check

- **I**: spec.md exists and states its acceptance criteria; this plan names
  every class before any code is written.
- **II**: every decision is in `src/device/recording/`; `src/device/shim/` does
  `open`, `mmap`, `dlopen`, `write` and argv, with no branch that is not the
  platform's; the QEMU glue gains four calls that translate and decide nothing.
- **III**: every row above owns state and has one responsibility. No static
  member function, no stateless class, no free function but `main`, which is a
  C entry point with nowhere to put a `this` and holds no logic.
- **IV**: tests first and failing first; every branch of
  `src/device/recording/` covered; `invariants_hold` on every stateful class,
  called after every mutation; a hostile-input table for the reader and a
  libFuzzer target with a committed corpus, because a recording is bytes this
  side did not write; an allocation-failure sweep over `MockHost`.
- **V**: the reader trusts nothing - every length is checked against the bytes
  the reader owns, every offset arithmetic is overflow-checked, an unknown
  kind stops the walk rather than being guessed at.
- **VIII**: no speculative structure. There is one recording format, one
  reader, and two things that use it, which is why the reader is shared and
  nothing else is abstracted.
- **IX**: a refused recording is logged once, at the point of decision, with
  the path that caused it, and the run continues.

## Interpretation recorded against the spec

FR-005 asks the inspect tool to decode arguments "with the generated decoders".
The generated decoders decode a command *into a call*; naming each argument
would need a new emitter in `tools/codegen/` writing a printer for each of
nearly two thousand commands. This feature therefore names every command from
the generated id tables (never a hand-written list), decodes in full the
transport commands this repository owns, and shows every other command's
arguments as its wire framing plus bounded payload bytes. research.md §5 records
the generated per-command printer as the follow-on it is.

## Risks

- **A record on the submission path costs time.** It is one `write` per batch,
  on the worker, only when recording is on. SC-002 measures the no-op round
  trip both ways and the measurement is what settles it.
- **Several workers writing one file.** One mutex in the recorder, taken for
  the length of one record, is what keeps records whole; the device's existing
  `Host` mutex contract is what it is built from.
- **A replay is not a guest.** Guest blob contents are not on the wire, so a
  stream that reads from a blob replays only with a snapshot in the file. The
  replay says `unreplayable` rather than producing a wrong answer quietly.
