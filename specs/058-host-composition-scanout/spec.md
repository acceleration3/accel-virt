# Feature Specification: The desktop on the host GPU: zero-copy composition and scanout

**Feature Branch**: `058-host-composition-scanout`

**Created**: 2026-09-11

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 058 · **Status**: `implement` · **Depends on**: 056, 046, 038

DWM composites on rvkgpu instead of WARP, primary surfaces are host GPU
images, flips go through SetVidPnSourceAddress with vertical-blank completion,
and the device hands the final image to QEMU's display as a dmabuf - no frame
crosses back into guest memory.

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
| `ScanoutFormat` | object | one virtio-gpu pixel format id | answers a format's DRM fourcc and pixel width, or that it is not one this device scans out |
| `ScanoutFramebuffer` | object | offset, stride, width, height, source rectangle, `ScanoutFormat`, the blob size it was checked against | holds one framebuffer's geometry inside a blob, refusing one that does not fit |
| `ScanoutBinding` | object | head index, blob id, `ScanoutFramebuffer`, `GuestDmaBuf`, shown flag | owns what one display head shows |
| `DisplayHeads` | system | `ScanoutBinding[16]`, `Host&`, `BlobTable&`, the display's resolved answer | keeps the host's display heads showing the blobs the guest bound to them |
| `ScanoutCommands` | object | `Host&`, `BlobTable&`, `DisplayHeads&` | answers the control requests that choose what a display head shows |
| `GpuRect` | object | x, y, width, height from the wire | says whether a virtio-gpu rectangle lies inside given bounds |
| `SetScanoutBlobRequest` | object | head, resource, size, format, four strides, four offsets, `GpuRect` | decodes a `SET_SCANOUT_BLOB` request |
| `FlushRequest` | object | resource, `GpuRect` | decodes a `RESOURCE_FLUSH` request |

Extended rather than added: `Host` and `RvkHostOps` gain four display
operations (`contracts/display_host_ops.md`), `CapsetInfo` gains
`feature_dmabuf_scanout`, `DeviceCapset` gains `resolve_display()`, and
`VirtioGpuRvk` gains a `DisplayHeads`.

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
- [`docs/kb/11-implementation-plan.md`](../../docs/kb/11-implementation-plan.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## User Scenarios & Testing *(mandatory)*

<!-- Written by /speckit-specify or by hand. The autogen block above is regenerated by tools/gen_specs.py; everything below is preserved. -->

Users: the person looking at the guest's desktop through QEMU's display, who
wants it smooth and at the host GPU's speed; and every presenting application,
whose frames should reach the screen without being copied back into guest
memory. This is milestone M8's done-when (`docs/kb/11-implementation-plan.md`):
DWM no longer falls back to WARP, and desktop frames reach QEMU's display
without a guest copy - presentation option 3 of `docs/kb/07-usermode.md`.

### User Story 1 - The desktop is a host GPU image (Priority: P1)

**Independent Test**: with QEMU started on a GL-capable display
(`-display egl-headless` in automated runs), the in-guest runner shows DWM
composing on rvkgpu, the device's counters show zero bytes of frame data
copied from guest memory for a minute of desktop animation, and host-side
captures of the dmabuf match a reference.

**Acceptance Scenarios**:

1. **Given** DWM on rvkgpu (feature 056), **When** it creates its primary
   swapchain, **Then** the primary surfaces are host GPU images backing
   guest allocations, not guest-backed memory.
2. **Given** a composed frame, **When** DWM presents, **Then** the KMD flips
   through `DxgkDdiSetVidPnSourceAddress` and the device hands the image to
   QEMU's display with `dpy_gl_scanout_dmabuf`.
3. **Given** the flip is shown, **Then** the KMD reports completion with a
   `CRTC_VSYNC` interrupt carrying the flipped-to address, and DWM's frame
   pacing follows it.

### User Story 2 - Fullscreen applications flip directly (Priority: P2)

**Independent Test**: a fullscreen D3D12, D3D11 and Vulkan application each
present via flip; the device shows their images as the scanout without DWM's
composition copy, verified by the device's scanout-source trace.

**Acceptance Scenarios**:

1. **Given** an exclusive or independent-flip fullscreen swapchain, **When** it
   presents, **Then** its image becomes the scanout source directly.
2. **Given** the application leaves fullscreen, **Then** scanout returns to
   DWM's surface without a blank frame.

### User Story 3 - It degrades instead of failing (Priority: P1)

**Independent Test**: QEMU started with a display that has no GL
(`-display vnc` alone) keeps presenting through the copy path of feature 046;
the capset reports the missing dmabuf scanout and the guest never selects it.

**Acceptance Scenarios**:

1. **Given** no GL-capable display, **Then** host images are resolved into the
   2D path and the desktop still works.
2. **Given** a host GPU whose images cannot be exported as dmabuf, **Then** the
   same fallback applies and is logged once.

### Edge Cases

- Several monitors: each VidPn source has its own scanout, flips and vsync.
- A mode change while a flip is pending: the pending flip completes or is
  cancelled with the status dxgkrnl expects before the new mode's first flip.
- TDR or a host fault during composition: the desktop recovers on rvkgpu, not
  on WARP, after DWM recreates its device.
- The QEMU display window is closed or resized on the host: the guest's modes
  do not change unless the EDID does.
- Hardware cursor over a dmabuf scanout.
- Protected or DRM content: declined, surfaces refused rather than displayed.
- The guest writes a primary surface with the CPU (lock): content remains
  correct, at the cost of a map of host memory.

## Requirements *(mandatory)*

- **FR-001**: Primary and flip-model swapchain surfaces are host GPU images
  exposed as guest allocations; DWM's composition never copies through guest
  memory.
- **FR-002**: Flips use `DxgkDdiSetVidPnSourceAddress` with `CRTC_VSYNC`
  completion; multi-plane overlay is not reported.
- **FR-003**: The device exports each scanout image as a dmabuf and calls
  `dpy_gl_scanout_dmabuf`; the feature requires a GL-capable QEMU display
  (gtk `gl=on`, `egl-headless`, dbus `gl=on` or SPICE with GL).
- **FR-004**: Dmabuf scanout is a capset bit; without it the guest uses the
  copy path of feature 046, tested on its own.
- **FR-005**: Vertical-blank timing comes from the device, not a guest timer,
  when dmabuf scanout is active.
- **FR-006**: Scanout-source changes, flip completions and fallbacks are logged
  once at the point of decision on both sides.
- **FR-007**: Device-side scanout logic is in `src/device` to the
  constitution's standard; the QEMU glue only translates.

## Success Criteria *(mandatory)*

- **SC-001**: One minute of desktop animation copies zero frame bytes from
  guest memory.
- **SC-002**: DWM composes on rvkgpu after install, TDR and DWM restart.
- **SC-003**: Desktop frame pacing at the mode's refresh rate has no more
  dropped vsyncs than the same host GPU presenting a native window, within a
  recorded threshold.
- **SC-004**: The no-GL fallback passes the desktop scenarios of feature 038.

## Assumptions

- The host runs a QEMU display that can import dmabufs; automated runs use
  `egl-headless`.
- Features 046 and 056 are complete.

## Out of Scope

- Multi-plane overlay (`DxgkDdiCheckMultiPlaneOverlaySupport3` family).
- HDR and wide color gamut scanout.
- Variable refresh rate.
- Remote display protocols beyond what QEMU's display offers.
