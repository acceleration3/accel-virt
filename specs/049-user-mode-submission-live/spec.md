# Feature Specification: User-mode submission in the guest: doorbells and native fences

**Feature Branch**: `049-user-mode-submission-live`

**Created**: 2026-09-11

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 049 · **Status**: `implement` · **Depends on**: 045, 024

The transport submits by writing its ring and storing to a WDDM doorbell
mapped into the process, and waits on native fences, so the KMD is out of the
submission path; doorbells disconnected by suspend or TDR are handled; per-
submit cost is measured against D3DKMTSubmitCommand and held by a benchmark
ratchet.

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
| | object / system | | |

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

- [`docs/kb/05-scheduling.md`](../../docs/kb/05-scheduling.md)
- [`docs/kb/08-wddm3x-delta.md`](../../docs/kb/08-wddm3x-delta.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## User Scenarios & Testing *(mandatory)*

Users: every application on the adapter, through the guest transport. Feature 043
submits through `D3DKMTSubmitCommand`, a kernel transition per submission. WDDM
3.1 doorbells map a device register into the process and native fences let it wait
without the kernel; on this device a doorbell store is a KVM ioeventfd that wakes
the host worker directly. This is milestone M6.

### User Story 1 - Submission without a kernel transition (Priority: P1)

**Independent Test**: on Linux, `MockKmt` and the loopback rig connect a doorbell
for a context, the transport writes records and stores to the mapped doorbell
page, and the device worker consumes them; the mock records no submit call.

**Acceptance Scenarios**:

1. **Given** an adapter reporting user-mode submission capabilities, **When** the
   transport creates a context, **Then** it creates and connects a doorbell for
   it (`D3DKMTCreateDoorbell`, `D3DKMTConnectDoorbell`) and maps the doorbell page.
2. **Given** a connected doorbell, **When** a record is written to the ring and
   the doorbell is stored, **Then** the host worker wakes through the ioeventfd
   and the record is consumed with no D3DKMT call on the submit path.
3. **Given** work waiting on completion, **When** the transport waits, **Then** it
   uses a native fence shared with the device and wakes without polling.

### User Story 2 - Disconnection is survived (Priority: P1)

**Independent Test**: `MockKmt` scripts a doorbell disconnect mid-stream; the
harness checks stores after it are harmless and the transport reconnects.

**Acceptance Scenarios**:

1. **Given** a connected doorbell, **When** dxgkrnl disconnects it for suspend,
   reset or TDR, **Then** the KMD remaps the page to a dummy page, a store to it
   has no effect, and the transport detects the disconnect from the doorbell's
   status and resubmits through `D3DKMTSubmitCommand` until it reconnects.
2. **Given** a device reset, **When** the transport reconnects, **Then** no record
   is executed twice and none is lost; sequence numbers stay monotonic.

### User Story 3 - The gain is measured and held (Priority: P2)

**Independent Test**: a benchmark in the in-guest suite submits a million no-op
records both ways and reports per-submit cost.

**Acceptance Scenarios**:

1. **Given** the benchmark, **When** it runs, **Then** it reports the per-submit
   cost with doorbells and with `D3DKMTSubmitCommand`, and the doorbell path is
   cheaper; the ratio is recorded as a ratchet.

### Edge Cases

- A hostile process stores arbitrary values, stores to a doorbell it disconnected,
  or writes a ring tail past the ring: the device validates the tail against the
  ring it owns, faults only that context, and never reads outside the ring.
- A process maps a doorbell for a context it does not own: `ConnectDoorbell`
  refuses it.
- The adapter does not report user-mode submission (an older device, or the
  capability disabled): the transport keeps using `D3DKMTSubmitCommand`, and the
  reported behaviour is identical.
- A native fence's value goes backwards or jumps past what was submitted: the
  context is faulted.
- A 32-bit process: the doorbell page and fence mapping are below 4 GiB.

## Requirements *(mandatory)*

- **FR-001**: The transport submits through a connected WDDM doorbell whenever
  the adapter reports `DXGKQAITYPE_USERMODESUBMISSION_CAPS`, and falls back to
  `D3DKMTSubmitCommand` while disconnected.
- **FR-002**: The KMD honours `DisconnectDoorbell` by remapping to a dummy page
  before returning (kb 05), and reports the doorbell's connection state.
- **FR-003**: Completion waits use native fences, including across processes
  once opened (feature 025).
- **FR-004**: The hot path takes no lock, makes no allocation and no system call
  in the steady state; this is asserted by the harness.
- **FR-005**: Every value the device reads from a user-written ring or doorbell is
  validated; a violation faults the context (constitution V).

## Success Criteria *(mandatory)*

- **SC-001**: With doorbells the in-guest benchmark's per-submit cost is at least
  five times lower than through `D3DKMTSubmitCommand` on the first passing host;
  the measured ratio becomes the ratchet.
- **SC-002**: The Vulkan application suite (feature 048) passes unchanged with
  doorbells on.
- **SC-003**: A suspend-resume and a forced TDR during the benchmark lose and
  duplicate no records.

## Assumptions

- Features 024 and 025 implement the doorbell and native fence DDIs and pass
  their harness tests; the device's doorbell BAR and ioeventfd exist (030).

## Out of Scope

- Changing render-server's command encoding; batching policy stays the drivers'.
- Preemption of user-mode submitted work beyond what TDR already provides.
