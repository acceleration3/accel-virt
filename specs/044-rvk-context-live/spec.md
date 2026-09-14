# Feature Specification: End to end: rvk contexts in a real guest

**Feature Branch**: `044-rvk-context-live`

**Created**: 2026-09-11

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 044 · **Status**: `implement` · **Depends on**: 037, 042, 043

M3 done in a real guest: a test program opens the adapter, creates a context
for each sub-API through the transport, allocates and maps host-visible
memory, submits command streams that the real executor runs on the host GPU,
waits on fences, and has a deliberately faulted context recover without taking
the adapter or the other contexts down.

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
| `TransportCommand` | object (device) | the batch's reply writer | Answer a command of the transport's own API |
| `ClHandleRegistry` | system (device) | the OpenCL objects this executor made | Say whether a wire value is a live OpenCL handle |
| `VulkanScenario` | object (guest) | the transport and the handles it made | Build the Vulkan object chain on the host GPU |
| `OpenClScenario` | object (guest) | the transport, the kernel and its buffers | Run vector add on the host GPU |
| `OpenGlScenario` | object (guest) | the transport | Set an OpenGL context up on the host |
| `IsolationScenario` | object (guest) | two transports | Show a faulted context changes no other |
| `RvkSuite` | system (guest) | the scenarios above and the adapter search | Run the rvk scenarios in order |

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
- [`docs/kb/11-implementation-plan.md`](../../docs/kb/11-implementation-plan.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## User Scenarios & Testing *(mandatory)*

Users: every later feature. Before any API driver is packaged, the whole path -
D3DKMT, the KMD, the device, the executor library and the host GPU - must be shown
to work in a real Windows guest, including when it fails. This is milestone M3's
"done when".

### User Story 1 - A context per sub-API runs real host work (Priority: P1)

**Independent Test**: the in-guest runner with the executor library runs
`rvktest.exe`'s rvk scenarios; each passes, and the host executor's log shows the
command streams it ran.

**Acceptance Scenarios**:

1. **Given** a guest with rvkgpu and a device loaded with the executor library,
   **When** the test program creates a link through the guest transport, **Then**
   it gets a Vulkan, an OpenGL and an OpenCL context on the adapter.
2. **Given** a Vulkan context, **When** the program submits a stream that creates
   an instance, a device and a buffer in host-visible memory and writes a
   pattern, **Then** reading the mapped allocation in the guest returns the
   pattern the host wrote.
3. **Given** an OpenCL context, **When** a vector-add kernel runs on buffers in
   host-visible memory, **Then** the result is correct and was produced on the
   host GPU the device is bound to.
4. **Given** an OpenGL context, **When** it clears a framebuffer and reads it back,
   **Then** the pixels match.

### User Story 2 - A faulted context recovers alone (Priority: P1)

**Independent Test**: the test program submits a deliberately malformed stream on
one context while another context works; the runner checks the verdict and the
guest's event log.

**Acceptance Scenarios**:

1. **Given** two live contexts, **When** one receives a malformed stream, **Then**
   that context faults, its sends fail, and the other context's work completes.
2. **Given** a faulted context, **When** the program creates a new link, **Then**
   it works, and the adapter never left the started state.
3. **Given** a stream that stalls the host longer than the TDR window, **When**
   dxgkrnl times out, **Then** only the owning device is reset and the program
   observes device loss on that device alone.

### User Story 3 - The run proves it with numbers (Priority: P2)

**Independent Test**: the runner's report records per-stream latency from submit
to fence, and the diagnostics collected show no Driver Verifier violation.

**Acceptance Scenarios**:

1. **Given** a completed run, **When** the report is read, **Then** it lists each
   scenario's result, the round-trip latency of a no-op command, and the
   host-visible mapping path used (zero-copy or copy fallback).

### Edge Cases

- The host GPU lacks one sub-API (for example no OpenCL ICD on the host): that
  scenario reports the capset does not offer it and is skipped, not failed.
- The executor library is absent: the device offers no capset and every rvk
  scenario reports it by name.
- The guest is 32-bit-process-only for a scenario (WoW64 build): the same
  scenarios pass through low views.
- The QEMU process dies: the runner reports its last log line (feature 035's
  debugging cycle) and the verdict fails.

## Requirements *(mandatory)*

- **FR-001**: `rvktest.exe` gains rvk scenarios over the guest transport,
  portable and tested on Linux over `MockKmt` like the existing scenarios.
- **FR-002**: The in-guest runner can name an executor library and records which
  one ran.
- **FR-003**: Every scenario destroys what it created in reverse, whatever failed.
- **FR-004**: The run passes with Driver Verifier's standard checks on
  `rvkgpu.sys`.
- **FR-005**: Fault isolation is observable: a faulted context changes no other
  context's results and produces no adapter-wide TDR.

## Success Criteria *(mandatory)*

- **SC-001**: With a prepared image and the executor library, the in-guest runner
  passes every rvk scenario on at least one host GPU class, and the gate runs it
  when `AV_GUEST_IMAGE` is set.
- **SC-002**: A no-op command's submit-to-fence latency is reported; its
  regression threshold is set from the first passing run.

## Assumptions

- Features 040, 042 and 043 are complete; the host has at least one Vulkan,
  OpenGL and OpenCL capable device or llvmpipe equivalents.

## Out of Scope

- Loading the real API drivers (045, 050, 052); scenarios use the transport
  directly with streams encoded by render-server's encoders.
- Presentation.
