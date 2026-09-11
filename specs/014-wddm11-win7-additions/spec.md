# Feature Specification: WDDM 1.1 (Windows 7) additions

**Feature Branch**: `014-wddm11-win7-additions`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 014 · **Status**: `partial` · **Depends on**: 013
· **WDDM level**: WDDM 1.1 (Windows 7)

### Status meaning

Implement only the subset listed below. Everything else in this level is stubbed or skipped, and the stub is tested.

### DDI slots in scope (8)

| Slot | Field | Introduced |
|--:|---|---|
| 62 | `DxgkDdiDescribePageTable` | WIN7 |
| 63 | `DxgkDdiUpdatePageTable` | WIN7 |
| 64 | `DxgkDdiUpdatePageDirectory` | WIN7 |
| 65 | `DxgkDdiMovePageDirectory` | WIN7 |
| 66 | `DxgkDdiSubmitRender` | WIN7 |
| 67 | `DxgkDdiCreateAllocation2` | WIN7 |
| 68 | `DxgkDdiRenderKm` | WIN7 |
| 70 | `DxgkDdiQueryVidPnHWCapability` | WIN7 |

Slot numbers are indices into `DRIVER_INITIALIZATION_DATA` as laid out by the WDDM 3.2 headers; see `docs/ref/driver_init_table.md`. They are the fastest way to navigate an unsymbolized vendor driver.

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
| DDI record `DxgkargQueryVidPnHwCapabilityRecord` | DDI record | the fields of `DXGKARG_QUERYVIDPNHWCAPABILITY` | Overlays memory dxgkrnl owns, layout-proven |

No new object class: the one implemented DDI answers from the adapter's source
and target counts (`DisplaySources`).

### Test requirements

Constitution Principle III applies in full. Concretely, this feature is not done until:

- every branch of every function it adds to `src/core`, `src/device` or `src/proto` is covered, or marked `AV_UNTESTABLE` with a justification;
- each stateful module it adds exports an invariant function that tests call after **every** mutation;
- every function it adds that reads bytes from outside its own address space has a hostile-input table test and a libFuzzer target with a committed corpus;
- the allocation-failure sweep passes: with each allocation site failed in turn, no crash and no leak;
- IRQL annotations are present and the mock's IRQL checker is satisfied;
- the DDI conformance harness (feature 003) drives every slot above, including the abusive-but-legal sequences for it;
- `tools/check_symbol_tests.py` reports no untested exported symbol.

### Reference material

Knowledge base:

- [`docs/kb/08-wddm3x-delta.md`](../../docs/kb/08-wddm3x-delta.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11

- Q (agent): Which of the eight WIN7 slots are implemented? -> A: Only
  `DxgkDdiQueryVidPnHWCapability`. `DescribePageTable`, `UpdatePageTable`,
  `UpdatePageDirectory`, `MovePageDirectory`, `SubmitRender` and
  `CreateAllocation2` are `PVOID` placeholders in `DRIVER_INITIALIZATION_DATA`
  ("Extended for WDDM 2.0") with no defined contract: skipped, left NULL.
  `RenderKm` is GDI hardware acceleration, reached from `DXGCONTEXT::Render`
  only for a GDI context, which dxgkrnl creates only for a driver reporting
  `PresentationCaps.SupportKernelModeCommandBuffer`; this driver does not, so
  the slot is skipped.
- Q (agent): What does QueryVidPnHWCapability answer? -> A: This adapter
  performs no rotation, scaling, cloning, colour conversion, linked-adapter or
  remote output in the driver: every `D3DKMDT_VIDPN_HW_CAPABILITY` bit is 0.
  A null VidPn, or a source or target the adapter does not have, is
  `STATUS_INVALID_PARAMETER`.

## User Scenarios & Testing *(mandatory)*

Users: dxgkrnl's display mode manager, which asks what the hardware does to
each path of a functional VidPn.

### User Story 1 - The path's hardware capability (Priority: P1)

1. **Given** a functional VidPn and source 0, target 0, **Then** success and a
   capability of 0, whatever the output held before.
2. **Given** a null VidPn, source 1 or target 1, or no argument, **Then**
   `STATUS_INVALID_PARAMETER` and the output untouched.

### User Story 2 - Skipped placeholders (Priority: P2)

1. The seven skipped slots are NULL in the registered table and dxgkrnl still
   accepts it.

## Requirements *(mandatory)*

- **FR-001**: No capability changes.

## Success Criteria *(mandatory)*

- **SC-001**: Through the table after a mode set, the capability query answers
  0; registration and the render core are unaffected.
- **SC-002**: 100% branch coverage.

## Out of Scope

- GDI hardware acceleration (RenderKm), the WDDM 2.0 placeholders.
