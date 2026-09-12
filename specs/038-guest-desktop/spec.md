# Feature Specification: Real-guest desktop: DWM on rvkgpu at a chosen mode

**Feature Branch**: `038-guest-desktop`

**Created**: 2026-09-11

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 038 · **Status**: `implement` · **Depends on**: 037

M1 in a real guest: the Windows desktop runs through rvkgpu, composited by DWM
on WARP, with modes from the EDID, synthetic vsync, VidPn commit and scanout
through the 2D path; mode changes, display power and several outputs work, and
the host sees every frame.

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
| `ScanoutResource` | object | its virtio resource id, the mode it was created for, the backing address the host was told, whether `SET_SCANOUT` was issued | Keep one host scanout resource matching its source's committed mode |
| `DisplayScanout` | system | one `ScanoutResource` per source, and the next resource id | Put each source's committed frame on the host's scanout |
| `DisplayInfoResponse` | object | the per-scanout rectangles and enabled flags it validated, and how many the device reported | Answer what the device's display info says |
| `VsyncScenario` | object | the vertical blanks it counted and the window it counted them in | Judge whether vertical blanks arrive at the committed rate |

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

- [`docs/kb/06-display.md`](../../docs/kb/06-display.md)
- [`docs/kb/11-implementation-plan.md`](../../docs/kb/11-implementation-plan.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## User Scenarios & Testing *(mandatory)*

Users: anyone using the guest - the desktop is the first thing they see - and
every presentation feature after this one. This is kb 11's M1 done-when in a
real guest: the Windows desktop runs at a chosen mode through `rvkgpu`,
composited by DWM on WARP. Frames still reach the host through the 2D path
(`RESOURCE_CREATE_2D`, `ATTACH_BACKING`, `SET_SCANOUT`, `RESOURCE_FLUSH`).

### User Story 1 - The desktop runs through rvkgpu (Priority: P1)

**Independent Test**: an L5 runner scenario with `-vga none` and only
`virtio-gpu-rvk-pci` as display takes QMP `screendump`s into the work
directory before and after the guest opens a window, and compares them.

**Acceptance Scenarios**:

1. **Given** the installed driver and no other display adapter, **When** the
   guest boots, **Then** it reaches the desktop, the display adapter is
   `rvkgpu` with status OK, and `dwm.exe` runs.
2. **Given** the desktop, **When** a window opens and the cursor moves,
   **Then** the host's screendump changes accordingly, and the cursor moves on
   the host through the cursor queue.
3. **Given** the desktop, **Then** Basic Display is not bound to the device and
   no display-related TDR is logged.

### User Story 2 - Modes (Priority: P1)

**Independent Test**: a guest-side script over the agent lists modes and
switches between them; screendump dimensions follow.

**Acceptance Scenarios**:

1. **Given** the device's EDID and display info, **When** the guest lists modes,
   **Then** every EDID mode is offered and none that is not.
2. **Given** each offered mode in turn, **When** the guest selects it, **Then**
   the host scanout has that size and the choice survives a reboot.
3. **Given** a mode the device does not offer, **When** it is requested,
   **Then** it is refused and the current mode stays.
4. **Given** the host changes the display size (a display configuration change
   from QEMU), **Then** the guest re-enumerates modes and follows it.

### User Story 3 - Frame pacing (Priority: P2)

**Independent Test**: an `rvktest.exe` scenario counts
`D3DKMTWaitForVerticalBlankEvent` returns over 60 s.

**Acceptance Scenarios**:

1. **Given** a 60 Hz mode, **Then** vertical blank events arrive at 60 Hz within
   5%, and stop when `dxgkrnl` disables vsync interrupts.

### User Story 4 - Display power and several outputs (Priority: P2)

**Independent Test**: L5 scenarios with `max_outputs=2`.

**Acceptance Scenarios**:

1. **Given** two outputs, **Then** the guest extends the desktop onto both, each
   with its own scanout and mode.
2. **Given** the display is turned off by power policy and back on, **Then**
   scanout resumes with the same mode.
3. **Given** the host disables one output, **Then** the guest drops it without
   a TDR.

### Edge Cases

- A hostile or broken host: EDID with a bad checksum, zero or enormous modes,
  more scanouts than the protocol allows - validated; the driver falls back to
  a safe mode list and logs the value.
- A mode larger than the device's memory for a 2D resource: refused.
- QEMU with `-display none`: scanout still works and screendump reads it.
- Rotation, scaling and gamma requests: refused as reported, not ignored.
- A mode change while a fullscreen application runs.
- DWM idle for minutes: no flush storm, no timeout.

## Requirements *(mandatory)*

- **FR-001**: With `rvkgpu` as the only adapter, the desktop composites through
  it on WARP; Basic Display never binds the device.
- **FR-002**: Modes come from the EDID and display info; cofunctional modality
  enumeration never returns an empty set for a connected output.
- **FR-003**: Vertical blank is synthesized at the committed mode's refresh
  rate whenever `dxgkrnl` enables it.
- **FR-004**: Every presented frame reaches the host scanout.
- **FR-005**: All display data from the device is validated (constitution V).
- **FR-006**: Rotation, scaling, gamma and multi-plane overlay are reported
  unsupported until implemented (constitution VI).
- **FR-007**: The in-guest runner gains screendump capture and comparison, and
  a mode-switch scenario; each mismatch found is reproduced at L2 first.

## Success Criteria *(mandatory)*

- **SC-001**: Five consecutive boots reach the desktop through `rvkgpu`.
- **SC-002**: Every EDID mode, selected in turn, is the host screendump's size.
- **SC-003**: Vertical blank rate within 5% of the refresh rate over 60 s.
- **SC-004**: Two outputs extend the desktop in an L5 run.
- **SC-005**: No Verifier violation or display TDR in any of these runs.

## Assumptions

- DWM composites on WARP; guest-to-host copies per flush are acceptable (M1).
- The runner's QEMU display is `none` plus optional VNC; screendump reads the
  device's scanout regardless.

## Out of Scope

- Hardware-accelerated composition and dmabuf scanout (058).
- HDR, variable refresh, multi-plane overlay, rotation.
