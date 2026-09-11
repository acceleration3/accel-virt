# Implementation Plan: Wire protocol core

**Branch**: `004-wire-protocol-core` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

`docs/protocol.md` is the normative layout. `src/proto` implements it as value
classes over a sticky-status `WireReader`/`WireWriter`, plus two trackers that
police counters written by the other side. Freestanding, allocation-free, no
platform dependency (usable at DISPATCH and in QEMU worker threads).

## Design decisions

- **Sticky status.** Reader and writer latch the first failure; field reads
  after it return zero and writes become no-ops. A codec reads or writes every
  field and checks the status once, which keeps the branch count - and the
  coverage burden - per structure small.
- **Atomic decode.** Decode builds a candidate and assigns `*this` only on `ok`.
- **Byte-assembled integers.** Little-endian by shifts, never `memcpy` into a
  host integer, so the code is endian- and alignment-independent.
- **One fuzz target** (`fuzz_proto`), first byte selects the decoder; every
  accepted input must re-encode to the same bytes.
- `Status::malformed` added; the shim maps it to `STATUS_INVALID_PARAMETER`.

## Constitution Check

| Principle | Status |
|---|---|
| II | Pass - freestanding, compiled into the driver and (later) the device |
| III | Pass - classes in spec table; trackers and cursors have invariants |
| IV | Pass - offset tests, hostile sweeps, fuzz target, tests first |
| V | Pass - every read bounds-checked; shared memory copied before validation |
| VIII | Pass - no ring framing or transport |

## Structure

```text
src/core/status.hpp            + malformed
src/proto/wire_reader.{hpp,cpp}  wire_writer.{hpp,cpp}  protocol_version.{hpp,cpp}
src/proto/sub_api.hpp           gpu_identity.{hpp,cpp}  capset_info.{hpp,cpp}
src/proto/context_setup.{hpp,cpp}  ring_header.{hpp,cpp}  ring_progress.{hpp,cpp}
src/proto/submission_descriptor.{hpp,cpp}  fence_page.{hpp,cpp}  fence_progress.{hpp,cpp}
tests/unit/test_<each>.cpp      tests/fuzz/fuzz_proto.cpp  tests/corpus/fuzz_proto/
```
