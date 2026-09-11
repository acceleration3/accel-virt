# Feature Specification: WDDM 1.0: VidPn topology and mode enumeration

**Feature Branch**: `011-wddm10-vidpn-and-modes`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 011 · **Status**: `implement` · **Depends on**: 006
· **WDDM level**: WDDM 1.0 (Windows Vista)

The display graph: sources, targets, paths, mode sets, and the cofunctional
modality enumeration that silently breaks mode changes when under-populated.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### DDI slots in scope (9)

| Slot | Field | Introduced |
|--:|---|---|
| 37 | `DxgkDdiIsSupportedVidPn` | BASE |
| 38 | `DxgkDdiRecommendFunctionalVidPn` | BASE |
| 39 | `DxgkDdiEnumVidPnCofuncModality` | BASE |
| 42 | `DxgkDdiCommitVidPn` | BASE |
| 43 | `DxgkDdiUpdateActiveVidPnPresentPath` | BASE |
| 44 | `DxgkDdiRecommendMonitorModes` | BASE |
| 45 | `DxgkDdiRecommendVidPnTopology` | BASE |
| 46 | `DxgkDdiGetScanLine` | BASE |
| 48 | `DxgkDdiControlInterrupt` | BASE |

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
| `DisplayMode` | object | width, height | Describes one scan-out resolution in dxgkrnl's mode records |
| `DisplayModeList` | object | the modes the adapter can scan out, the preferred one | Names the resolutions the one output supports |
| `Edid` | object | the 128-byte EDID 1.3 block for the preferred mode | Answers QueryDeviceDescriptor for the synthetic monitor |
| `VidPn` | object | platform, VidPn handle, its interface, topology handle and interface | Speaks dxgkrnl's VidPn and topology interfaces for one VidPn |
| `SourceModeSet` / `TargetModeSet` | object | platform, the VidPn, mode set handle, its interface | Speaks one source (target) mode set interface |
| `VidPnSupportCheck` | object | platform, modes, one `DXGKARG_ISSUPPORTEDVIDPN` | Decides whether one proposed VidPn is achievable |
| `CofuncModality` | object | platform, modes, one `DXGKARG_ENUMVIDPNCOFUNCMODALITY` | Fills one VidPn's unpinned mode sets |
| `VidPnCommit` | object | platform, display sources, one `DXGKARG_COMMITVIDPN` | Applies one functional VidPn to its source |
| `MonitorModes` | object | platform, modes, one `DXGKARG_RECOMMENDMONITORMODES` | Adds the adapter's modes to one monitor's mode set |
| `DisplaySources` (extended) | object | + committed mode, visibility, vsync enabled, per source | Remembers each source's display state |
| DDI records | DDI record | the fields of one WDK structure or interface table | Overlays memory dxgkrnl owns, layout-proven |
| `FakeVidPn` (harness) | object | paths, per-source and per-target mode sets, pinned modes | dxgkrnl's VidPn object and its interfaces |

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

