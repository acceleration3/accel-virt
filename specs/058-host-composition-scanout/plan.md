# Implementation plan: the desktop on the host GPU (058)

**Branch**: `058-host-composition-scanout` · **Spec**: spec.md · **Research**: research.md

## Summary

Give the QEMU device a scanout of its own. Today it has none: the base
virtio-gpu class copies the guest's primary into a pixman image once per frame
(`TRANSFER_TO_HOST_2D`) and `SET_SCANOUT_BLOB` is one line answering
`ERR_INVALID_PARAMETER`. This feature replaces that line with a display object
model in `src/device/display/`: a head shows a blob, the blob's pages become
one dma-buf, and QEMU's display imports it. A new capset bit says whether the
host's display can take one, and without it the guest keeps the 2D path of
feature 038 unchanged.

That removes the copy SC-001 counts. It does not by itself put the desktop *on*
the host GPU: three rungs stand between here and that (research §3), and the
two above this one need feature 041 to make a host-visible allocation the guest
can actually use, and an executor operation that does not exist yet. Both are
specified here - `contracts/executor_scanout_export.md` is written so that
building it later is transcription - and neither is built, because nothing can
exercise either until 056 makes DWM produce a host image and 041 lets the guest
name it.

## Technical context

C++23 freestanding (constitution); `src/device/display/` for the new object
model, `src/device/control/` for the command class beside `BlobCommands`,
`src/proto/virtio_gpu/` for the two wire records, `src/device/abi/` and
`src/device/qemu/` for the four host operations. Tests over `MockHost`
(`tests/harness/host/`), `MockHostOps`, the device module rig
(`tests/unit/device/module/`), a hostile table and a fuzz target for each
record, then the real patched QEMU through `tests/qemu/` qtest. The live proof
is the guest, and it needs one flag the runner does not pass today.

## What this feature does not build, and why

**A guest-side flip path, a new DDI, or a flip queue.** Research §2: DWM asks
for `DxgkDdiPresent`, `SetVidPnSourceAddress` and `CRTC_VSYNC`, all three of
which this driver already implements; the flip-queue slots of feature 022 are
NULL by that feature's own measured decision and give 058 nothing. FR-002 keeps
multi-plane overlay out. So the guest-side half of this feature is a capset
check and a change of *what the primary is*, and what the primary is belongs to
041 (a host-visible allocation) and 056 (a DWM that renders into it).

**A device-driven vertical blank (FR-005).** Research §7: the guest recognises
one device interrupt today and it is not a display interrupt, so this would be
a new interrupt kind on both sides with no way to prove either half without the
guest. `tasks.md` carries it as T031-T033 with the bit it needs.

**The executor's image export (rung 3).** The ABI
(`src/device/abi/rvk_executor.h`, version 5) has no operation that hands a host
object out; adding version 6 for a caller that cannot exist until 056 merges is
exactly the speculative structure Principle VIII forbids. The contract is
written; the code is not.

**Host-visible blobs as a scanout (rung 2).** One word away on the device side -
`qemu_memfd_alloc`'s seals are `0` and udmabuf takes only `F_SEAL_SHRINK`
(research §3) - but the guest cannot map such a blob usefully while 041's
host-visible segment is parked, so a scanout the guest can never point at is a
branch with no caller. T027 records the seal and the one-line change.

## Approach, by user story

**US3, it degrades instead of failing (P1, and first).** The fallback is the
behaviour that already exists, so it is built first and it is what keeps the
gate green: `CapsetInfo::feature_dmabuf_scanout` is absent unless a display
head can show a dma-buf, `SET_SCANOUT_BLOB` answers `ERR_UNSPEC` in that case,
logged once at the point of decision (FR-006), and feature 038's display,
mode-switch and power scenarios run exactly as they do today (SC-004).

The bit is decided **when the guest first reads the capset**, not at realize,
and that is not a convenience: QEMU registers its display backends in
`qemu_init_displays()` at `system/vl.c:3864`, *after* `qmp_x_exit_preconfig`
realizes every device at 3862, so at realize the console has no GL context and
a realize-time probe would answer no on every host. `DeviceCapset::resolve_display()`
asks once, memoizes, and says so in its comment.

**US1, the desktop is a host GPU image (P1).** The device half:
`ScanoutCommands` answers `SET_SCANOUT_BLOB`, `RESOURCE_FLUSH` naming a blob,
and `SET_SCANOUT` naming a blob (refused, as `RESOURCE_CREATE_2D` over a blob
id already is). `DisplayHeads` keeps one `ScanoutBinding` per head; a binding
pins its blob, owns one `GuestDmaBuf` over the whole blob, and holds the
`ScanoutFramebuffer` that says which rectangle of it is the picture. Showing a
head is `Host::show_scanout`; a flush is `Host::update_scanout`; unbinding is
`Host::hide_scanout` and the dma-buf closed. `RESOURCE_UNREF` of a blob a head
shows is refused, because the pin is what makes the dma-buf safe.

The guest half of US1 - the primary being that blob - is 041's and 056's, and
this plan says so rather than half-building it.

