# Feature Specification: WDDM 2.2: hardware contexts and queues

**Feature Branch**: `019-wddm22-hardware-queues`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 019 · **Status**: `implement` · **Depends on**: 018
· **WDDM level**: WDDM 2.2 (1703)

Hardware scheduling: the driver owns a queue the device drains, instead of
VidSch arbitrating one packet at a time.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### DDI slots in scope (19)

| Slot | Field | Introduced |
|--:|---|---|
| 110 | `DxgkDdiCreateHwContext` | WDDM2_2 |
| 111 | `DxgkDdiDestroyHwContext` | WDDM2_2 |
| 112 | `DxgkDdiCreateHwQueue` | WDDM2_2 |
| 113 | `DxgkDdiDestroyHwQueue` | WDDM2_2 |
| 114 | `DxgkDdiSubmitCommandToHwQueue` | WDDM2_2 |
| 115 | `DxgkDdiSwitchToHwContextList` | WDDM2_2 |
| 116 | `DxgkDdiResetHwEngine` | WDDM2_2 |
| 117 | `DxgkDdiCreatePeriodicFrameNotification` | WDDM2_2 |
| 118 | `DxgkDdiDestroyPeriodicFrameNotification` | WDDM2_2 |
| 119 | `DxgkDdiSetTimingsFromVidPn` | WDDM2_2 |
| 120 | `DxgkDdiSetTargetGamma` | WDDM2_2 |
| 121 | `DxgkDdiSetTargetContentType` | WDDM2_2 |
| 122 | `DxgkDdiSetTargetAnalogCopyProtection` | WDDM2_2 |
| 123 | `DxgkDdiSetTargetAdjustedColorimetry` | WDDM2_2 |
| 124 | `DxgkDdiDisplayDetectControl` | WDDM2_2 |
| 125 | `DxgkDdiQueryConnectionChange` | WDDM2_2 |
| 126 | `DxgkDdiExchangePreStartInfo` | WDDM2_2 |
| 127 | `DxgkDdiGetMultiPlaneOverlayCaps` | WDDM2_2 |
| 128 | `DxgkDdiGetPostCompositionCaps` | WDDM2_2 |

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
| | object / system | | |

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

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11

- Q (agent, RE): Which of the nineteen slots does dxgkrnl reach at WDDM 2.2?
  -> A: None. `DpiGetAdapterInfo @ 14024eb50` never copies 110
  (`CreateHwContext`), 111 (`DestroyHwContext`) or 115
  (`SwitchToHwContextList`). Hardware queues (112-114, 116) need a node with
  `DXGK_NODEMETADATA.Flags.ContextSchedulingSupported` (read from DDI interface
  0x9000, `ADAPTER_RENDER::Initialize @ 1401b3d4c`), GPU VA, the OS allowing
  hardware scheduling through `DxgkCbQueryFeatureSupport(DXGK_FEATURE_HWSCH)`
  and `HwSchMode`, and a context created with `HwQueueSupported`, which
  `DXGCONTEXT::Initialize @ 1402dc810` refuses below driver version 2500.
  `SubmitCommandVirtual` stays the submission path until then.
- Q (agent): Then what does this feature build? -> A: The hardware queue model
  (`CreateHwQueue`, `DestroyHwQueue`, `SubmitCommandToHwQueue`, `ResetHwEngine`),
  registered and tested through the harness's model of dxgkrnl's hardware
  queue objects, but not enabled: no node claims context scheduling and the
  feature callback is not called. Enabling it arrives with the driver version
  that permits it (feature 020). Doorbells (024) build on these queues.
- Q (agent, RE): The display slots? -> A: Left NULL, each safe:
  `CreatePeriodicFrameNotification` (NULL-checked; VidSch falls back to a
  software timer) with `DestroyPeriodicFrameNotification` (only for
  notifications the driver created); `SetTimingsFromVidPn` (opt-in, needs
  `SetVidPnSourceAddressWithMultiPlaneOverlay3`; `CommitVidPn` stays),
  `DisplayDetectControl` and `QueryConnectionChange` (only in the
  connection-change mode `SetTimingsFromVidPn` turns on); `SetTargetGamma` (no
  `GammaRampCaps`, and `UpdateActiveVidPnPresentPath` is registered);
  `SetTargetContentType`, `SetTargetAnalogCopyProtection` (NULL gets a default
  answering NOT_SUPPORTED); `SetTargetAdjustedColorimetry` (NULL-checked,
  integrated panels); `ExchangePreStartInfo` (NULL is success with no output);
  `GetMultiPlaneOverlayCaps`, `GetPostCompositionCaps` (need overlays and slot
  105). `CreateHwContext`, `DestroyHwContext`, `SwitchToHwContextList` are never
  copied.
- Q (agent, RE): WDDMVersion 2.2? -> A: Reported. No DDI, query or capability
  becomes mandatory. `ADAPTER_DISPLAY::Initialize @ 1403abf44` then assumes
  `VirtualModeSupport` when `DXGKQAITYPE_DISPLAY_DRIVERCAPS_EXTENSION` (16) is
  not answered, so the driver answers it with no extension claimed.

## User Scenarios & Testing *(mandatory)*

Users: dxgkrnl's adapter and display core initialization; VidSch's hardware
queue objects once hardware scheduling is enabled.

### User Story 1 - Starting a WDDM 2.2 adapter (Priority: P1)

1. **Given** start, **Then** DRIVERCAPS reports 2.2 and the display caps
   extension answers no extension, so dxgkrnl assumes no virtual modes.

### User Story 2 - Hardware queues (Priority: P1)

1. **Given** a context, **When** dxgkrnl creates a hardware queue on it with its
   progress fence, **Then** the driver answers a queue handle; submissions to it
   complete through the DPC in order; destroying the queue releases it; a queue
   on an unknown context, a second destroy or a submission to an unknown queue
   is refused.
2. ResetHwEngine abandons what the engine's queues had submitted.

## Requirements *(mandatory)*

- **FR-001**: Nothing enables hardware scheduling; the display slots and the
  three uncopied slots are NULL.
- **FR-002**: The submission path allocates nothing and takes no mutex.

## Success Criteria *(mandatory)*

- **SC-001**: Through the table: start at 2.2; hardware queues created,
  submitted to, reset and destroyed by the harness's queue model; no leak or
  violation.
- **SC-002**: 100% branch coverage.

## Out of Scope

- Enabling hardware scheduling (020), periodic frame notifications, timings from
  VidPn and connection change, gamma and colorimetry, overlay caps.
