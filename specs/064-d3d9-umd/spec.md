# Feature Specification: Direct3D 9: the user-mode driver

**Feature Branch**: `064-d3d9-umd`

**Created**: 2026-09-11

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 064 · **Status**: `implement` · **Depends on**: 055

The D3D9 UMD DDI in rvkumd.dll over Vulkan remoting - resources, fixed
function and shader model 1-3 translated to SPIR-V, state blocks, draws, locks
and presents - so Direct3D 9 applications, and DirectDraw applications Windows
runs on top of it, use the host GPU through the Microsoft runtime without DXVK
placed beside them.

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

- [`docs/kb/07-usermode.md`](../../docs/kb/07-usermode.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## User Scenarios & Testing *(mandatory)*

Users: people running Direct3D 9 and DirectDraw software on the guest - older
games, installers, tools and applications that link the system `d3d9.dll` and
never ship DXVK - who expect it to run on the GPU the guest was given rather
than fail to create a device or fall back to software.

### User Story 1 - A Direct3D 9 device on rvkgpu through the system runtime (Priority: P1)

**Independent Test**: with the driver package installed and no `d3d9.dll`
beside the test program, `Direct3DCreate9` enumerates the rvkgpu adapter, and
`CreateDevice` with hardware vertex processing succeeds; the device's caps
match what the host GPU supports through the Vulkan path, and the same program
on WARP produces frames that compare within tolerance.

**Acceptance Scenarios**:

1. **Given** the adapter, **When** the D3D9 runtime opens it, **Then**
   `rvkumd.dll` answers the D3D9 adapter open and reports `D3DCAPS9` that
   describe only what is implemented and tested: shader model 3.0 when complete,
   the texture formats, multisample types and presentation intervals the host
   GPU backs.
2. **Given** a device, **When** a program creates vertex and index buffers,
   textures, surfaces and render targets, locks them, draws with the fixed
   function pipeline and with vertex and pixel shaders of model 1.x to 3.0,
   and presents, **Then** each frame matches the reference renderer within
   tolerance.
3. **Given** a windowed or fullscreen device, **When** it is reset, lost by a
   mode change or Alt-Tab and restored, **Then** `TestCooperativeLevel`,
   `Reset` and resource recreation follow the D3D9 contract for
   `D3DPOOL_DEFAULT` and `D3DPOOL_MANAGED` resources.
4. **Given** `IDirect3D9Ex`, **When** a program creates a device with shared
   surfaces, **Then** sharing and the flip-model present behave as on a
   hardware driver, using the cross-process sharing of feature 053.

### User Story 2 - DirectDraw and 32-bit programs (Priority: P1)

**Independent Test**: a 32-bit DirectDraw program and a 32-bit Direct3D 9 game
run under WoW64 on the guest and render their reference frames through the
32-bit `rvkumd.dll`.

**Acceptance Scenarios**:

1. **Given** a DirectDraw program, **When** Windows maps it onto Direct3D 9,
   **Then** it renders through rvkgpu with no DirectDraw-specific code in the
   driver.
2. **Given** a 32-bit process, **When** it creates a D3D9 device, **Then** the
   WoW64 UMD loads, maps host-visible memory below 4 GiB and renders the same
   frames as the 64-bit build.

### User Story 3 - Hostile and broken D3D9 input fails safely (Priority: P2)

**Independent Test**: malformed shader bytecode, out-of-range state values,
locks with bad rectangles and resource sizes that overflow are fed to the UMD
from a test program; each call returns the D3D9 error the runtime expects and
nothing reaches the host that the executor would fault on.

### Edge Cases

- A shader using a construct the translator does not support: device creation
  still succeeds, `CreateVertexShader`/`CreatePixelShader` fails with
  `D3DERR_INVALIDCALL`, and the capability is not reported.
- A host GPU without a feature a D3D9 cap depends on (a texture format, a
  multisample count): the cap is not reported.
- `D3DCREATE_SOFTWARE_VERTEXPROCESSING` and mixed vertex processing: software
  transform is done in the guest and the results drawn on the GPU.
- Locking a render target with `D3DLOCK_READONLY` every frame: correct pixels
  come back; the cost is measured, not hidden.
- TDR or device removal: the device reports `D3DERR_DEVICELOST` /
  `D3DERR_DEVICEREMOVED` and a reset after the adapter recovers succeeds.
- Gamma ramps and palettized formats: gamma is declined with the documented
  cap; 8-bit palettized surfaces used by DirectDraw programs are converted.
- Several D3D9 devices in one process and across processes on one adapter.

## Requirements *(mandatory)*

- **FR-001**: `rvkumd.dll` (x64 and x86) implements the D3D9 UMD DDI entries the
  runtime calls, the minimum set derived by reading `d3d9.dll`'s calls, not
  guessed (docs/kb/12-re-cookbook.md).
- **FR-002**: Every `D3DCAPS9` field and every supported format is reported
  only when implemented and backed by a passing test (constitution VI).
- **FR-003**: Fixed-function state and shader models 1.x-3.0 are translated
  to SPIR-V and executed through the Vulkan remoting path of features 043-047;
  the D3D9 UMD adds no new wire protocol.
- **FR-004**: Every value the runtime hands the UMD from the application -
  bytecode, rectangles, sizes, state values - is validated before use and
  refused with the D3D9 error the runtime expects.
- **FR-005**: Allocation private data the UMD sends the KMD is the same
  validated descriptor format the other drivers use.
- **FR-006**: Device loss, reset and managed-resource restoration follow the
  D3D9 contract, tested with injected mode changes and TDRs.

## Success Criteria *(mandatory)*

- **SC-001**: A D3D9 conformance subset (the HLK D3D9 graphics tests used as an
  unsigned oracle, per feature 059) passes on the adapter, and the pass rate is
  held as a ratchet.
- **SC-002**: A fixed set of 32-bit and 64-bit Direct3D 9 and DirectDraw
  programs render their reference frames within tolerance in the in-guest
  runner.
- **SC-003**: No reported cap lacks a test; the unit tests for translation and
  validation cover every branch of the UMD's portable logic.

## Assumptions

- The D3D9 runtime on Windows 11 still loads a WDDM UMD's D3D9 interface from
  the adapter's `UserModeDriverName` registration, as the Microsoft runtimes do
  for WDDM 1.x and later drivers.
- DirectDraw on Windows 11 is served through Direct3D 9, so no DirectDraw DDI is
  needed.

## Out of Scope

- Direct3D 8 and earlier through the Microsoft runtime beyond what Windows maps
  onto Direct3D 9; they run through DXVK over the Vulkan ICD (feature 047).
- Video acceleration (DXVA) through the D3D9 interface.
