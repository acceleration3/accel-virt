# Feature Specification: Wire protocol: shared definitions and codecs

**Feature Branch**: `004-wire-protocol-core`

**Created**: 2026-09-10

**Status**: Draft

**Input**: `/dev` autonomous run after 003

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 004 · **Status**: `implement` · **Depends on**: 001

src/proto: the structures shared by driver and device — submission
descriptors, fence pages, ring headers, capset payload, version negotiation.
One header, two consumers, no divergence.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

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

<!-- Fill this in during /speckit-plan, before writing code. One row per class; if a row has no state, it is not a class. -->

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `WireReader` | object | borrowed bytes, length, cursor, sticky status | Reads little-endian fields from bytes this side did not write |
| `WireWriter` | object | destination, capacity, cursor, sticky status | Writes little-endian fields without exceeding its destination |
| `ProtocolVersion` | object | major, minor | Decides compatibility with a peer version |
| `GpuIdentity` | object | vendor, device, type, UUID, memory sizes, name | Describes the host GPU a device instance is bound to |
| `CapsetInfo` | object | version, sub-APIs, limits, features, `GpuIdentity` | Carries what one device instance offers |
| `ContextSetup` | object | version, sub-API, ring and fence resources, ring order | Binds a context to its ring and fence page |
| `RingHeader` | object | order, tail, head | Describes one snapshot of a shared ring |
| `RingProgress` | object | order, last head, last tail | Rejects ring counters that break monotonicity or capacity |
| `SubmissionDescriptor` | object | fence id, begin, end | Names one batch of ring bytes and its fence |
| `FencePage` | object | faulted, completed fence, progress, fault code | Describes one snapshot of a context's fence page |
| `FenceProgress` | object | highest submitted, last completed, last progress, faulted | Rejects fence pages inconsistent with what was submitted and seen |

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

## Clarifications

### Session 2026-09-11

- Q (user): Are all host GPUs exposed, and how does that work on any PC? -> A:
  One guest adapter per host GPU, each device instance bound by Vulkan device
  UUID, configuration generated per machine by `tools/host/list_gpus`
  (`docs/kb/11-implementation-plan.md`). For this feature: the capset carries
  that GPU's identity and capability bits.
- Q (agent): Exact layouts? -> A: `docs/protocol.md` is normative; every
  structure fixed-size, little-endian, magic-prefixed, reserved-must-be-zero.
- Q (agent): Casts or codecs? -> A: Codecs only. Shared memory is copied once,
  then decoded and validated from the copy.

## User Scenarios & Testing *(mandatory)*

Users: developers of the guest driver (features 005-025) and the QEMU device
(026-033), who must agree on every byte, and reviewers who need hostile bytes
from either side to be harmless.

### User Story 1 - Both sides encode and decode the same bytes (Priority: P1)

**Independent Test**: every structure in `docs/protocol.md` round-trips:
encode, decode, re-encode gives identical bytes; each field lands at its
documented offset.

**Acceptance Scenarios**:

1. **Given** a valid structure, **When** it is encoded, **Then** each field is
   at its documented offset, little-endian, reserved bytes zero.
2. **Given** the encoded bytes, **When** decoded and re-encoded, **Then** the
   bytes are identical.
3. **Given** a destination shorter than the structure, **When** encoding,
   **Then** the result is `too_large` and nothing past the destination is written.

### User Story 2 - Hostile bytes are rejected, never trusted (Priority: P1)

**Independent Test**: for every decoder, the hostile sweep (empty, every
truncation, bit flips, all-zero, all-ones) and a table of field-specific
violations each produce the documented outcome; a fuzz target round-trips every
accepted input and never crashes.

**Acceptance Scenarios**:

1. **Given** fewer bytes than the structure, **Then** `malformed`, and the
   destination object is unchanged.
2. **Given** a wrong magic, non-zero reserved byte, or out-of-range field,
   **Then** `malformed` naming nothing beyond the status.
3. **Given** a capset whose name has no NUL, or bytes after the NUL, **Then**
   `malformed`.
4. **Given** any accepted input, **Then** re-encoding reproduces it exactly.

### User Story 3 - Counters from the other side cannot go backwards (Priority: P1)

**Independent Test**: ring and fence trackers accept legal progress and reject
each regression, overflow and impossible completion with `invalid_state`,
keeping their last good observation.

**Acceptance Scenarios**:

1. **Given** head H and tail T observed, **When** a snapshot has head < H or
   tail < T, **Then** `invalid_state`.
2. **Given** a snapshot with tail - head > capacity or head > tail, **Then**
   `invalid_state`.
3. **Given** highest submitted fence F, **When** a page reports completed > F,
   **Then** `invalid_state`.
4. **Given** a faulted page was observed, **When** a later page is not faulted,
   **Then** `invalid_state`.
5. **Given** a submission of fence id <= the last submitted, **Then** `invalid_state`.

### User Story 4 - Versions and capabilities negotiate cleanly (Priority: P2)

**Acceptance Scenarios**:

1. **Given** equal majors, **Then** compatible, negotiated minor is the smaller.
2. **Given** different majors, **Then** incompatible.
3. **Given** a context setup whose major, sub-API or ring order the capset does
   not offer, **Then** `not_supported`.
4. **Given** unknown feature bits in a capset, **Then** decoding succeeds and
   the known bits read correctly.

### Edge Cases

- A null byte pointer with non-zero length: treated as zero bytes.
- Ring order 12 and 24 accepted; 11 and 25 rejected.
- Fence id 0 in a submission rejected; completed fence 0 before any completion accepted.
- Counter at 2^64-1: comparisons do not overflow.

## Requirements *(mandatory)*

- **FR-001**: `src/proto` MUST implement each structure in `docs/protocol.md`
  with encode and decode, and nothing else defines those layouts.
- **FR-002**: Decoding MUST leave the destination unchanged unless it returns `ok`.
- **FR-003**: Outcomes MUST be `ok`, `malformed`, `not_supported`,
  `invalid_state` or `too_large` exactly as `docs/protocol.md` assigns them.
- **FR-004**: Ring and fence trackers MUST enforce the monotonicity and bound
  rules, and keep their last good observation on rejection.
- **FR-005**: Every decoder MUST have a hostile table test and be reachable from
  a libFuzzer target with a committed seed corpus that round-trips accepted inputs.
- **FR-006**: The code MUST be freestanding (driver and device both compile it),
  allocate nothing and never block, so it is usable at DISPATCH.

## Success Criteria *(mandatory)*

- **SC-001**: Every documented field has an offset test.
- **SC-002**: 100% branch coverage of `src/proto`, no untestable marks.
- **SC-003**: The fuzz smoke finds no crash, sanitizer report or round-trip mismatch in 60 s.
- **SC-004**: The driver build compiles `src/proto` for the kernel.

## Assumptions

- Command payloads inside the ring are opaque bytes owned by the codec (feature 032).
- The control-command envelope that carries `ContextSetup` is defined with the
  device's command handling (feature 029); this feature defines the payload.

## Out of Scope

- Ring record framing and read/write of the data area (features 009/030).
- Transport, virtqueues, doorbells, interrupts (005, 030, 031).
