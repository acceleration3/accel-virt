# Research: the desktop on the host GPU (058)

Every claim below is either read out of a file in this tree, read out of the
pinned QEMU source (`third_party/qemu/qemu-11.1.1`, `tools/fetch_qemu.sh`), or
measured by an earlier feature and cited to where it was written down. Where
nothing has been measured yet, the question is listed in §9 with the one line
that would settle it.

## 1. What is true today, before this feature

**The desktop does not reach `rvkgpu` at all.** Feature 046 measured it on the
live guest (`docs/kb/07-usermode.md`, "What the desktop actually presents
from"): in a whole boot `DxgkDdiPresent` is called **twice**, both with
`NumSrcAllocations = NumDstAllocations = 0`; `DxgkDdiSetVidPnSourceAddress`
only ever names `0xC0000000`, the shared primary; a QMP `pmemsave` of the
primary's 4 096 000 bytes is **all zeros**; the display class key has no
`UserModeDriverName`. DWM composes on WARP and its output never becomes bytes
in an allocation this miniport has pages for.

So the thing 058 is asked to make zero-copy is not currently a copy. It is
nothing. The path is real but it carries no frame, and it will not carry one
until a D3D UMD exists (055-057). That ordering is already recorded in three
places - `specs/055-umd-foundation/spec.md:194-195` ("presents through the KMD
present path of feature 046 **until feature 058 replaces it with flips of host
images**"), `specs/056-d3d11-umd/spec.md` Out of Scope ("Zero-copy composition
and dmabuf scanout: feature 058"), and `docs/kb/11-implementation-plan.md:418`
(M8) - and this feature does not try to move it.

**What the scanout does today, and the copy in it.** `DxgkDdiSetVidPnSourceAddress`
(`Adapter::set_vidpn_source_address`, `src/core/adapter/adapter.cpp:1162`)
writes nothing to the device: it stores the address and the allocation handle
in `DisplaySources` (`src/core/display/display_sources.hpp`) at `Irql::isr`.
The wire traffic happens later, from the adapter DPC
(`adapter.cpp:406`, `DisplayScanout::refresh`), and it is stock
virtio-gpu 2D:

```
RESOURCE_CREATE_2D        once per committed mode        (ScanoutResource::configure)
RESOURCE_ATTACH_BACKING   when the primary's address moved
SET_SCANOUT               when the primary's address moved
TRANSFER_TO_HOST_2D       every DPC on which the fence advanced   <- the copy
RESOURCE_FLUSH            with it
```

`TRANSFER_TO_HOST_2D` is a full-frame `memcpy` inside QEMU, from the guest's
pages into a pixman image, performed by QEMU's own
`hw/display/virtio-gpu.c`. Nothing in `src/device` participates: the rvk device
subclasses `VirtIOGPU` and `rvk_device_control` declines every 2D command
(`src/device/qemu/virtio-gpu-rvk.c:126-140`). **That per-frame copy out of
guest memory is the one SC-001 measures, and removing it is what this feature
is for.**

**`SET_SCANOUT_BLOB` is the hook, and it is a refusal today.**
`src/device/control/blob_commands.cpp:29`:

```cpp
case set_scanout_blob: response.set(GpuResponse::err_invalid_parameter); break;
```

with the header (`blob_commands.hpp:2-3`) saying why in as many words: *"blob
scanout (refused until presentation)"*. `docs/protocol.md:694` records the
refusal as the contract. The base class cannot take the command back, either:
the rvk device refuses the base class's `blob` property at realize
(`docs/protocol.md`, "Blob resources"), so `virtio_gpu_set_scanout_blob` in
QEMU is unreachable and this one line is the whole of blob scanout.

**Flip queue logs (022) provide nothing.** All four slots -
`DxgkDdiSetFlipQueueLogBuffer` (154), `UpdateFlipQueueLog` (155),
`CancelQueuedFlips` (156), `SetInterruptTargetPresentId` (157) - are absent
from `Miniport::Miniport`'s wiring table (`src/core/miniport/miniport.cpp`), so
`DdiTable::routine()` answers `nullptr` for each, and
`tests/unit/scenarios/test_ddi_scenarios.cpp` asserts it. 022's own
clarification records the reason: dxgkrnl walks the flip-queue path only when
the driver asked for `DXGK_FEATURE_HWFLIPQUEUE` through
`DxgkCbQueryFeatureSupport` and reported `MaxHwQueuedFlips > 1`, which this
driver does neither of. **There is no flip-queue log buffer for 058 to read.**
Whatever this feature needs to know about a flip it must learn from
`SetVidPnSourceAddress` itself. The whole multi-plane-overlay family (86, 95,
98, 104, 105, 106, 127) is NULL for the same reason, and FR-002 keeps it that
way.

**Vertical blank is a guest timer.** `SyntheticVsync`
(`src/core/display/synthetic_vsync.cpp`) runs a `PeriodicTimer` at
60 Hz and calls `InterruptNotification::report_vsync(0, scanout_address)`, which
builds a `DXGK_INTERRUPT_CRTC_VSYNC` record and delivers it through
`DxgkCbNotifyInterrupt`. The device's own interrupt path
(`Adapter::interrupt_routine`, `adapter.cpp:366`) recognises exactly one thing
today - the configuration-changed interrupt - and knows nothing about display
timing.

## 2. What DWM actually requires of a WDDM driver to composite

**Decision.** DWM needs a D3D user-mode driver and nothing else from the
display half: `DxgkDdiPresent` with `Flags.Flip`, `DxgkDdiSetVidPnSourceAddress`
and a `CRTC_VSYNC` interrupt are all already implemented and are all DWM asks
for. 058 adds no DDI slot on the guest side.

**Rationale.** Three separate pieces of evidence agree:

- `docs/kb/07-usermode.md:38-41`: *"The desktop compositor creates a real D3D
  device on the real adapter. If your adapter has no working UMD, DWM falls
  back to WARP."* The measured absence of `UserModeDriverName` in the display
  class key is the same fact from the registry's side.
- `docs/kb/06-display.md`, "Presenting": the three mechanisms in increasing
  order of preference are `DxgkDdiPresent`, `SetVidPnSourceAddress`, and
  multi-plane overlay, and *"Skip [MPO] initially; it is a large surface and
  DWM works fine without it."*
- `basicrender.sys`, Microsoft's own full miniport, implements
  `DxgkDdiPresent` as `WarpKMPresent` → `WARPKMCONTEXT::Present` building a
  `WARPGPUCMDPRESENT` and queueing it like any other work, with no flip queue
  and no overlay.

`DxgkDdiPresentDisplayOnly` is **not** an option and never was: it belongs to
`DxgkInitializeDisplayOnlyDriver` + `KMDDOD_INITIALIZATION_DATA`, a different
entry point with a different init structure, and `docs/kb/06-display.md`
closes with *"you cannot grow a KMDOD into a render driver."* This driver is a
full miniport at `DXGKDDI_INTERFACE_VERSION_WDDM3_2`; the slot does not exist
in its table.

**What "flip model" means here.** DWM's own swapchain is a flip-model
swapchain, but that word describes what DXGI does in user mode. What reaches
the miniport is either a `DxgkDdiPresent` with `Flags.Flip` set (which this
driver already turns into a `PresentCommand` with `PresentOperation::flip` and
deliberately moves no pixels for - `Adapter::present`, `adapter.cpp:1247`) or a
`SetVidPnSourceAddress` naming the next primary. The guest-side work 058 needs
is therefore not new DDIs; it is making the allocation those two name be a
host image instead of guest pages.

**Alternatives rejected.** Asking for `DXGK_FEATURE_HWFLIPQUEUE` to get the
flip-queue log buffers: it buys hardware-queued flips this device cannot honour
(the host display has one scanout per head and no queue), and it would turn
seven NULL slots into seven implemented ones for no observable behaviour.
Reporting MPO: FR-002 forbids it and the spec's Out of Scope says why.

## 3. How a guest allocation becomes a host GPU image with no copy

There are exactly three rungs, and this project already owns the mechanism for
each. They differ in *where the desktop's pixels live*, which is the only thing
that matters.

| Rung | Where the frame lives | Who writes it | Who shows it | What it needs |
|---|---|---|---|---|
| 0 (today) | guest RAM | the guest CPU | `TRANSFER_TO_HOST_2D` into a pixman image, then a surface | nothing |
| 1 | guest RAM | the host GPU, through the imported dma-buf | a udmabuf over the same pages, handed to the display | this feature |
| 2 | host memory in the hostmem BAR | the host GPU | a dma-buf over that block | this feature + 041 |
| 3 | host GPU memory / a `VkImage` | the host GPU, natively | a dma-buf exported from that memory | this feature + 041 + an executor ABI addition + 056 |

**Rung 1 is buildable and provable today, and it is what removes the copy.**
The pieces exist: guest pages reach the host as a dma-buf through
`Host::dmabuf_over_guest` (`src/device/host/host.hpp`), implemented by
`rvk_dmabuf_over_guest` (`src/device/qemu/virtio-gpu-rvk-host.c:249-295`) over
`/dev/udmabuf` and `UDMABUF_CREATE_LIST`, and wrapped by `GuestDmaBuf`
(`src/device/memory/guest_dma_buf.hpp`). That is the same mechanism stock QEMU
uses for blob scanout (`hw/display/virtio-gpu-udmabuf.c`,
`virtio_gpu_create_udmabuf`), which is a useful confirmation that the shape is
right, and it is the mechanism docs/testing.md's last section already forced on
this project for a completely different reason (amdgpu refusing a file-backed
userptr). One dma-buf, two consumers: the host GPU importing it to render
into, and the host display importing it to show. No byte is copied on either
side.

**Rung 2 needs 041 and one seal.** `HostMemoryBlock` is backed by
`Host::create_host_memory`, which is `qemu_memfd_alloc("virtio-gpu-rvk-blob",
size, 0, &fd, ...)` (`virtio-gpu-rvk-host.c:309-324`) - **seals `0`**. udmabuf
takes only a memfd sealed against shrinking (`docs/testing.md`: *"The guest RAM
memfd must be sealed against shrinking; udmabuf takes no other kind"*), so a
one-word change (`F_SEAL_SHRINK`) makes a host-visible blob scanout-able the
same way. What rung 2 cannot do without 041 is let the guest *see* it: the
runner reports that the guest's `D3DKMTLock2` view is not the device's BAR, and
clients asking for host-visible memory take FR-007's fallback instead. So the
device half of rung 2 is buildable now and the guest half is 041's.

**Rung 3 needs something that does not exist.** The executor ABI
(`src/device/abi/rvk_executor.h`, version 5) is import-only: `import_blob`
takes an `RvkBlobHandle` *into* the host API and `imported_handle` answers the
wire handle it became. **There is no operation that hands a host object back
out.** A `VkImage` the guest created on an executor - which is what DWM's
primary would be once 056 exists - cannot be exported as a dma-buf by any call
in that table. Rung 3 therefore needs ABI version 6 with one new entry, and
this plan specifies it (`contracts/executor_scanout_export.md`) rather than
building it, because nothing can exercise it until 056 makes DWM produce such
an image.

Two details rung 3 will have to settle and rung 1 does not:

- **Tiling.** An optimally-tiled `VkImage` exported as a dma-buf is
  uninterpretable to EGL without a DRM format modifier. That means
  `VK_EXT_image_drm_format_modifier` on the host driver and
  `vkGetImageDrmFormatModifierPropertiesEXT` +
  `vkGetImageSubresourceLayout` to fill in `qemu_dmabuf_new`'s per-plane
  offsets, strides and `modifier`. Without it the scanout image must be
  `VK_IMAGE_TILING_LINEAR` and `DRM_FORMAT_MOD_LINEAR`, which every EGL
  implementation takes and every GPU renders into more slowly. Rungs 1 and 2
  are linear by construction, so this is rung 3's problem alone.
- **Synchronisation.** `qemu_dmabuf_new`'s `allow_fences` and
  `qemu_dmabuf_set_fence_fd` exist for exactly this; nothing in this project
  produces a sync fd yet.

**Alternative rejected: a host window.** Option 1 of
`docs/kb/07-usermode.md:310-313` - render and present entirely on the host -
does not composite into the guest desktop and is not what the spec asks for.

## 4. What QEMU's display can and cannot do with a host GPU texture

Read out of the pinned tree, not remembered.

**The call is two calls.** `qemu_console_gl_scanout_dmabuf(QemuConsole *con,
QemuDmaBuf *dmabuf)` (`ui/console.c:987`) sets `con->scanout.kind =
SCANOUT_DMABUF` and fans the dmabuf out to every registered listener that
implements `dpy_gl_scanout_dmabuf`; `qemu_console_gl_update(con, x, y, w, h)`
is what makes a new frame appear. `qemu_console_gl_release_dmabuf` drops the
listener's import. The `QemuDmaBuf` itself is opaque and is built by
`qemu_dmabuf_new(width, height, offsets, strides, x, y, backing_width,
backing_height, fourcc, modifier, fds, num_planes, allow_fences, y0_top)`
(`include/ui/dmabuf.h`) - so **the device supplies a DRM fourcc and a DRM
modifier**, not a virtio-gpu format id. Translating a virtio-gpu format to a
fourcc is a decision, so by Principle II it belongs in `src/device`, not in the
glue.

**Which displays take it.** Five backends implement `dpy_gl_scanout_dmabuf`:
`egl-headless` (`ui/egl-headless.c:179`), `gtk` in both its GL-area and EGL
variants (`ui/gtk.c:629,660`), `sdl2` (`ui/sdl2.c:840`), `dbus`
(`ui/dbus-listener.c:937`, `ui/dbus-console.c:138`) and SPICE
(`ui/spice-display.c:1364`). `-display none`, `-display vnc` and `-display
gtk` with `gl=off` implement none of them.

**The gate on the device side is `qemu_console_has_gl(con)`, and the *display*
sets it, not the device.** `con->gl` is assigned only by
`qemu_console_set_display_gl_ctx`, whose only callers are the display backends
themselves (`ui/egl-headless.c:241`, `ui/gtk.c:2413`, `ui/sdl2.c:974`,
`ui/dbus.c:202`, `ui/spice-display.c:1423`). Stock virtio-gpu reads exactly
this before taking the dmabuf path (`hw/display/virtio-gpu.c:691-709`:
`if (res->blob) { if (qemu_console_has_gl(scanout->con)) { ...
virtio_gpu_update_dmabuf ... } data = res->blob; }`). So the rvk device asks
the same question, once, and FR-004's capset bit is the answer.

**egl-headless reads the frame back - and `screendump` still cannot see it.**
*Measured, and this paragraph's original claim was wrong (§9's first question,
now answered).* `egl_scanout_dmabuf` imports the fd as a texture
(`egl_dmabuf_import_texture`, EGL's `EGL_EXT_image_dma_buf_import`, compiled
only with `CONFIG_GBM`) and `egl_scanout_flush` blits it and then calls
`egl_fb_read(edpy->ds, &edpy->blit_fb)` into the console's `DisplaySurface`
before `qemu_console_update`. That readback is a GPU→CPU copy **on the host,
per flush**, and it is the price of being able to assert anything in an
automated run. `gtk,gl=on` does not pay it. It also asserts
`surface_format(edpy->ds) == PIXMAN_x8r8g8b8`, which constrains what the
scanout's fourcc may be.

What the readback does *not* give is a screendump. `qmp_screendump` asks
`qemu_console_surface(con)`, and that answers the console's surface only while
`con->scanout.kind == SCANOUT_SURFACE` (`ui/console.c`); the moment
`qemu_console_gl_scanout_dmabuf` runs it is `SCANOUT_DMABUF`, and every
screendump of that console answers *"no surface"* - ours, and stock
virtio-gpu-gl's alike. `egl_fb_read` writes into `edpy->ds`, which is the
surface the console *had*, and nothing reads it back out through QMP. So an
automated run cannot assert a picture of a dma-buf scanout at all on this
QEMU; what it can assert is the device's own account of what each head shows,
which is what `tools/guest/run_guest_suite.py`'s scanout scenario does.

**What QEMU cannot do.** It cannot take a Vulkan image handle, a GL texture id
from another process, or an opaque fd: `egl_dmabuf_import_texture` builds an
`EGLImage` from a dma-buf fd and nothing else, so a
`VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT` export - which is the type
`rvk_export_gpu_memory` tries **first** today
(`virtio-gpu-rvk-host.c:513-517`) - is useless for scanout. Rung 3 must ask for
`VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT` specifically. It also cannot
show more than one plane per scanout usefully for us, cannot composite planes
(that would be MPO, declined), and has no notion of a flip queue: a scanout is
a single current buffer, replaced whole.

**The runner uses `-display none`.** `tools/guest/run_guest_suite.py:410`
passes `-vga none -display none`, and the display scenario asserts through QMP
`screendump`. With no display backend there is no `con->gl`, so **the path this
feature builds cannot be exercised by the runner as it stands.** Changing it to
`-display egl-headless` keeps `screendump` working (§4 above) and is the single
change the live proof needs; the no-GL configuration then becomes FR-004's
fallback test rather than the only configuration.

## 5. What "zero-copy" can honestly mean here, and where a copy is unavoidable

There are four distinct copies in this stack. They are not the same copy and
the spec's success criterion names only one of them.

1. **The guest-memory copy** - `TRANSFER_TO_HOST_2D`, one full frame per
   presented frame, guest pages into a pixman image inside QEMU. This is the
   copy SC-001 measures ("one minute of desktop animation copies zero frame
   bytes from guest memory") and the one this feature removes, from rung 1
   upwards. Removing it is honest and complete: with a dma-buf scanout the
   display samples the same physical pages, and the device's counter of bytes
   transferred stays at zero for the life of the desktop.

2. **The composition copy** - whether DWM's composed output *is* the scanout
   buffer or is blitted into it. At rung 1 and 2 it can be: DWM's primary is
   the allocation the KMD points the scanout at, and the host GPU renders into
   those exact pages. At rung 3 it can only be when the composed image's tiling
   and format are importable by EGL; otherwise a host-side blit into a linear
   scanout image is **unavoidable**, and it is a GPU blit inside the host GPU,
   not a bus transfer.

3. **The host readback** - `egl_fb_read` in `egl-headless`, one GPU→CPU frame
   per flush, so that a `DisplaySurface` exists for `screendump` and for any
   2D listener. **Unavoidable in the automated rig, by construction**, and
   absent from `gtk,gl=on`. It costs nothing to SC-001, which counts bytes
   copied *from guest memory*.

4. **The guest CPU's own reads and writes of the primary** - the spec's last
   edge case. A `D3DKMTLock` of a rung 2 or rung 3 primary is a map of host
   memory across the BAR, at BAR speed; of a tiled rung 3 image it is not
   possible at all without a resolve copy. **Unavoidable**, and the honest
   answer is that it is rare (GDI on the primary) and that it is slow rather
   than wrong.

Two further honest limits:

- **Rung 1 is zero-copy but it is not "on the host GPU".** The desktop's pixels
  live in guest RAM and the host GPU reaches them over the IOMMU. Nothing is
  copied, and it will be slower than rung 2 or 3. FR-001 asks for host GPU
  images, and only rungs 2 and 3 deliver that. Saying "zero-copy" of rung 1
  without saying this would be the kind of claim constitution VI exists to
  forbid.
- **Without a GL-capable display there is no zero-copy at all.** Rung 0 is the
  floor and FR-004 makes it a capset bit rather than a silent regression.

## 6. The scanout as a device object, and where the decision lives

**Decision.** The device core owns `SET_SCANOUT_BLOB` and the `RESOURCE_FLUSH`
that names a blob, keeps one binding per head, and asks `Host` to show and to
update. The QEMU glue gains four `RvkHostOps` entries that translate directly
onto `qemu_console_has_gl`, `qemu_console_gl_scanout_dmabuf`,
`qemu_console_gl_update` and `qemu_console_gl_release_dmabuf`, with no branch
of their own beyond the null checks the ABI forces.

**Rationale.** FR-007 requires it, Principle II requires it, and the two
decisions in the path - "does this framebuffer fit inside this blob" and "which
DRM fourcc is this virtio-gpu format" - are both validation of guest-authored
bytes, which is core's by Principle V. The existing `BlobCommands` /
`ContextCommands` pair (`VirtioGpuRvk::control`,
`src/device/module/virtio_gpu_rvk.cpp:97-98`) is the shape to follow.

**A scanned-out blob is pinned.** `BlobTable::pin`/`unpin` already exist and
`BlobTable::release` already answers `invalid_state` for a pinned blob, which
`BlobCommands::unref` already turns into `ERR_INVALID_PARAMETER`
(`blob_commands.cpp:46-50`). Making the scanout take a pin therefore costs one
call and gives `RESOURCE_UNREF` of a blob that is on screen a defined refusal
instead of a dangling dma-buf. The guest disables the scanout first, exactly as
it must for a 2D resource.

**The dma-buf covers the whole blob; the framebuffer's offset is the plane's
offset.** `GuestDmaBuf::make` refuses a range that is not whole pages
(`guest_dma_buf.hpp`), and a framebuffer inside a blob is not page-aligned in
general. `qemu_dmabuf_new` takes a per-plane `offset`, so the natural split is
one dma-buf over the blob and the offset carried in the geometry. It also means
one dma-buf per blob rather than one per flip.

## 7. Vertical blank from the device (FR-005)

**Decision.** Not built in this feature's device path, and the reason is
recorded rather than assumed: the device has no way to reach the guest with a
display interrupt. The guest recognises exactly one device interrupt today
(configuration-changed, `adapter.cpp:366`) and the 64 fence queues are the
scheduler's. A device-driven vblank is a new interrupt kind on both sides plus
a guest-side switch between `SyntheticVsync` and it, and none of it can be
proven without the guest.

**What is done here instead.** The device counts and timestamps every scanout
update it makes, so the pacing FR-005 wants to derive from the host is
*measurable* before it is *reported*; and the guest-side task is written out in
`tasks.md` with the interrupt bit it would need. `SyntheticVsync`'s 60 Hz timer
keeps its phase across enable/disable for the reason its header already gives,
and nothing in this feature disturbs it.

**Rationale.** Constitution VIII: no speculative structure. A vblank source
with one implementation and no consumer is exactly that, and the honest
ordering is that FR-005 follows the flip, not the other way round.

## 8. The capset bit, and what the guest does without it

**Decision.** One new bit,
`CapsetInfo::feature_dmabuf_scanout = 8u`
(`src/proto/context/capset_info.hpp`, beside `feature_host_visible_memory`,
`feature_gpu_exported_memory` and `feature_guest_memory_import`), set at
realize by the existing `HostFeasibility` (`src/device/probe/host_feasibility.hpp`)
from one question to `Host`. Without the bit the guest never sends
`SET_SCANOUT_BLOB` and `DisplayScanout` keeps the 2D path it has, unchanged and
already tested by feature 038's scenarios (SC-004).

**Rationale.** It matches the two feature bits that already exist, it is
decided once for the life of the device (the display backend cannot appear
later), and it puts the fallback where constitution VI wants it: the guest is
never told about a capability that is not there. The 64-bit `features_` field
already on the wire means **no capset size change and no new capability
structure**, which matters: feature 060 measured the PCI configuration-space
budget at 192 bytes with the express capability's 60 not fitting beside our two
shared-memory capabilities, so this feature deliberately adds **no PCI
capability and no BAR**.

**Alternative rejected.** Answering `SET_SCANOUT_BLOB` with
`ERR_INVALID_PARAMETER` and letting the guest infer the fallback: an error per
frame, a guest that has to guess whether the refusal was the display or the
framebuffer, and no way to log the decision once (FR-006).

## 9. What only the live guest, or merged 056, can answer

Each of these is one question with the one line that settles it. None of them
gate the device-side work.

1. ~~**Does `egl-headless` change what `screendump` sees?**~~ **Answered: it
   ends it.** With `--display egl-headless` the run reaches the device's
   "the display shows dma-bufs" line, the guest scans its primary out as a
   blob, and every `screendump` of that console answers "no surface" - because
   a console showing a dma-buf has none (§4). The flag is therefore *not*
   adopted unconditionally: the default stays `-display none`, where the 2D
   path and every screendump-based display scenario are exactly what they
   were, and the scanout scenario is what runs under the flag.
2. **Does this host have the pieces at all?**
   `ls /dev/udmabuf && qemu-system-x86_64 -display help | grep egl` and
   `ls /usr/lib/libgbm.so*`. If `CONFIG_GBM` was off when QEMU was built,
   `egl_scanout_dmabuf` is compiled out and the whole path is silently a no-op.
3. **Does the guest's primary ever become a blob?** It cannot today: the
   primary is a `shared_primary` standard allocation pinned into
   `ApertureSegment` (id 1, base `0xC0000000`) with guest system-memory pages
   (`src/core/memory/allocation.cpp:52-82`). Making it a blob is the guest-side
   half of rung 2 and belongs with 041.
4. **Does DWM compose on rvkgpu once 056 lands?** `SC-002`; the in-guest check
   is DWM's adapter selection, already written into 056's US2.

## 10. Summary of decisions

| # | Decision | Where it is proven |
|---|---|---|
| 1 | No new guest DDI slot; no flip queue, no MPO | §2, `miniport.cpp`, 022's clarification |
| 2 | `SET_SCANOUT_BLOB` + blob `RESOURCE_FLUSH` move into the device core | §6, `blob_commands.cpp:29` |
| 3 | The scanout is a dma-buf; guest pages via udmabuf first | §3 rung 1, `guest_dma_buf.hpp` |
| 4 | The core maps virtio-gpu format to DRM fourcc; the glue passes it through | §4, §6, Principle II |
| 5 | A scanned-out blob is pinned and cannot be unreferenced | §6, `blob_table.hpp` |
| 6 | One dma-buf per blob, the framebuffer offset carried as the plane offset | §6, `qemu_dmabuf_new` |
| 7 | `feature_dmabuf_scanout` capset bit, decided once at realize | §8 |
| 8 | No PCI capability, no BAR, no capset size change | §8, feature 060 |
| 9 | Host-image export is executor ABI v6, specified not built | §3 rung 3, `rvk_executor.h` |
| 10 | Device-driven vblank is deferred with its reason recorded | §7 |
