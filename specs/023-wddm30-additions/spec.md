# Feature Specification: WDDM 3.0: backing stores and CPU events

**Feature Branch**: `023-wddm30-additions`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 023 · **Status**: `partial` · **Depends on**: 022
· **WDDM level**: WDDM 3.0 (22H2)

### Status meaning

Implement only the subset listed below. Everything else in this level is stubbed or skipped, and the stub is tested.

### DDI slots in scope (4)

| Slot | Field | Introduced |
|--:|---|---|
| 158 | `DxgkDdiSetAllocationBackingStore` | WDDM3_0 |
| 159 | `DxgkDdiCreateCpuEvent` | WDDM3_0 |
| 160 | `DxgkDdiDestroyCpuEvent` | WDDM3_0 |
| 161 | `DxgkDdiCancelFlips` | WDDM3_0 |

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
| `DriverCapabilities` (extended) | object | WDDM 3.0 | Reports what the driver implements |

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

- Q (agent, RE): Which of the four 3.0 slots? -> A: None; all NULL.
  `SetAllocationBackingStore` (NULL-checked) serves only allocations with
  `Flags2.ShareBackingStoreWithKmd`, which this driver never sets.
  `CreateCpuEvent` (NULL-checked; a SignalByKmd CPU notification sync object then
  fails NOT_SUPPORTED) and `DestroyCpuEvent` (called only for an event 159
  created) wait for a device that signals CPU events. `CancelFlips` is
  NULL-checked and reached, like the 2.9 flip queue slots, only with the
  hardware flip queue. `basicrender.sys` fills none of them.
- Q (agent, RE): WDDMVersion? -> A: 3.0. `DXGDEVICE::CreateDriverAllocations
  @ 1402f2220` then refuses `Flags2` above 0x3f (the driver sends 0), and VidMm
  requires a CPU-visible allocation to include an aperture segment, a test of
  segment flags 0x1001 the implicit system memory segment (flags 0x1190) passes -
  inferred from the decompile, to be confirmed in a VM (feature 035). Nothing
  else becomes mandatory.

## User Scenarios & Testing *(mandatory)*

Users: dxgkrnl's adapter start and allocation creation.

### User Story 1 - Starting a WDDM 3.0 adapter (Priority: P1)

1. **Given** start, **Then** DRIVERCAPS reports 3.0, every check passes, the four
   slots are NULL, and a device creates and destroys a CPU-visible allocation.

## Requirements *(mandatory)*

- **FR-001**: The only capability change is WDDMVersion 3.0; allocations keep
  `Flags2` zero.

## Success Criteria *(mandatory)*

- **SC-001**: Through the table: start at 3.0 with the four slots NULL, a primary
  allocated and destroyed, no leak or violation.
- **SC-002**: 100% branch coverage.

## Out of Scope

- Shared backing stores, KMD-signalled CPU events, flip cancellation.
