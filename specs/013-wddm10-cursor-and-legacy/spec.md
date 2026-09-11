# Feature Specification: WDDM 1.0: cursor, and declining the legacy surface

**Feature Branch**: `013-wddm10-cursor-and-legacy`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 013 · **Status**: `partial` · **Depends on**: 012
· **WDDM level**: WDDM 1.0 (Windows Vista)

Hardware cursor is real and maps onto the virtio-gpu cursor queue. Palette,
swizzling ranges, overlays and capture are legacy: decline them correctly and
prove the decline.

### Status meaning

Implement only the subset listed below. Everything else in this level is stubbed or skipped, and the stub is tested.

### DDI slots in scope (10)

| Slot | Field | Introduced |
|--:|---|---|
| 23 | `DxgkDdiAcquireSwizzlingRange` | BASE |
| 24 | `DxgkDdiReleaseSwizzlingRange` | BASE |
| 29 | `DxgkDdiSetPalette` | BASE |
| 30 | `DxgkDdiSetPointerPosition` | BASE |
| 31 | `DxgkDdiSetPointerShape` | BASE |
| 47 | `DxgkDdiStopCapture` | BASE |
| 49 | `DxgkDdiCreateOverlay` | BASE |
| 55 | `DxgkDdiUpdateOverlay` | BASE |
| 56 | `DxgkDdiFlipOverlay` | BASE |
| 57 | `DxgkDdiDestroyOverlay` | BASE |

Slot numbers are indices into `DRIVER_INITIALIZATION_DATA` as laid out by the WDDM 3.2 headers; see `docs/ref/driver_init_table.md`. They are the fastest way to navigate an unsymbolized vendor driver.

### Note

Only the two pointer DDIs are implement; the rest are stub.

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
| `GpuRequestWriter` (proto) | object | the destination bytes and capacity | Encodes virtio-gpu 2D and cursor requests |
| `GpuCommandQueue` | object | platform, transport, the virtqueue, a spin lock, request/response slots, which slot each chain holds | Sends virtio-gpu requests on one queue and reclaims their slots |
| `VirtioGpuDevice` (extended) | object | + control and cursor `GpuCommandQueue`s instead of bare queues | The virtio-gpu function while powered |
| `CursorImage` | object | platform, one `DXGKARG_SETPOINTERSHAPE` | Draws one pointer shape as 64x64 B8G8R8A8 |
| `HardwareCursor` | object | platform, the device storage, the cursor resource's backing, whether the host has the resource, hot spot, position, visibility | Shows dxgkrnl's pointer through the virtio-gpu cursor queue |
| `DriverCapabilities` (extended) | object | + pointer capabilities | Reports what the driver implements |
| DDI records | DDI record | `DXGKARG_SETPOINTERSHAPE`, `DXGKARG_SETPOINTERPOSITION` | Overlays memory dxgkrnl owns, layout-proven |

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

- Q (agent, RE): How does dxgkrnl decline the legacy DDIs itself? -> A:
  `ADAPTER_RENDER` installs `DefaultDdiAcquireSwizzlingRange`,
  `DefaultDdiReleaseSwizzlingRange`, `DefaultDdiStopCapture`,
  `DefaultDdiCreateOverlay`, `DefaultDdiUpdateOverlay`, `DefaultDdiFlipOverlay`
  and `DefaultDdiDestroyOverlay` for a null slot; each logs and answers
  `STATUS_NOT_SUPPORTED`. The stubs answer the same, so registering them
  changes nothing dxgkrnl can observe: `NumberOfSwizzlingRanges` and
  `MaxOverlays` stay 0 and no palettized mode is offered. SetPalette answers
  `STATUS_NOT_SUPPORTED` too. The overlay DDIs receive an overlay handle this
  driver never creates, so they touch nothing.
- Q (agent): What does the hardware cursor need from the device? -> A: The
  virtio-gpu 2D path of virtio 1.2 §5.7: `RESOURCE_CREATE_2D` (64x64
  `B8G8R8A8_UNORM`, resource id 1, reserved for the cursor) and
  `RESOURCE_ATTACH_BACKING` (one contiguous 16384-byte block) once per power-up,
  then per shape `TRANSFER_TO_HOST_2D` on the control queue and `UPDATE_CURSOR`
  on the cursor queue; `MOVE_CURSOR` per position; `UPDATE_CURSOR` with resource
  0 hides it. Requests are not waited on: responses are reclaimed in the DPC and
  a refusal is logged. The control queue has request and response slots, the
  cursor queue request slots only (QEMU writes nothing back).
- Q (agent): How are shapes converted? -> A: Top-down rows. Colour: the ARGB
  pixels as they are. Masked colour: mask byte 0 is the opaque colour, 0xFF over
  black is transparent, 0xFF over a colour (an XOR) is the opaque colour.
  Monochrome: AND 1/XOR 0 transparent, AND 0/XOR 0 opaque black, AND 0/XOR 1
  opaque white, AND 1/XOR 1 (invert) opaque black. Anything beyond the shape is
  transparent. A shape wider or taller than 64, empty, with a hot spot outside
  it, a pitch narrower than a row, or not exactly one type is refused.
- Q (agent): Capabilities? -> A: `PointerCaps` Monochrome | Color | MaskedColor,
  `MaxPointerWidth` and `MaxPointerHeight` 64 - implemented and tested here.

## User Scenarios & Testing *(mandatory)*

Users: dxgkrnl's pointer path (win32k's cursor through DxgkDdiSetPointerShape
and DxgkDdiSetPointerPosition); QEMU's display, which draws the virtio-gpu
cursor.

### User Story 1 - Hardware cursor (Priority: P1)

1. **Given** a started adapter, **When** SetPointerShape with a 32x32 colour
   arrow, **Then** the control queue carries `RESOURCE_CREATE_2D` and
   `RESOURCE_ATTACH_BACKING` (first time only) and `TRANSFER_TO_HOST_2D`, the
   backing holds the arrow, and the cursor queue carries `UPDATE_CURSOR` with the
   hot spot.
2. **When** SetPointerPosition visible at (100, 200), **Then** `MOVE_CURSOR`;
   invisible, **Then** `UPDATE_CURSOR` with resource 0; visible again, **Then**
   `UPDATE_CURSOR` with the resource.
3. **Given** a power cycle, **When** SetPointerShape, **Then** the resource is
   created again.
4. A shape the adapter cannot show, another source, or no device answers an
   error and sends nothing.

### User Story 2 - Declining the legacy surface (Priority: P2)

1. AcquireSwizzlingRange, ReleaseSwizzlingRange, SetPalette, StopCapture,
   CreateOverlay, UpdateOverlay, FlipOverlay and DestroyOverlay answer
   `STATUS_NOT_SUPPORTED`, dereferencing nothing, and the render core is still
   created.

## Requirements *(mandatory)*

- **FR-001**: Every field of a pointer shape and every byte of its bitmap within
  the claimed size is checked or bounded; nothing is written outside the
  64x64 backing (fuzz).
- **FR-002**: Queue slots are reclaimed only for chains the queue published; a
  full queue answers `no_space` and publishes nothing.
- **FR-003**: Only the pointer capabilities change.

## Success Criteria *(mandatory)*

- **SC-001**: Through the table, a shape, moves, hide and show reach the fake
  device's queues in order; the legacy slots decline; no leak, no violation.
- **SC-002**: 100% branch coverage; fuzz_cursor_image clean.

## Out of Scope

- Waiting for control responses, cursor on sources other than 0, palettes,
  overlays, swizzling ranges, capture.
