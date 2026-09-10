# Feature Specification: Build and test foundation

**Feature Branch**: `001-build-and-test-foundation`

**Created**: 2026-09-10

**Status**: Draft

**Input**: `/dev` - lowest-numbered unblocked feature; gaps observed in the tree on 2026-09-11 (see Context)

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 001 · **Status**: `implement` · **Depends on**: nothing

CMake/Ninja build producing three targets from one source tree: the native
test binary, the Windows driver, and the QEMU device object. ctest as the
single entry point, with ASan/UBSan and llvm-cov wired in and gating.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### Design rules — binding on this feature

Constitution Principle III and [`CLAUDE.md`](../../CLAUDE.md). Restated here because it is checked at review, not assumed:

- **Every unit of behaviour is a class that owns state.** No static member functions. No stateless class used as a namespace. No free function outside the two ABI boundaries (`DxgkDdi*` thunks in `src/km/`, and `extern "C"` entry points), each one line with no logic.
- If functions thread the same handle, buffer, bounds or platform through their parameters, **that parameter is a member**. Make it a class and inject its dependencies at construction.
- Name the classes this feature adds before writing them, and say which kind each is: an **object class** (one instance per real thing) or a **system class** (a long-lived service owning a collection).
- Dependencies injected at construction. No global, no singleton, no file-scope state.
- Data members private, trailing underscore. Classes `PascalCase`, methods and locals `snake_case`. No `Manager`, `Helper`, `Utils`, `Handler`, `Impl`, `process()`, `handle()`.
- One class per header, named for it. A header declares; it does not define. One responsibility per class, statable without an "and".
- Errors are `av::Status` values. No exceptions, no RTTI, no C++ standard library, no `new`/`delete`. Cleanup is RAII via `ScopeExit`, never repeated per branch.
- Fallible construction is two-phase: trivial constructor, `initialize(...)` returning `Status`.
- Every public method declares its IRQL ceiling via `platform_.require_irql_at_most(...)`.

### Classes this feature introduces

<!-- Fill this in during /speckit-plan, before writing code. One row per class; if a row has no state, it is not a class. -->

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| *none* | - | - | This feature adds build rules, gate scripts, a kit fetch, and one `extern "C"` ABI entry point (`DriverEntry`). A C++ class here would own nothing. |

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

