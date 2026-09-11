# Feature Specification: WDDM 3.1: doorbells and native fences

**Feature Branch**: `024-wddm31-doorbells-native-fences`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 024 · **Status**: `implement` · **Depends on**: 023
· **WDDM level**: WDDM 3.1 (23H2)

User-mode submission. The performance story: the guest ICD writes its ring and
stores to a doorbell with no kernel transition, and the KMD leaves the fast
path.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### DDI slots in scope (9)

| Slot | Field | Introduced |
|--:|---|---|
| 162 | `DxgkDdiCreateNativeFence` | WDDM3_1 |
| 163 | `DxgkDdiDestroyNativeFence` | WDDM3_1 |
| 164 | `DxgkDdiUpdateMonitoredValues` | WDDM3_1 |
| 165 | `DxgkDdiUpdateCurrentValuesFromCpu` | WDDM3_1 |
| 166 | `DxgkDdiCreateDoorbell` | WDDM3_1 |
| 167 | `DxgkDdiConnectDoorbell` | WDDM3_1 |
| 168 | `DxgkDdiDisconnectDoorbell` | WDDM3_1 |
| 169 | `DxgkDdiDestroyDoorbell` | WDDM3_1 |
| 170 | `DxgkDdiNotifyWorkSubmission` | WDDM3_1 |

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
| `DriverCapabilities` (extended) | object | WDDM 3.1 | Reports what the driver implements |

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

- [`docs/kb/05-scheduling.md`](../../docs/kb/05-scheduling.md)
- [`docs/kb/08-wddm3x-delta.md`](../../docs/kb/08-wddm3x-delta.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11

- Q (agent, RE): What turns native GPU fences on? -> A: `SchedulingCaps.NativeGpuFence`
  (0x800), with `DXGK_FEATURE_NATIVE_FENCE` (37) granted through
  `DxgkCbQueryFeatureSupport` (else start fails, `DXGADAPTER::Initialize
  @ 1401a2818`) and `DXGKQAITYPE_NATIVE_FENCE_CAPS` (0x38 bytes) answered. VidMm
  owns the current and monitored value pages; the driver allocates no storage.
  Waits reach the device only through hardware queue packets, so in practice
  native fences need hardware scheduling. `UpdateMonitoredValues` and
  `UpdateCurrentValuesFromCpu` run at device IRQL, take no adapter handle, and
  bugcheck 0x119 on any failure; whether the driver or the OS writes the value
  through the CPU address is not determined.
- Q (agent, RE): What turns doorbells on? -> A: A node flagged for user-mode
  submission, which `ADAPTER_RENDER::Initialize @ 1401b3d4c` accepts only from
  driver version 3100, with `ContextSchedulingSupported`, `NativeGpuFence`,
  `DXGK_FEATURE_USER_MODE_SUBMISSION` granted, all of slots 166-170 registered
  and `DXGKQAITYPE_USERMODESUBMISSION_CAPS` answered. `ConnectDoorbell` must
  answer a physically contiguous doorbell page, which `MmRotatePhysicalView` maps
  into the process; `NotifyWorkSubmission` comes from user mode.
- Q (agent): Then what does this feature build? -> A: WDDMVersion 3.1 only; the
  nine slots stay NULL, each NULL-checked or reached only once a node claims
  user-mode submission. Both mechanisms stand on hardware scheduling, which
  dxgkrnl refuses without a user-mode driver submitting to hardware queues
  (feature 021), and the doorbell page is a page of the device's doorbell BAR,
  whose layout feature 030 defines. The enablement checklists above are the
  specification for the feature that turns hardware scheduling on.
- Q (agent, RE): WDDMVersion 3.1? -> A: Reported. A sweep of every compare
  against 3.1 finds nothing mandatory for this capability set: external outputs
  honour virtual-refresh-rate target modes, and scanout re-enable stops
  suspending the adapter.

## User Scenarios & Testing *(mandatory)*

Users: dxgkrnl's adapter start.

### User Story 1 - Starting a WDDM 3.1 adapter (Priority: P1)

1. **Given** start, **Then** DRIVERCAPS reports 3.1 without `NativeGpuFence`,
   every check passes, and the nine slots are NULL.

## Requirements *(mandatory)*

- **FR-001**: The only capability change is WDDMVersion 3.1.

## Success Criteria *(mandatory)*

- **SC-001**: Through the table: start at 3.1 with the nine slots NULL, a hardware
  queue still created and destroyed, no violation.
- **SC-002**: 100% branch coverage.

## Out of Scope

- Native GPU fences and doorbells until hardware scheduling is enabled; the
  doorbell BAR (030).