- [`docs/kb/06-display.md`](../../docs/kb/06-display.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11

- Q (agent, RE): What is the minimum VidPn implementation dxgkrnl accepts? ->
  A: `basicdisplay.sys`, one source and one target:
  `BASIC_DISPLAY_DRIVER::IsSupportedVidPn` accepts a null VidPn and any VidPn
  whose source has at most one path; `EnumVidPnCofuncModality` walks every path
  and, for each mode set that is neither the pivot nor pinned, replaces it with
  the adapter's modes (target modes follow a pinned source mode), then updates
  the path's scaling and rotation support; `CommitVidPn` reads the pinned
  source mode for the affected source and records it (nothing when
  `PathPoweredOff`); `RecommendFunctionalVidPn` answers
  `STATUS_GRAPHICS_NO_RECOMMENDED_FUNCTIONAL_VIDPN`; `RecommendMonitorModes`
  adds the adapter's mode as a driver-origin preferred monitor source mode;
  `UpdateActiveVidPnPresentPath` validates the path and stores its
  transformation. Iteration ends on `STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET`
  (0x401E034C, a success); a duplicate add answers
  `STATUS_GRAPHICS_MODE_ALREADY_IN_MODESET` and is not an error.
- Q (agent): Where do the modes come from before the device reports displays?
  -> A: A fixed list of common resolutions up to 1920x1080 at 60 Hz, 1280x800
  preferred (QEMU virtio-gpu's default), 32-bpp X8R8G8B8 sources. The EDID is
  synthesized for the preferred mode. Feature 027 (device display) can report
  the host's preferred mode; the list is then the device's answer.
- Q (agent): RecommendVidPnTopology? -> A: Answers
  `STATUS_GRAPHICS_NO_RECOMMENDED_VIDPN_TOPOLOGY`, so dxgkrnl builds the one
  source-to-one target topology itself.
- Q (agent): ControlInterrupt and GetScanLine without a scanout timer? -> A:
  ControlInterrupt records whether CRTC_VSYNC is enabled (the synthetic vsync
  that consumes it arrives with 012) and refuses other types; GetScanLine
  derives the line from interrupt time and the committed mode at 60 Hz, with
  the last 45 of 1125 lines as vertical blank (1080p's blanking ratio).

## User Scenarios & Testing *(mandatory)*

Users: dxgkrnl's display mode manager and CDD, which build, validate and commit
VidPns; Windows' monitor stack, which reads the EDID.

### User Story 1 - The synthetic monitor (Priority: P1)

1. **Given** QueryDeviceDescriptor for child 0 with offset 0 and 128 bytes,
   **Then** a valid EDID 1.3 block (header, checksum) whose preferred timing is
   1280x800; a shorter length copies that many bytes; an offset at or past 128
   answers `STATUS_MONITOR_NO_MORE_DESCRIPTOR_DATA`.

### User Story 2 - Validating and enumerating VidPns (Priority: P1)

1. **Given** IsSupportedVidPn with no VidPn, **Then** supported.
2. **Given** a VidPn with the one path, **Then** supported; a source with two
   paths, or a path to a target the adapter does not have, **Then** not
   supported (success status).
3. **Given** EnumVidPnCofuncModality with nothing pinned and no pivot,
   **Then** the source and target mode sets hold every mode, and the path
   supports identity scaling and rotation.
4. **Given** a pinned source mode, **Then** the target mode set holds the
   matching mode only; a pinned mode set is left alone; the pivot's mode set is
   left alone.

### User Story 3 - Committing (Priority: P1)

1. **Given** CommitVidPn with a pinned 1024x768 X8R8G8B8 source mode, **Then**
   source 0 records that mode; a mode not in the list or a pixel format the
   adapter does not scan out is `STATUS_GRAPHICS_INVALID_VIDEO_PRESENT_SOURCE_MODE`.
2. **Given** `PathPoweredOff`, **Then** success with no change.
3. **Given** UpdateActiveVidPnPresentPath with identity scaling and rotation,
   **Then** success; anything else is
   `STATUS_GRAPHICS_PATH_CONTENT_GEOMETRY_TRANSFORMATION_NOT_SUPPORTED`.

### User Story 4 - The rest (Priority: P2)

1. RecommendMonitorModes adds every mode, the preferred one marked preferred;
   a mode already present is not an error.
2. RecommendFunctionalVidPn and RecommendVidPnTopology decline as above.
3. ControlInterrupt CRTC_VSYNC on/off is recorded; other types are
   `STATUS_NOT_SUPPORTED`.
4. GetScanLine for target 0 answers a line within the committed height or in
   vertical blank; another target is `STATUS_INVALID_PARAMETER`.

## Requirements *(mandatory)*

- **FR-001**: Every mode info and path the driver acquires MUST be released on
  every path out (RAII), and the fake VidPn MUST fail a test on a leak.
- **FR-002**: Every structure read from dxgkrnl's VidPn MUST be checked
  (source and target ids, pixel format, sizes) before use.
- **FR-003**: No capability changes; the render core rule's first missing slot
  becomes Present.

## Success Criteria *(mandatory)*

- **SC-001**: Scenario through the table: descriptor, IsSupported,
  EnumCofuncModality, pin, Commit, monitor modes - no leak, no violation.
- **SC-002**: 100% branch coverage.

## Out of Scope

- Scanning out and vsync interrupts (012), hot plug and multiple outputs,
  modes from the device (027).