**US2, fullscreen applications flip directly (P2).** Nothing device-side
distinguishes DWM's primary from a fullscreen application's: both are a blob a
head is pointed at, and swapping which blob a head shows is the same call.
What decides it is the guest, so US2's device-side acceptance is that
`DisplayHeads` reports which blob each head shows in a state dump
(`adapter`-style `report(StateWriter&)`), which is the "scanout-source trace"
US2's independent test names.

## Classes

Every one owns state; the spec's table is filled from this.

| Class | Kind | Where | State it owns | Responsibility |
|---|---|---|---|---|
| `ScanoutFormat` | object | `src/device/display/` | one virtio-gpu pixel format id | answers a format's DRM fourcc and pixel width, or that it is not one this device scans out |
| `ScanoutFramebuffer` | object | `src/device/display/` | offset, stride, width, height, source rectangle, `ScanoutFormat` | holds one framebuffer's geometry inside a blob, refusing one that does not fit |
| `ScanoutBinding` | object | `src/device/display/` | head id, blob id, `ScanoutFramebuffer`, `GuestDmaBuf`, shown flag | owns what one display head shows |
| `DisplayHeads` | system | `src/device/display/` | `ScanoutBinding[max_heads]`, `Host&`, `BlobTable&`, the resolved display answer | keeps the host's display heads showing the blobs the guest bound to them |
| `ScanoutCommands` | object | `src/device/control/` | `Host&`, `BlobTable&`, `DisplayHeads&` | answers the control requests that choose what a display head shows |
| `GpuRect` | object | `src/proto/virtio_gpu/` | x, y, width, height from the wire | holds a virtio-gpu rectangle and says whether it lies inside given bounds |
| `SetScanoutBlobRequest` | object | `src/proto/virtio_gpu/` | head, resource, size, format, four strides, four offsets, `GpuRect` | decodes a `SET_SCANOUT_BLOB` request |
| `FlushRequest` | object | `src/proto/virtio_gpu/` | resource, `GpuRect` | decodes a `RESOURCE_FLUSH` request |

Extended, not added: `Host` and `RvkHostOps` gain four display operations
(`contracts/display_host_ops.md`); `CapsetInfo` gains one feature constant;
`DeviceCapset` gains `resolve_display()`; `VirtioGpuRvk` gains a
`DisplayHeads` member and one more line in `control()`; `MockHost` and
`MockHostOps` gain the four operations and the adversarial knobs for them.

## Constitution check

- **I**: the spec is written; this plan records where the spec's own
  requirements outrun what can be proven (FR-001 rungs 2 and 3, FR-005) rather
  than implementing them blind.
- **II**: every decision - is this framebuffer inside this blob, which fourcc
  is this format, which head shows what - is in `src/device`. The glue gains
  four functions, each a straight-line translation onto a QEMU display call
  with no branch of its own beyond the null checks the ABI forces, and the
  virtio-gpu-format-to-DRM-fourcc table is in core precisely so the glue does
  not have to choose.
- **III**: eight new classes, all stateful, all in the table above. No static
  member function, no new process-lifetime state.
- **IV**: tests first. Every one of the sixteen refusals in
  `contracts/scanout_blob.md` is named in a test; `fuzz_scanout_request`
  covers both records with a committed corpus; `DisplayHeads`,
  `ScanoutBinding` and `ScanoutFramebuffer` each answer `invariants_hold()`
  and every test calls it after every mutation; the allocation-failure sweep
  runs through `MockHost`'s failure knobs.
- **V**: `SET_SCANOUT_BLOB` is 96 bytes of guest-authored input naming an
  offset, a stride and a rectangle into memory the host will hand a GPU. Every
  term is 64-bit and overflow-checked before it is used, and the blob's own
  size - which the device owns - is the only bound trusted.
- **VI**: the capset bit is set only where the display proved it can show a
  dma-buf, and it is withdrawn nowhere else. No multi-plane overlay, no HDR,
  no variable refresh rate is reported.
- **VIII**: no executor ABI version 6, no second scanout backend, no vblank
  source with no consumer.
- **IX**: every decision logs once - the display's answer, the first refusal of
  a kind, each change of what a head shows.

## Complexity tracking

| Deviation | Why | Rejected alternative |
|---|---|---|
| The capset bit is decided at first read, not at realize | QEMU registers displays after device realize (`system/vl.c:3862-3864`) | a realize-time probe, which answers no on every host |
| FR-001's host GPU image is not delivered here | the executor ABI is import-only and 041's segment is parked | building an export nothing can call |
| FR-005's device-driven vblank is deferred | no display interrupt exists on either side; unprovable without the guest | a vsync source with one implementation and no consumer |
| One dma-buf per head, not per blob | a head owns its lifetime; two heads on one blob stay independent | shared ownership and a refcount for a case that does not exist |
| A scanned-out blob cannot be unreferenced | the pin is what keeps the dma-buf's pages alive; `BlobTable` already refuses a pinned release | tearing the scanout down inside `RESOURCE_UNREF`, which hides a guest bug |
