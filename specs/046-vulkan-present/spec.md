# Feature Specification: Vulkan: presentation through the KMD

**Feature Branch**: `046-vulkan-present`

**Created**: 2026-09-11

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 046 · **Status**: `implement` · **Depends on**: 045, 038, 041

VK_KHR_win32_surface and VK_KHR_swapchain present through rvkgpu instead of
GDI: swapchain images are allocations the host renders into, presented with
D3DKMT and composed by DWM, paced by the adapter's vertical blank; FIFO,
mailbox and immediate modes, resize, occlusion and fullscreen behave as on a
hardware driver.

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
| `WindowSurface` (moved to `src/icd/common`) | interface | - | The windows an ICD draws on, as the Windows shim sees them |
| `SurfaceTable` | system | the process's `VkSurfaceKHR` records (window, instance) | Owns the surfaces `vkCreateWin32SurfaceKHR` made |
| `SurfaceProperties` | object | the window, the surface query answers | Answers a surface's capabilities, formats and present modes from its window |
| `SwapchainImages` | object | the host images, their memory, the read-back buffer | Makes and releases one swapchain's host objects |
| `Swapchain` | object | extent, format, mode, images, next index, the copy's command objects | Hands out images and puts a presented one on its window |
| `SwapchainTable` | system | the process's swapchains | Owns the swapchains `vkCreateSwapchainKHR` made |
| `FramePacer` | object | the adapter, the device, the vertical blank count | Holds a FIFO present until the adapter's next vertical blank |
| `PresentProbe` (guest suite) | object | the window, the swapchain, the frame count | Proves a presented frame reaches the window at the display's rate |
| `PresentBlit` (core) | object | the source and destination pages, the rectangle | Copies one present's pixels between CPU-backed allocations |

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
- [`docs/kb/07-usermode.md`](../../docs/kb/07-usermode.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-17 (answered from the repository and the RE workspace)

- **Why is the guest desktop black, and does that block this feature?** The
  KMD's `ExecutionNode` completes every non-rvk DMA buffer as a loopback: the
  blit, colour-fill and flip commands `DxgkDdiPresent` writes are never
  executed, so DWM's (CDD's) presents never reach the scanout. → Executing
  present commands is part of this feature (FR-004), as `PresentBlit` in the
  KMD at `DxgkDdiPresent`, since every source DWM presents from on this
  adapter is CPU-backed.
- **Can a windowed Vulkan present go through D3DKMT and be composed by DWM
  now?** No. A windowed present composed by DWM needs a redirection surface,
  which a D3D UMD makes (feature 055), and dxgkrnl's legacy present is allowed
  only to the VidPn source's owner (`DXGDEVICE::AllowLegacyPresent`,
  dxgkrnl.sys @14036fb0c). → Until 055, a swapchain's frame reaches its window
  through the `WindowSurface` seam (GDI `SetDIBitsToDevice` on the window's
  DC), which DWM composes like any GDI window; FR-001's "never GDI" is
  deferred to 055, recorded here and in the plan.
