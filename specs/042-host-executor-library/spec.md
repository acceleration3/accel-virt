# Feature Specification: Host executor: render-server's backends behind rvk_executor.h

**Feature Branch**: `042-host-executor-library`

**Created**: 2026-09-11

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 042 · **Status**: `implement` · **Depends on**: 032, 039, 063

A C-ABI executor library, built from render-server's Vulkan, OpenGL and OpenCL
server backends, that the device loads by its executor property: one rvk
context is one session on one worker, commands decode into the backend for the
context's sub-API, host-visible and guest-backed blobs are imported rather
than copied, and every decode or device-loss failure becomes a fault on the
wire. An executor conformance suite in this repository holds any executor
library to the contract.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### Note

render-server builds the library, pinned and built here by feature 063; this
repository owns the ABI (src/device/abi/rvk_executor.h), the conformance suite
and the device side. render-server exports no C ABI or pkg-config file today.

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

- [`docs/kb/11-implementation-plan.md`](../../docs/kb/11-implementation-plan.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## User Scenarios & Testing *(mandatory)*

Users: the QEMU device, which today executes through an echo executor, and the
render-server project, whose Vulkan, OpenGL and OpenCL server backends already
execute the command streams its drivers encode but reach them only through its own
shared-memory server. This feature puts those backends behind the executor ABI
this repository owns, so one QEMU device runs real host GPU work.

### User Story 1 - The device runs render-server's backends (Priority: P1)

**Independent Test**: the qemu configuration loads the executor library by the
`executor=` property; the virtio-gpu suites create a Vulkan context, submit a
stream that creates an instance and enumerates devices, and read a reply that
names the host GPU the device is bound to.

**Acceptance Scenarios**:

1. **Given** a device realized with the executor library, **When** its capset is
   read, **Then** it offers exactly the sub-APIs (Vulkan, OpenGL, OpenCL) the
   library reports it can execute on the bound host GPU.
2. **Given** an rvk context created for a sub-API, **When** the guest submits a
   command stream on its ring, **Then** the context's worker decodes it into
   that sub-API's backend, writes replies to the context's reply area, and
   advances the fence the stream names.
3. **Given** two contexts of different sub-APIs, **When** both submit, **Then**
   each runs on its own worker and neither observes the other's state.

### User Story 2 - Memory is imported, not copied (Priority: P1)

**Independent Test**: the executor conformance suite creates a host-visible blob
and a guest-backed blob through the device, asks the executor to import each, and
checks the executor's view of a byte written by the guest side without a
transfer command.

**Acceptance Scenarios**:

1. **Given** a host-visible blob, **When** a stream uses it as Vulkan device
   memory, a GL buffer or CL memory, **Then** the executor uses the host memory
   the blob already is.
2. **Given** a guest-backed blob on a host where external host-memory import is
   unavailable (feature 039), **When** a stream uses it, **Then** the executor
   copies at the boundary the capset advertises, and says so in its capabilities.

### User Story 3 - Any executor is held to the contract (Priority: P1)

**Independent Test**: the executor conformance suite runs against the echo
executor and the render-server library alike.

**Acceptance Scenarios**:

1. **Given** an executor library, **When** the suite runs, **Then** it checks ABI
   version negotiation, context create and destroy, stream submission, replies,
   blob import and release, faults and teardown with contexts still live.
2. **Given** a malformed or truncated stream, **When** it is submitted, **Then**
   the executor reports a fault for that context and every other context keeps
   running.

### Edge Cases

- A guest-authored stream with a command size past the ring, a nested length
  that overflows, an unknown API id, a handle the context never created, or a
  blob id outside the context: a fault on the wire for that context, no read
  outside the stream, no crash under ASan (constitution V).
- The host driver reports device loss: every context on that GPU faults, the
  device reports it in each fence page, and new contexts are refused until
  reset.
- The guest destroys a context while its worker executes: the executor finishes
  or abandons the stream, releases imports, and the context's memory is not
  touched afterwards.
- ABI version mismatch between the device and the library: realize fails with a
  named error, and the device offers no capset rather than a partial one.
- A stream blocks on host GPU work longer than the guest's TDR window: the
  executor reports progress through the heartbeat the device exposes to
  `QueryEngineStatus` (feature 033), not ring occupancy.

## Requirements *(mandatory)*

- **FR-001**: This repository owns the contract: `rvk_executor.h` (ABI v1 and its
  successors), the device's loading and validation of it, and the executor
  conformance suite. The ABI is C; no C++ type crosses it.
- **FR-002**: render-server must provide a shared library exporting
  `rvk_executor_ops` that implements the ABI over its Vulkan, OpenGL and OpenCL
  server backends, with a pkg-config file the device build finds; render-server
  exports neither today.
- **FR-003**: render-server's backends must accept a per-context attachment in
  place of its shared region: the device supplies the ring, reply area and blob
  table, and completion is reported through the ABI (in place of
  `CompletionSink`), not by writing a region header.
- **FR-004**: One rvk context is one executor session on one device worker; the
  executor creates no threads that call back into the device.
- **FR-005**: Every decode failure, host API error that loses the device, and
  unknown command becomes a context fault on the wire; the executor never
  aborts the QEMU process.
- **FR-006**: Blob import prefers zero-copy (host-visible memory as it is; guest
  memory by external host-memory import through a memfd window) and falls back
  to copies only where the capset says so.
- **FR-007**: The executor reports, per bound host GPU, which sub-APIs and which
  memory import paths it supports; the capset reports only those.
- **FR-008**: render-server's decoders are fuzzed against guest-authored streams
  in render-server's suite; this repository's suite fuzzes the ABI boundary with
  hostile stream descriptors and blob tables.

## Success Criteria *(mandatory)*

- **SC-001**: The conformance suite passes for the echo executor and the
  render-server library, under ASan and UBSan.
- **SC-002**: The qemu configuration runs a Vulkan triangle, an OpenGL clear and
  an OpenCL vector add through the device with results read back by the
  virtio-gpu suite.
- **SC-003**: A byte-flip sweep of a valid stream produces faults, never a crash,
  for 60 seconds of fuzzing per sub-API.

## Assumptions

- render-server remains the source of the generated encoders and decoders; its
  server executable keeps working on its own transport alongside the library.
- Host OpenGL is surfaceless EGL, and host OpenCL is the host's ICD loader, as
  render-server's server uses today.

## Out of Scope

- The guest transport and drivers (043, 045, 050, 052).
- Changing render-server's wire format; the executor carries its streams opaquely
  inside the rvk ring's command records.
