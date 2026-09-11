# Feature Specification: render-server as a pinned build input

**Feature Branch**: `063-render-server-integration`

**Created**: 2026-09-11

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 063 · **Status**: `implement` · **Depends on**: 001, 032

render-server pinned by revision and hash like QEMU and the Windows kit, and
built from this repository's build: its Vulkan, OpenGL and OpenCL guest
drivers for Windows x64 and x86, and its host executor library with a pkg-
config file. The seam changes this project needs of it - a C-ABI executor over
its server backends, a link built over a transport it is handed, and an OpenGL
client an ICD front end can drive - are carried as a patch series with tests
and sent upstream, so the pin can return to a plain revision.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### Note

Numbered last because it was identified last; 042 and 043 depend on it, so the
roadmap schedules it before them.

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
- [`docs/kb/07-usermode.md`](../../docs/kb/07-usermode.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## User Scenarios & Testing *(mandatory)*

Users: the developers of this repository, who need render-server's encoders,
decoders and guest drivers as ordinary build inputs - fetched, verified and
built by one command like QEMU and the Windows kit - and render-server's
maintainers, who receive the seam changes this project needs as reviewed,
tested patches rather than a fork.

### User Story 1 - A pinned render-server builds with everything else (Priority: P1)

**Independent Test**: from a clean checkout with no render-server anywhere on
the machine, the fetch tool downloads the pinned revision, verifies it, and
the build produces the guest drivers and the executor library; a changed hash
or a missing revision fails the fetch with a named error.

**Acceptance Scenarios**:

1. **Given** a clean checkout, **When** render-server is fetched, **Then** the
   pinned revision is retrieved from its repository, checked against the
   recorded hash, unpacked under `third_party/` (git-ignored) and left
   untouched; a second fetch is a no-op.
2. **Given** a fetched tree, **When** the build runs, **Then** it applies this
   repository's patch series in order, builds the Vulkan, OpenGL and OpenCL
   guest drivers for Windows x64 and x86 with the repository's clang toolchain,
   and builds the host executor library with a `pkg-config` file the device
   build finds.
3. **Given** a patch that no longer applies, **When** the build runs, **Then** it
   stops at that patch, naming it and the file that rejected it.
4. **Given** the build, **Then** every artefact is reproducible from the pin:
   two builds from clean trees yield identical guest DLLs apart from recorded
   timestamps, and the image gates (imports, machine, subsystem) pass for each.

### User Story 2 - The seam render-server must offer, as tested patches (Priority: P1)

**Independent Test**: each patch in the series carries render-server-side tests
that pass in render-server's own test suite, and this repository's executor
conformance suite (feature 042) and transport tests (feature 043) pass against
the patched build.

**Acceptance Scenarios**:

1. **Given** the patched render-server, **When** the executor library is
   loaded through `rvk_executor.h`, **Then** it exposes render-server's Vulkan,
   OpenGL and OpenCL server backends behind the C ABI: context creation per
   sub-API, command-stream dispatch with replies, blob import, fault
   reporting and teardown, with no C++ type crossing the boundary.
2. **Given** the patched guest drivers, **When** a process's first driver
   starts, **Then** render-server's per-process link is built over a transport
   object it is handed, instead of opening its shared-memory region itself; the
   region transport remains available for render-server's own tests.
3. **Given** the patched OpenGL client, **Then** it can be driven by an ICD
   front end - context creation, making current and dispatch lookup reachable
   without exporting `opengl32.dll`'s symbols - which feature 050 builds.
4. **Given** a patch accepted upstream, **When** the pin moves to a revision
   containing it, **Then** the patch is deleted from the series in the same
   change and every test above still passes.

### User Story 3 - The pin moves deliberately (Priority: P2)

**Independent Test**: bumping the pin is one change that updates the revision,
the hash and the patch series together; the gate runs every suite that
consumes render-server before it merges.

### Edge Cases

- The repository is unreachable at fetch time: the fetch fails naming the
  revision; a previously fetched and verified tree is used unchanged.
- The fetched archive's hash differs: nothing is unpacked; the partial download
  is removed.
- The pinned revision's generated sources disagree with its generators: the
  build uses the generated sources as committed at the pin and records the
  mismatch as a warning, never regenerating silently.
- A patch touches render-server's wire format: the change also bumps the rvk
  protocol version this repository negotiates, or the build refuses it.
- The Windows x86 toolchain lacks a library a guest driver links: the x86
  build fails naming the library; the x64 build is unaffected.
- A render-server test that the patch series depends on is flaky: the series
  does not merge until it is reproducible, since features 042 and 043 rest on it.

## Requirements *(mandatory)*

- **FR-001**: render-server is fetched by revision and verified by hash, like
  `tools/fetch_qemu.sh`; no tracked file names a path outside this repository
  (the `self_contained` gate).
- **FR-002**: The build produces, from the pin plus the patch series only: the
  host executor library and its `pkg-config` file; the Vulkan ICD, OpenCL ICD
  and OpenGL client for Windows x64 and x86.
- **FR-003**: The patch series is ordered, each patch has a message stating why
  this project needs it, and each carries tests in render-server's suite.
- **FR-004**: The executor library implements `rvk_executor.h` as this
  repository defines it; the ABI's owner stays this repository (see the
  executor decision recorded with feature 032).
- **FR-005**: Guest drivers built here import only modules every Windows guest
  has, plus `gdi32` for D3DKMT; the image gate proves it for each DLL.
- **FR-006**: The device build (`build-qemu/`) links the executor library found
  through `pkg-config`; without a fetched render-server it still builds with
  the echo executor, and says which it used.
- **FR-007**: Moving the pin runs the executor conformance suite (042), the
  transport suites (043) and the loopback suite (034) before merge.

## Success Criteria *(mandatory)*

- **SC-001**: A clean machine goes from checkout to guest drivers and executor
  library with one fetch and one build command.
- **SC-002**: Every patch in the series has passing render-server tests and an
  upstream submission recorded against it.
- **SC-003**: Features 042 and 043 run their suites against the pinned,
  patched render-server with no manual step.

## Assumptions

- render-server remains reachable as a git repository this project can fetch
  from by URL; the URL and revision are recorded in the fetch tool, not in any
  document path.
- render-server's maintainers accept the seam changes upstream in some form;
  until they do, the patch series is the delivery.

## Out of Scope

- New API coverage in render-server (compatibility profile, fine-grained SVM):
  those belong to features 051 and 052, which state what they need.
- render-server's own server binary and its shared-memory transport: they stay
  render-server's, used by its tests, not by this project's runtime.
- Production signing of the guest DLLs.