- **Where can a run see a presented frame?** The runner's programs run in
  session 0, whose windows are not on the console's scanout. → The probe reads
  its window back (`GetDIBits` of the window's DC) and saves the frame as a
  bitmap; the runner's screendump proves the desktop path of FR-004.
- **Present modes.** FIFO (paced on `D3DKMTWaitForVerticalBlankEvent`) and
  IMMEDIATE are reported; MAILBOX is not, since a synchronous present cannot
  replace a queued frame.
- **User Story 3 (vkd3d-proton)** needs a runtime this repository does not
  pin; it is deferred to feature 047, which brings the D3D translation layers.

## User Scenarios & Testing *(mandatory)*

<!-- Written by /speckit-specify or by hand. The autogen block above is regenerated by tools/gen_specs.py; everything below is preserved. -->

Users: every Vulkan application in the guest that draws to a window -
including every D3D application running through DXVK or vkd3d-proton - which
needs frames on the desktop at the display's rate, composed by DWM like any
other window, without the CPU read-back and GDI blit render-server's ICD uses
today.

### User Story 1 - A swapchain presents through rvkgpu (Priority: P1)

**Independent Test**: vkcube in a window in the guest renders at the display's
refresh rate; the in-guest runner captures the desktop through the device's
scanout and finds the cube's frames; the ICD's log shows no `StretchDIBits`
call, and the KMD's log shows a present per frame.

**Acceptance Scenarios**:

1. **Given** a `VK_KHR_win32_surface` for an HWND, **When** a swapchain is
   created, **Then** its images are rvkgpu allocations the host GPU renders
   into, and presenting one hands it to the KMD through D3DKMT so DWM composes
   it (kb 07, option 2).
2. **Given** FIFO present mode, **When** frames are presented faster than the
   display, **Then** presentation is paced by the adapter's vertical blank
   (`D3DKMTWaitForVerticalBlankEvent` or the flip completion), with no tearing
   and no dropped acknowledged frame.
3. **Given** MAILBOX or IMMEDIATE, **Then** they are reported only if the path
   implements their semantics, and behave accordingly.
4. **Given** surface capabilities are queried, **Then** extents, formats,
   color spaces and image counts describe what the path supports - sRGB and
   UNORM 8-bit BGRA/RGBA at least - and nothing it does not.

### User Story 2 - Windows that change (Priority: P1)

**Independent Test**: a scripted application resizes, minimizes, restores,
moves across outputs and goes fullscreen while presenting; every transition
returns the result the Vulkan specification requires and frames resume.

**Acceptance Scenarios**:

1. **Given** a window resized, **When** the next present happens, **Then** it
   returns `VK_SUBOPTIMAL_KHR` or `VK_ERROR_OUT_OF_DATE_KHR` and a recreated
   swapchain at the new extent presents correctly.
2. **Given** a minimized window, **Then** `currentExtent` is 0x0 and the
   application's swapchain creation fails as the specification says, not with
   a crash or a host-side error.
3. **Given** exclusive fullscreen (`VK_EXT_full_screen_exclusive` if reported,
   or a borderless window on one output), **Then** the image reaches scanout
   on that output at its mode.
4. **Given** an occluded window, **Then** presents succeed and consume no host
   GPU time beyond the application's rendering.

### User Story 3 - A D3D12 sample in a window (Priority: P2)

**Independent Test**: the kb 11 M5 done-when - a vkd3d-proton D3D12 sample,
with vkd3d-proton placed beside it, renders in a window on the guest desktop,
checked by desktop capture against a reference frame.

### Edge Cases

- Device lost during present: `VK_ERROR_DEVICE_LOST`, the window keeps its
  last composed frame, no KMD bugcheck.
- Window destroyed with images in flight: resources released after the
  pending present completes or is cancelled; no leak in the guest or the
  device.
- Several outputs with different refresh rates: pacing follows the output the
  window is on; moving the window changes it.
- A 32-bit WoW64 application presenting: same behaviour through the 32-bit
  ICD.
- Adapter removal while presenting: presents fail as out-of-date or lost; DWM
  recovers on the remaining adapter.
- Memory budget exhausted when a swapchain is recreated larger: creation fails
  with `VK_ERROR_OUT_OF_DEVICE_MEMORY` and the old swapchain remains usable.
- Host that cannot import the image memory: the capset bit from feature 039
  selects the copy fallback; behaviour is identical, only slower.

## Requirements *(mandatory)*

- **FR-001**: Presentation never reads a frame back through the CPU into GDI;
  it goes through rvkgpu allocations and D3DKMT present.
- **FR-002**: Surface capabilities, formats, color spaces and present modes
  report only what is implemented and tested (constitution VI).
- **FR-003**: Pacing follows the adapter's vertical blank for the output that
  shows the window.
- **FR-004**: The KMD's present path (features 012, 038) accepts the swapchain
  allocations and rejects malformed present requests with a status, never an
  assert.
- **FR-005**: Present-related guest logic is testable on Linux against the
  loopback rig and the fake dxgkrnl's present and VidPn models.
- **FR-006**: The in-guest runner can capture the scanned-out desktop for
  frame comparison.

## Success Criteria *(mandatory)*

- **SC-001**: vkcube presents at the display's refresh rate within 2% over a
  minute, measured by present timestamps.
- **SC-002**: The resize/minimize/fullscreen script passes with no validation
  error from the Khronos validation layer.
- **SC-003**: The vkd3d-proton sample's captured frame matches its reference.

## Assumptions

- DWM composites on WARP at this stage; host-side composition is feature 058.
- The display path of feature 038 is live.

## Out of Scope

- HDR, wide color spaces and 10-bit swapchains: not reported until a later
  feature implements them.
- Zero-copy host composition and dmabuf scanout (058).
- OpenGL presentation (051), which reuses this path.