- [`docs/kb/12-re-cookbook.md`](../../docs/kb/12-re-cookbook.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Context

What exists on 2026-09-11, and why this feature is not already done:

- The native test build, sanitizers, a coverage script and a design gate exist
  and pass (4/4 tests, 100% branches over `src/core`).
- **Nothing proves the portable core compiles for the guest kernel.** Compiling
  `src/core` for the Windows kernel fails on its first include
  (`alignment.hpp(6,10): fatal error: 'cstdint' file not found`). The "kernel
  subset" the constitution promises is currently enforced only by flags on a
  host build that still has a C++ standard library on its include path.
- There is no Windows driver target and no QEMU device target.
- Coverage is a separate script, not part of the single test entry point. It
  scans for `*.c` sources while every source is `.cpp`, so its
  `AV_UNTESTABLE` count is always zero and nothing ratchets it.
- `tools/check_symbol_tests.py` is named as a build gate by the constitution,
  `CLAUDE.md` and every spec, and does not exist; `tools/check_design.py` does
  its job under another name.
- The fuzz build restates the core's source list by hand instead of consuming
  the core library, so a new core file is silently absent from every fuzz
  target.

## Clarifications

### Session 2026-09-11

- Q: `src/device` is empty; build a minimal device target now or defer? -> A:
  Define the build rule now; the target comes into existence with feature 026's
  first real source. No placeholder entry point.
- Q: Windows kit absent? -> A: Fail configuration, naming the expected path and
  its override.
- Q: Sanitizer, coverage and fuzzer builds cannot share a build directory; how
  is "one entry point" met? -> A: One committed command drives three build
  configurations and runs the test entry point in each. Plain test entry point
  in the default build runs the fast gates (unit, design, symbol-test, kernel
  cross-build).
- Q: (user, mid-plan) No references to the knowledge base or other external
  projects; bring what is needed into this repository. How does the Windows kit
  come in? -> A: A pinned fetch script in this repo downloads the WDK and SDK
  NuGet packages at fixed versions, verifies SHA-256, and extracts only the
  needed subset into a git-ignored directory of this repo. Nothing
  Microsoft-licensed is committed.
- Q: Reverse-engineering setup? -> A: The knowledge base documents move into
  `docs/kb/`, the RE scripts into `tools/re/`; the Ghidra workspace is rebuilt
  locally into a git-ignored directory by a script from a committed manifest of
  pinned binaries.
- Q: The command-serialization codec (`render-server`)? -> A: Referred to as a
  named external C-ABI library found via pkg-config, with no filesystem path;
  how it is brought in is decided at feature 032.

## User Scenarios & Testing *(mandatory)*

The users of this feature are the people and agents developing accel-virt, and
the reviewers who have to trust that a green run means the constitution holds.

### User Story 1 - A green run means the core runs in the guest kernel (Priority: P1)

A developer changes `src/core`. The standard test run builds the same core for
the Windows kernel, links it into the driver image, and fails if the core
reached for anything the kernel does not provide - a standard library header,
a C runtime symbol, an exception or RTTI dependency, a heap operator.

**Why this priority**: Principle II is the reason unit testing a miniport is
possible at all, and today it is unverified. Every later feature builds on core
code that might not link in the guest; discovering that at feature 006 means
rewriting five features of headers.

**Independent Test**: Run the standard test run on a tree where a core header
includes a standard library container header; the run fails and names the
header. Revert; it passes and the driver image exists.

**Acceptance Scenarios**:

1. **Given** the current tree, **When** the standard test run executes, **Then**
   a driver image is produced from the same core sources the unit tests use, and
   a test confirms it is a kernel-mode image whose imports come only from
   kernel-provided modules.
2. **Given** a core source that includes a hosted-only standard header, **When**
   the standard test run executes, **Then** it fails and the failure names the
   file and the header.
3. **Given** a core source that references a C runtime or C++ runtime symbol
   (heap operator, exception machinery, `memcpy` not provided by the kernel),
   **When** the driver image is linked, **Then** the run fails naming the
   unresolved symbol.
4. **Given** the driver image is loaded by Windows before feature 006 exists,
   **When** its entry point runs, **Then** it declines to load with a failure
   status rather than registering an adapter it cannot operate (Principle VI:
   report nothing that is not implemented).

---

### User Story 2 - One entry point runs every gate (Priority: P1)

A developer or CI job runs one committed command and gets one verdict
covering: unit tests under address and undefined-behaviour sanitizers, the
design rules, the symbol-test rule, 100% branch coverage of `src/core`,
`src/device` and `src/proto`, the `AV_UNTESTABLE` ratchet, the kernel
cross-build, and fuzz smoke runs. No gate lives only in a script someone has to
remember to run.

**Why this priority**: The constitution calls coverage and fuzzing "gates, not
reports". A gate outside the entry point is a report.

**Independent Test**: Delete one test that covers a branch; the entry point
fails with the uncovered file and line. Add an `AV_UNTESTABLE` mark without
updating the recorded baseline; the entry point fails and states the old and
new counts.

**Acceptance Scenarios**:

1. **Given** a branch in `src/core` with no test reaching it, **When** the
   entry point runs, **Then** it fails and reports the file, line and branch
   coverage percentage.
2. **Given** the number of `AV_UNTESTABLE` marks in `src/` rises above the
   committed baseline, **When** the entry point runs, **Then** it fails and
   prints both counts and the location of every mark.
3. **Given** the mark count falls below the baseline, **When** the entry point
   runs, **Then** it passes and tells the developer to lower the baseline, so
   the ratchet only moves one way.
4. **Given** a public method in a `src/core` header that no test names, **When**
   the entry point runs, **Then** the symbol-test gate fails naming the class
   and method.
5. **Given** a static member function or a class with no data members in
   `src/`, **When** the entry point runs, **Then** the design gate fails naming
   it.
6. **Given** a fuzz target that crashes, leaks or trips a sanitizer on any
   committed corpus input or within its smoke budget, **When** the entry point
   runs, **Then** it fails and the reproducer path is printed.
7. **Given** a clean tree, **When** the entry point runs, **Then** every gate
   above reports pass, and the run states which gates ran.

---

### User Story 3 - The device side has a build home (Priority: P2)

A developer starting the QEMU device work (feature 026) finds the device
build rule already defined, waiting for its first source: device-core sources in `src/device` build into a
C-ABI object that QEMU can link, under the same no-exceptions, no-RTTI,
no-standard-library rules and the same sanitizer, coverage and design gates as
the guest core.

**Why this priority**: Feature 026 depends on 004, not on 001's device target
directly, so this can trail the driver target. It still belongs here so the
three-target shape is decided once.

**Independent Test**: Add a device-core source that throws or includes a
hosted-only header; the entry point fails. Remove it; it passes.

**Acceptance Scenarios**:

1. **Given** device-core sources exist, **When** the build runs, **Then** they
   compile into a device-core library under the no-exceptions, no-RTTI rules
   and link into the unit tests. The QEMU-loadable object and its exported
   C-ABI symbol check arrive with feature 026, which defines that ABI.
2. **Given** a device-core source, **When** the entry point runs, **Then** it
   is included in the sanitizer, coverage and design gates exactly as a
   `src/core` source is.
3. **Given** `src/device` holds no sources (true at the end of this feature),
   **When** the build is configured, **Then** no device object is produced, no
   placeholder source exists to give it a body, and the gates pass; feature 026
   brings the target into existence with its first real source.

---

### User Story 4 - A new source file needs no build edits (Priority: P3)

A developer adds `src/core/foo.cpp`, `tests/unit/test_foo.cpp` and
`tests/fuzz/fuzz_foo.cpp` with a corpus directory. Without touching any build
file, the unit test, driver image, coverage gate and fuzz smoke all pick them
up.

**Why this priority**: The current hand-maintained source list in the fuzz
build is exactly how a file ends up covered by one gate and missed by another.

**Independent Test**: Add a core file with an uncovered branch and a fuzz
target, touch nothing else, run the entry point; both the coverage gate and the
fuzz smoke see the new file.

**Acceptance Scenarios**:

1. **Given** a new core source, **When** the build is reconfigured, **Then** the
   unit tests, the driver image and every fuzz target link against it with no
   build-file edit.
2. **Given** a fuzz target with no committed corpus directory, **When** the
   entry point runs, **Then** it fails: Principle IV.3 requires a committed seed
   corpus.

---

### User Story 5 - A clean checkout of this repository is all you need (Priority: P1)

A developer clones accel-virt alone - no knowledge base checkout beside it, no
other project in a sibling directory - and can build, test, read the design,
and rebuild the reverse-engineering workspace using only paths inside this
repository.

**Why this priority**: Principle IX requires a reproducible build from a clean
checkout with one command. A build that reads headers from
a sibling checkout is reproducible only on one machine.

**Independent Test**: Search every tracked file for a path that leaves the
repository; there is none. Clone into an empty directory, run the kit fetch
and the gate command; both succeed.

**Acceptance Scenarios**:

1. **Given** a clean clone with network access, **When** the kit fetch runs,
   **Then** it downloads the pinned packages, rejects any whose SHA-256 differs
   from the committed value, and leaves only the required header and library
   subset inside the repository's git-ignored kit directory.
2. **Given** a tracked file anywhere in the repository, **When** it is
   searched for references to the knowledge base checkout or a sibling
   project path, **Then** none is found, and a gate enforces this.
3. **Given** the design documents, **When** a spec or `CLAUDE.md` cites
   knowledge-base material, **Then** the citation resolves to a file inside
   `docs/`.
4. **Given** the RE manifest, **When** the workspace script runs, **Then** it
   fetches exactly the pinned binaries (by SHA-256) with their PDBs and imports
   them into a git-ignored Ghidra project inside the repository.

### Edge Cases

- The Windows kit (headers and import libraries) is absent on the machine:
  configuration fails, naming the path it looked in, the fetch command, and
  the setting that overrides the location. A green run always includes the
  kernel cross-build.
- A downloaded kit package has the wrong SHA-256 (truncated, tampered,
  republished): the fetch fails, deletes it, and leaves any existing kit
  untouched.
- The kit headers include each other with a case that differs from the file
  name on disk (they were authored for a case-insensitive filesystem): the
  cross-build resolves them without renaming or duplicating kit files.
- The compiler is not clang: configuration fails with the constitution
  reference (existing behaviour, kept).
- A test binary crashes under a sanitizer before writing its coverage profile:
  the coverage gate reports the missing profile as a failure, never computes a
  percentage over fewer binaries.
- No source files exist in `src/device` or `src/proto`: the coverage and design
  gates treat the empty directories as zero branches, not as an error.
- A fuzz smoke run is interrupted by its time budget: that is a pass; only a
  crash, leak or sanitizer report fails.
- `AV_UNTESTABLE` appears in a comment or string rather than as a mark: counted
  anyway; the gate is deliberately conservative.
- The coverage profile from a previous run is left in the build tree: stale
  profiles are discarded before a coverage run, so a deleted test cannot keep a
  branch "covered".

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: One source tree MUST produce native test binaries and the Windows
  kernel driver image `rvkgpu.sys`, and MUST define the rule that builds the
  QEMU device object from `src/device` sources once any exist. No placeholder
  device source is added (Principle VIII).
- **FR-002a**: Configuration MUST fail when the Windows kit is not found,
  naming the expected location and the setting that overrides it.
- **FR-002**: The driver image MUST be built from the same `src/core` sources as
  the unit tests, compiled for the Windows kernel with exceptions, RTTI and
  hosted standard headers unavailable.
- **FR-003**: The driver image MUST link with no unresolved symbol and MUST
  import only from kernel-provided modules; a test MUST verify the image's
  subsystem is native and list any non-kernel import as a failure.
- **FR-004**: `src/core` MUST compile under both the native test build and the
  kernel cross-build without conditional compilation that distinguishes the
  two.
- **FR-005**: The driver entry point MUST be a single-statement ABI boundary
  (Principle III exception) that declines to load until feature 006 replaces
  it.
- **FR-006**: One committed command MUST produce the full verdict: it builds
  the sanitized, coverage-instrumented and fuzzer-instrumented configurations
  and runs the test entry point in each, failing if any gate fails. The test
  entry point in the default (sanitized) configuration alone MUST run the unit
  tests under address and undefined-behaviour sanitizers, the design gate, the
  symbol-test gate and the kernel cross-build check. The coverage
  configuration's entry point MUST add the branch coverage gate and the
  `AV_UNTESTABLE` ratchet; the fuzz configuration's MUST add the fuzz smoke
  runs.
- **FR-007**: The coverage gate MUST measure branch coverage over every
  `.cpp` and `.hpp` under `src/core`, `src/device` and `src/proto`, excluding
  test code, and MUST fail below 100%.
- **FR-008**: The `AV_UNTESTABLE` ratchet MUST compare the mark count in `src/`
  against a committed baseline, fail when it rises, and list every mark with
  its location and reason.
- **FR-009**: The symbol-test rule (Principle IV.5) MUST be enforced by
  `tools/check_symbol_tests.py`, and the design rules (Principle III) by
  `tools/check_design.py`, so every document that names either gate is
  accurate.
- **FR-010**: Fuzz targets MUST link the core library rather than restating its
  source list, and each MUST have a committed corpus under
  `tests/corpus/<target>/`; a missing corpus MUST fail the gate.
- **FR-011**: Fuzz smoke runs MUST run each target against its corpus for the
  constitution's 60-second budget, and fail on any crash, leak or sanitizer
  report.
- **FR-012**: Adding a source file under `src/`, `tests/unit/` or `tests/fuzz/`
  MUST NOT require editing a build file.
- **FR-013**: The build MUST be reproducible from a clean checkout with the
  commands documented in `README.md`; every command there MUST work as written.
- **FR-015**: The Windows kit MUST be obtained by a committed fetch command
  that pins package identity, version and SHA-256, extracts only the needed
  subset into a git-ignored directory inside the repository, and never
  commits kit files.
- **FR-016**: No tracked file MUST reference a path outside the repository
  (the knowledge base checkout, sibling projects, or a user home directory).
  A gate MUST enforce this.
- **FR-017**: The knowledge base documents MUST live in `docs/kb/` and the
  reverse-engineering scripts in `tools/re/`; every reference to them MUST use
  those paths.
- **FR-018**: The reverse-engineering workspace MUST be reproducible from a
  committed manifest pinning each binary by file name and SHA-256, into a
  git-ignored directory inside the repository.
- **FR-019**: Regenerating spec headers MUST preserve a filled-in "Classes this
  feature introduces" table, so a plan's class decisions are not erased.
- **FR-014**: Every gate failure MUST print the value that caused it (file,
  line, symbol, count) once, at the point of decision (Principle IX).

### Key Entities

- **Driver image** - `rvkgpu.sys`; the guest-kernel artifact. Built from
  `src/core` plus the `src/km` shim.
- **Device object** - the QEMU-linkable artifact built from `src/device` behind
  a C ABI.
- **Gate** - a check whose failure fails the entry point: unit, design,
  symbol-test, coverage, untestable ratchet, kernel cross-build, fuzz smoke.
- **Untestable baseline** - the committed count of `AV_UNTESTABLE` marks the
  ratchet compares against.
- **Seed corpus** - committed inputs per fuzz target.
- **Windows kit** - the pinned WDK/SDK header and import-library subset,
  fetched, never committed.
- **RE manifest** - the pinned list of Microsoft binaries the reverse
  engineering workspace is built from.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On a clean checkout, the documented commands produce the test
  binaries and the driver image with zero manual steps beyond installing the
  documented toolchain and kit.
- **SC-002**: Each of the seven gates can be made to fail by a one-line change
  to a source or test file, and each failure message identifies the offending
  file, symbol or count without further investigation. Demonstrated once per
  gate.
- **SC-003**: The unit, design, symbol-test and cross-build gates complete in
  under 60 seconds on the development machine, so they are run on every change
  rather than batched.
- **SC-004**: After this feature, zero documents in the repository name a gate,
  script or command that does not exist or does not work as described.
- **SC-006**: A fresh clone into a directory with no sibling projects builds
  and passes every gate after the documented fetch and gate commands, with no
  edits.
- **SC-005**: The existing address-space module keeps 100% region, function,
  line and branch coverage through the change.

## Assumptions

- The Windows kit is NuGet `Microsoft.Windows.WDK.x64` 10.0.26100.6584 and
  `Microsoft.Windows.SDK.CPP` 10.0.26100.9169 (the versions the knowledge base
  was derived from). The subset is the kernel and shared headers plus
  `ntoskrnl.lib`, `hal.lib`, `displib.lib` and `BufferOverflowK.lib`; its
  location is overridable at configuration time.
- Network access is available the first time the kit is fetched.
- The toolchain is the one installed: clang 22, `clang-cl`, `lld-link`,
  `llvm-cov`, `llvm-profdata`, CMake, Ninja, Python 3.
- The QEMU source tree is not required by this feature. The C glue in
  `hw/display/` is built inside QEMU's own build by feature 026; this feature
  only builds the C-ABI object that glue links.
- Replacing hosted headers such as `<cstdint>` in core with freestanding
  equivalents is part of this feature, because it is the first thing the kernel
  cross-build rejects.
- The `MockPlatform`/`TestRegistry` harness is reused as is; improving it is
  feature 002.
- Loading the driver in a VM is out of scope (feature 006, M0). This feature
  proves the image is well-formed, not that Windows accepts it.

## Out of Scope

- `DriverEntry` calling `DxgkInitialize`, any DDI, and `KernelPlatform` (006,
  002).
- Test-signing, INF files, and deploying the image to a guest (006/035).
- Building or patching QEMU; the `hw/display/virtio-gpu-rvk*.c` glue (026).
- The DDI conformance harness (003) and loopback suite (034).
- Nightly long fuzz runs and corpus minimization jobs; only the 60-second smoke
  gate is in scope.
- Deciding how the command-serialization codec is brought into the
  repository (feature 032).
- Committing Microsoft binaries, PDBs or kit files to git.
- Amending stale wording in the constitution (Principle II "freestanding C11",
  IV.2 `<module>_invariants(const <module>_t *)`, VII `locks.h`). Recorded as a
  follow-up constitution PATCH amendment, not silently edited here.
