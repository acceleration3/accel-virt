# Research: wire recordings (065)

## 1. What already exists that a recording must not duplicate

- `src/proto/submission/` frames everything that crosses the wire: `BatchHeader`
  (fence and payload length), `CommandHeader` (id, size, *generate reply*),
  `ReplyHeader` (the answer's slot header), `FaultCode`.
- `src/proto/context/context_setup.hpp` is a context's setup, already encodable
  in 32 or 48 bytes with its own magic and version.
- `src/device/execution/batch_executor.cpp` is the one place a batch's payload
  has been copied out of the ring and is about to be walked - the exact bytes
  the executor library receives.
- `src/device/execution/executor_library.hpp` is the device's own wrapper over
  `rvk_executor.h`; a replay that drives it drives the ABI the device drives
  (FR-006) without a second implementation.
- `src/proto/generated/*/\*_command_names.h` answers `cmd_id_to_string` for each
  API. That is the generated id table FR-005 names.
- `tests/harness/executor/executor_library_file.hpp` already dlopens a library
  for tests; the tool needs the same thing in `src/device/shim/`, which is where
  constitution II puts a `dlopen`.

**Decision**: the recording carries the encoded forms `src/proto` already owns
as record payloads, and adds only the framing around them. Nothing about a
batch, a setup, a reply or a fault is re-described.

## 2. Where the bytes are written

The device core is freestanding and reaches QEMU only through `Host`
(`src/device/host/host.hpp`). It has no file.

**Decision**: four operations join `Host` and `RvkHostOps` -
`create_recording`, `append_recording` (writes *and* flushes), `truncate_recording`
and `close_recording`. The QEMU glue implements them with `open`/`write`/
`fdatasync`/`ftruncate`; `MockHost` keeps the bytes in memory, which is what
lets a unit test read a recording back (US1's independent test).

**Alternatives rejected**: a `RecordingSink` interface of its own would be a
second platform seam beside `Host` for one kind of resource; writing from
`src/device/shim/` directly would put a decision in the shim, which
constitution II forbids.

## 3. When the bytes are written

US1 scenario 3 is decisive: after a crash inside a host driver the recording
must hold *the batch it was running*. That is only true if the batch record is
on disk before `execute` is called.

**Decision**: the batch record is written on the context's worker, synchronously,
before the first command of the batch is run, and `append_recording` flushes.
Records from several workers are serialised by a mutex the recorder owns, so a
record never interleaves with another (the concurrency edge case). FR-003's
"off the executor's hot path" is honoured by the recording being *off* by
default: with no file open `WireRecorder`'s first line returns and the
submission path does no copy (US1 scenario 4).

## 4. The limit

**Decision**: `RecordingLimit` owns the ceiling and the bytes written and
answers one of three verdicts for the next record - `fits`, `stop`, `wrap`.
`stop` leaves the file as it is and logs once; `wrap` truncates the file back to
its header, bumps the header's wrap count and rewrites it, so the newest records
survive and the file never passes the ceiling. Either way the device log says so
once, not per record.

**Alternative rejected**: a second file (`.1`, `.2`) would put path arithmetic in
core and give the reader two files to stitch.

## 5. What "decoded arguments" can mean here (FR-005, US2 scenario 1)

The generated tables give two things: `cmd_id_to_string(id)` per API, and
`dispatch_command(ServerDispatch&, ...)`, which decodes a command *and calls a
handler for it*. There are close to two thousand handlers; a printer that named
each argument would be a new emitter in `tools/codegen/` writing a printer per
command - a change to the generator, not to this feature, and megabytes of
committed code.

**Decision, and the one place this feature interprets its spec**: the inspect
tool names every command from the generated table (`cmd_id_to_string`, never a
hand-written list), decodes in full the arguments of the transport commands this
repository owns (NOP, IMPORT_BLOB, RELEASE_BLOB - the ids in
`CommandHeader`), and shows every other command's arguments as its wire framing
plus its payload bytes, bounded by `--bytes`. A generated per-command argument
printer is recorded here as the follow-on it is.

## 6. Replay fidelity

A recording holds what the executor library received, not what the host GPU
held. Guest blob contents are not on the wire (the spec says so in its edge
cases). A replay therefore reproduces: every context's setup, every command in
recorded order, and every answer, compared with the recorded answer by fence and
command index. A command that names memory imported from a guest blob replays
against an import the replay itself makes from a recorded blob snapshot when one
is in the file, and is reported as unreplayable when it is not.

**Decision**: `blob` records carry the lifetime (created, imported, released,
with id, offset and size); a `blob_bytes` record carries the snapshot, and is a
record kind that is off unless asked for, because it is the only kind whose size
is not bounded by the wire.

## 7. Where the tool lives

`executor_conformance` is the precedent: logic in a class, `main` in a file of
its own that parses argv and calls it.

**Decision**: `rvkrecord print|replay`, whose `main` is
`src/device/shim/recording_tool.cpp` (the shim: argv, `mmap`, `dlopen`, `write`
to standard output) and whose every decision is in `src/device/recording/`,
which the coverage gate measures like the rest of `src/device`.
