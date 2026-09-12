# Feature Specification: Real-guest bring-up: the in-guest suite passes

**Feature Branch**: `037-guest-bring-up`

**Created**: 2026-09-11

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 037 · **Status**: `implement` · **Depends on**: 036

rvkgpu.sys starts on the real dxgkrnl under Driver Verifier and rvktest.exe
passes. Every disagreement between the fake dxgkrnl and Windows that L5
exposes is first reproduced as a harness scenario, then fixed in core; the
driver logs every refusal it makes at the point of decision.

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
| `LogBudget` | object | Lines written in the current window, the window's start, its `Platform` | Answers whether another log line may be written now |

The rest of this feature is not new classes: it is log lines at decisions that
already exist, L2 scenarios reproducing what Windows exposes, and fixes inside
the classes features 006 to 025 already introduced. A fix that does need a new
class adds its row here before the code is written.

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

- [`docs/kb/02-kmd-contract.md`](../../docs/kb/02-kmd-contract.md)
- [`docs/kb/10-warp-kmd.md`](../../docs/kb/10-warp-kmd.md)
- [`docs/kb/12-re-cookbook.md`](../../docs/kb/12-re-cookbook.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## User Scenarios & Testing *(mandatory)*

Users: the driver's developers, and every later feature - nothing above the KMD
can be tested until `rvkgpu.sys` runs for real. The harness (feature 003)
encodes beliefs about `dxgkrnl`; this feature is where Windows says which ones
were wrong. It has already said one: lld-link marked the image
`IMAGE_DLL_CHARACTERISTICS_TERMINAL_SERVER_AWARE`, and `MiObtainSectionForDriver`
refused it with `STATUS_CONFLICTING_ADDRESSES` (Code 39) before `DriverEntry`
ran. That is fixed; the rest are not yet known.

### User Story 1 - The driver starts on the real dxgkrnl (Priority: P1)

**Independent Test**: `tools/guest/run_guest_suite.py` against the prepared
image: the check stage passes, and `diag/` shows no start failure.

**Acceptance Scenarios**:

1. **Given** the prepared image and features 035 and 036, **When** the runner
   installs the package with Driver Verifier's standard checks and reboots,
   **Then** the device reports status OK, problem code 0 and service `rvkgpu`,
   and outranks `viogpudo` for the rvk subsystem id.
2. **Given** the started adapter, **Then** `rvkgpu.log` shows `DriverEntry`,
   `AddDevice`, `StartDevice` and the capability queries in order, and
   `diag/kernel-pnp.txt` holds no Kernel-PnP 411 for the device.
3. **Given** the started adapter, **When** `D3DKMTEnumAdapters2` runs, **Then**
   the adapter is listed with WDDM 3.2 and its PCI identity.
4. **Given** a run that bugchecks, **Then** the runner names the bugcheck, the
   kernel dump reaches `diag/dumps/` with the installed `rvkgpu.pdb` beside it,
   and the feature is not done.

### User Story 2 - The in-guest suite passes (Priority: P1)

**Independent Test**: the runner's verdict is 0 on three consecutive runs.

**Acceptance Scenarios**:

1. **Given** the started adapter, **When** `rvktest.exe` runs, **Then** the
   adapter, allocation, GPU mapping and submission scenarios all pass under
   Driver Verifier, with no Verifier violation and no TDR.
2. **Given** the run, **Then** `diag/dumps/` holds no live kernel report caused
   by `rvkgpu` (the `0x1A8` watchdog report at the display handoff is
   explained and either eliminated or shown to come from the outgoing driver).

### User Story 3 - Every disagreement becomes a harness scenario first (Priority: P1)

**Independent Test**: each fix lands with an L2 scenario that fails on the
commit before it.

**Acceptance Scenarios**:

1. **Given** L5 exposes a DDI order, argument or field value the fake
   `dxgkrnl` never produced, **When** it is fixed, **Then** an L2 scenario
   reproduces it first, and the harness change cites the `dxgkrnl.sys` function
   it was read from.
2. **Given** a kernel refusal before `DriverEntry`, **Then** its cause becomes a
   `driver_image` gate rule citing the `ntoskrnl` function, as
   `TERMINAL_SERVER_AWARE` did.

### User Story 4 - Refusals are visible (Priority: P2)

**Independent Test**: L1 tests assert the log line for each refusal path over
`MockPlatform`.

**Acceptance Scenarios**:

1. **Given** any DDI path that returns a failing NTSTATUS, **Then** exactly one
   line names the DDI, the status and the value that caused it.

### Edge Cases

- A Verifier violation (`0xC4`) or IRQL bugcheck (`0xD1`) at start: named,
  dumped, reproduced at L2 before the fix.
- A bugcheck on every boot: the runner's rescue boot recovers the dumps.
- `DxgkInitialize` failing, or `dxgkrnl` rejecting the DDI table or
  `DRIVERCAPS`: logged with the status, reproduced in the harness.
- A TDR during start from a query the driver answers slowly.
- Interrupt messages fewer than the INF's `MessageNumberLimit`, or none.
- Logging from the ISR or DPC under load: bounded, never at unbounded rate.
- Basic Display taking the device back after a failed start.

## Requirements *(mandatory)*

- **FR-001**: A mismatch found in the guest is fixed only with an L2 scenario
  that reproduces it, citing the `dxgkrnl` function behind the corrected belief.
- **FR-002**: Every property of the image the kernel checks before
  `DriverEntry` that the build once got wrong is a `driver_image` gate rule.
- **FR-003**: Every refusal is logged once at the point of decision with the
  value that caused it (constitution IX), rate-bounded at DISPATCH and above.
- **FR-004**: A pass counts only with Driver Verifier's standard checks active
  on `rvkgpu.sys`.
- **FR-005**: No capability is reported that the in-guest run has not
  exercised (constitution VI).
- **FR-006**: From this feature on, `tools/gate.sh` with `AV_GUEST_IMAGE` set
  must pass before a feature merges.

## Success Criteria *(mandatory)*

- **SC-001**: Three consecutive in-guest runs end with verdict 0.
- **SC-002**: Every fix in this feature has an L2 scenario and a citation.
- **SC-003**: No Kernel-PnP start error, bugcheck or Verifier violation in any
  of those runs' `diag/`.

## Assumptions

- The prepared image is Windows 11 build 26200 on the host's libvirt disk; the
  device runs with the echo executor.
- The desktop still composites on whatever display the guest has; driving it
  through `rvkgpu` is feature 038.

## Out of Scope

- The desktop through `rvkgpu` (038), the real executor (042), power and
  recovery (040).
- Production signing.
