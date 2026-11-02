# Stale frames in blt-model windows

**Status: reproduced and located, October 2026; not fixed. The fix is a
change of present path (§ "What fixes it"), not an ordering the driver can
add where it is.** The probe that catches it is
`tools/guest/probes/d3d9_dirty_present.exe` (docs/testing.md, "Probes from
an application's calls").

## The symptom

Affinity, dragging a gradient: the handles of earlier drag positions flicker
back in for a composition and go again. Half-Life 2 in a window: a frame
flickers to an older one. Seen through the window mirror and on the VM's
display alike; never in a `PrintWindow` of the window or a BitBlt from the
screen.

## What Affinity does

Traced at avumd's DDI (`AV_D3D_TRACE`) while a gradient was dragged: one
X8R8G8B8 back buffer under `D3DSWAPEFFECT_COPY`, never locked, and a present
every 3 to 20 ms of a `Clear` of one rectangle, a few draws under small
viewports, and those rectangles as the dirty region. What the window shows is
the back buffer as every present before left it. Half-Life 2 clears and
redraws the whole frame under `DISCARD`. Both are windowed blt-model presents.

## What the probe found

`d3d9_dirty_present` presents the same way, with a handle that moves a step a
frame and a bar code of the frame number along the top and along the bottom
of the window, and reads the window back three ways:

| Reader | What it sees |
|---|---|
| `PrintWindow`, `PW_RENDERFULLCONTENT` (DWM on request) | every frame whole, zero lag |
| BitBlt from the screen (DWM composes the region) | every frame whole |
| Windows.Graphics.Capture (DWM's own frames, as a mirror and the scanout take them) | one or two frames in a hundred **torn**: the top bar code says frame N, the bottom N-1 (or N+1) |

A torn frame is the window composed from a surface only partly written: the
erase rectangle of one present and the handle rectangle of the next, which is
exactly the stale handle Affinity flickers. Reading a captured frame twice,
3 ms apart, reads the same bytes: the tear is in what DWM composed, not in
the capture.

**The tear needs a later present.** Presents 50 ms apart - DWM composes each
before the next arrives - never tear in 400 frames. Presents 4 ms apart tear
2 in 100; back to back, 14 in 100.

## Where the pixels go

Taken from the driver's state dump while the probe ran, and from a DDI trace
of DWM itself (`AV_D3D_TRACE` set system-wide):

1. The present's destination is a *staging* standard allocation
   (`D3DKMDT_STANDARDALLOCATION_STAGING`, kind 3) the size of the client
   area, in the **presenting process's own device** - dxgkrnl's legacy
   redirection path, taken because `PresentationCaps` is 0
   (docs/research/d3d-desktop/reverse-engineering.md §4). The present engine
   moves the back buffer's frame into its pages on a thread of the host
   device, and the miniport parks the present's packet until it has
   (`presents_ticketed` = `presents_parked`, every one).
2. **Nobody of this driver's reads that allocation.** DWM never opens it (one
   open, the app's device's), no present reads from it (the engine's tickets
   are exactly the application's), and `avumd` in DWM never sees it.
3. DWM's D3D11 device, traced, uploads the window once per composition with
   `ResourceUpdateSubresourceUP` **from a CPU buffer of DWM's own** - the
   dirty bound of the present, pitch 5192 for a 1282-wide window, which is
   neither the staging allocation's pitch (5120) nor any allocation of this
   driver's - into its own texture, and draws it.

So between 1 and 3 the kernel copies the staging pages into DWM's bitmap with
the CPU, at or near composition time, through no DDI, open, lock or fence
this driver can see. That copy of present N and the engine's write of present
N+1 into the same pages are two machines with no handshake. When they
overlap, DWM's bitmap holds part of each, and the window shows it for one
composition.

## What was tried, and measured not to be it

Both of these were built, unit-tested, installed in the guest and run against
the probe; neither changed the tear rate.

- **The present returns only once its pixels have landed**
  (`DxgkDdiPresent` sleeping on the engine's page until the ticket completes;
  17,478 sleeps over 3,000 presents, no timeouts). Rules out a read of the
  *same* present's pages before they were written. Rules in a later present:
  the race is with N+1.
- **The copy made when dxgkrnl submits the packet, not when it builds it**
  (the present described at `DxgkDdiPresent`, moved at
  `SubmitCommandVirtual`, where a GPU would run the DMA buffer), so that a
  packet VidMm holds back while the allocation is locked would hold the copy
  too. dxgkrnl does delay submissions under load - 606 of 1,500 by more than
  100 µs, up to 7.5 ms - but not for DWM's read: the tear rate is unchanged.
  VidMm does not hold a present into a locked aperture allocation on a GPU
  virtual addressing driver, and the kernel's copy takes no lock the miniport
  is told of.
- Ordering DWM's `avumd` refresh of a GDI surface
  (`D3d11Device::refresh_guest_surface`) after the engine by an escape: DWM
  never took that path for this window (one open of the staging allocation;
  the escape's waiters never slept). That path is the one DWM takes in the
  *non*-legacy present path, below.
- The capture's own read racing DWM's write of the capture texture: a frame
  read twice reads the same bytes.

## What fixes it

On a real GPU the blt into the redirection surface and DWM's read of it are
serialized by the one hardware queue, or by a GPU-composed path where DWM
samples the surface itself. This driver has neither in the legacy path: the
writer is a host thread and the reader is the guest kernel's CPU. Nothing
inside `DxgkDdiPresent`, the packet, or the engine can see the reader, so no
ordering added there can be correct.

The fix is to leave the legacy path:

1. **Report `DXGK_PRESENTATIONCAPS.SupportKernelModeCommandBuffer`** (RE §4:
   without it, and without `SupportSoftwareDeviceBitmaps` for types 6 and 8,
   CDD creates the staging-style allocations above). With it, CDD creates
   `D3DKMDT_STANDARDALLOCATION_GDISURFACE` allocations, which the miniport
   already describes (`StandardAllocation::describe_gdi`), and DWM opens them
   through `avumd` (`D3d11Device::open_guest_surface`, written for this and
   never exercised) and composes from the host texture it refreshes from
   their pages.
2. That makes the reader `avumd` in DWM's process - a read this driver can
   order after the engine's write. The ordering is the escape that was built
   and withdrawn here: a begin that waits for the pages' last present and a
   sequence the end compares (`PagesReadRecord`, `PresentWaiters`,
   `PresentWrites`) - and, since a compare that fails only defers the band, and
   under continuous presents nearly every compose overlapped a write and the
   window starved on stale content, a gate: a begin opens a read
   (`PresentReads`) that the presents into the pages wait for until the end,
   so the compare fails only when a hold ran out (docs/protocol.md, "Pages
   read record"). Or better, the host-side copy of
   docs/d3d-desktop.md §3.5 - the frame copied on the host GPU into the
   texture DWM composes from, ordered with DWM's draws on the host's queue, so
   no pixel crosses the guest's CPU at all.
3. `SupportKernelModeCommandBuffer` also means `DxgkDdiRenderKm`: CDD sends
   GDI's own drawing (bitblt, colour fill, alpha and transparent blends,
   stretch, ClearType) as kernel-mode command buffers for the driver to run.
   That is GDI hardware acceleration, a feature of its own, and the reason
   the caps were left at 0.

Until then the window is right in every reader but the composition, and the
composition is wrong for one frame, one or two times in a hundred when the
application presents faster than DWM composes.

## What `SupportSoftwareDeviceBitmaps` does to a Direct3D 9 window (measured)

Reporting `SupportSoftwareDeviceBitmaps` (0x10000000, as WARP does) leaves
the legacy path. Here is what happens to a windowed D3D9 present, from ETW
`Present`, `Blit` and `BlitRect` events and the adapter's present counters
(`presents_made`, `presents_refused`, `presents_unpointed`,
`last_present_flags`, `last_present_destination_kind` in the state dump):

- d3d9.dll opens the window's redirection surface through `pfnOpenResource`
  as the swap chain's primary. The surface covers the **whole window rect**,
  invisible borders included (1298x847 for a 1280x800 client area), and
  every present after that names it as `hDstResource`.
- The present has to keep that surface resident (`keep_resident`). If it
  doesn't, dxgkrnl fails it with `VidSchErrorSubmittingRenderForNonResidentAllocation`
  and the device is lost.
- dxgkrnl records each present as `Blt|RedirectedBlt` with a
  `D3DKMT_PM_REDIRECTED_BLT` history token. It emits a `Blit` from the back
  buffer's full rect to the client rect inside the surface ((9,38)-(1289,838)
  here), on a software command buffer. **`DxgkDdiPresent` is never called for
  it**: `present_rows` stays at 0, and nothing is refused or left unpointed.
- That kernel blit writes nothing visible, even when the back buffer's pages
  hold the frame (tried by reading the frame back into them before
  `PresentCb`). The window stays white, which is the surface's GDI fill.
- When the UMD writes the frame into the redirection surface's pages itself,
  DWM shows it, refreshed through `open_guest_surface`. It lands at the
  surface's (0,0), though, so it sits under the caption, because
  `D3DDDIARG_PRESENT` carries no rectangle and no window. Only the kernel
  knows the client rect.

So enabling the bit would need one of these:

1. The UMD places the frame itself. That needs the window's client rect,
   which `avumd` can only find through user32 (its image check forbids user32
   today) or through a size-matching heuristic. Neither is a sound contract.
2. Make dxgkrnl hand the redirected blt to `DxgkDdiPresent`, which already
   receives the rects and has the ordered write path. What decides between
   the software blit and the miniport is not known yet. The candidates are
   the back buffer's CPU visibility and its segment.
3. Report `SupportKernelModeCommandBuffer` and run the blit in
   `DxgkDdiRenderKm` (point 3 above). Then the kernel's rectangles arrive
   at the miniport as a command.

The caps stay at 0 until one of these is chosen.

## Kernel-mode command buffers (measured, build 26200)

Reporting `SupportKernelModeCommandBuffer` with `DxgkDdiRenderKm` registered,
then again with valid GDI limits (`AlignmentShift` 2, texture shifts 3,
`PresentationCaps` 0x6c804), on a cache-coherent aperture: dxgkrnl starts the
adapter, but nothing changes. CDD still creates `Staging` and
`StagingCpuVisible` GDI surfaces (ETW `CddStandardAllocation`), never creates
a GDI context, and never calls `RenderKm` (`renderkm_calls=0`). The D3D9
redirected blt still arrives through `DxgkDdiPresent` into a staging
allocation (`last_present_destination_kind=3`), which is the legacy path and
its tear. The cap is honoured somewhere this driver cannot see. The open
question is what else CDD or dxgkrnl requires before it accelerates GDI for
a GPU-VA (WDDM 2.x) adapter. Public documentation doesn't say.

## What Windows orders, from its binaries (build 26200, public symbols)

Decompiled from `dxgkrnl.sys`, `dxgmms2.sys`, `cdd.dll`, `win32kbase.sys`
and `win32kfull.sys`. VirtualBox's WDDM driver (open source) reports the same
legacy `PresentationCaps` as this one. It moves a D3D surface -> staging blt
inside the present's DMA buffer (`SVGA3D_CMD_SURFACE_DMA`, `gaPresentBlt`).
It is WDDM 1.2 without GPU virtual addressing.

- **The app's redirected blt** (`DXGCONTEXT::Present`, flag 0x10000), in order:
  1. `CWin32kLocks::Lock(hwnd)`.
  2. `DxgkEngAddRedirBitmapD3DDirtyRgn`, which does two things:
     - The PDEV's `DrvAccumulateD3DDirtyRect`, under the CDD bitmap's
       semaphore. For `CddBitmapHw` that queues GPU waits through
       `CDDSYNCOBJECT`. For `CddBitmapSw` it only marks the bitmap dirty.
     - `GreAddBitmapD3DDirtyRgn`, which notifies DWM's sprite with
       `bSpDwmNotifyDirty`. This happens **before** the present is submitted.
  3. `SubmitPresent`: `DxgkDdiPresent`, then the packet is queued.
  4. `SubmitPresentHistoryToken` (REDIRECTED_BLT, with the dirty rects).
  5. Unlock.
- **A GDI-side access** to a CDD bitmap the app wrote, two paths:
  - `CddBitmapHw` uses `SyncDXAccessInternal`: `IssueSignalForDxProcess` +
    `IssueWaitForGdi`, which are GPU waits on the contexts.
  - `CddBitmapSw` uses `CDDPDEV::SyncGPUAccess` -> `DxgkCddSyncGPUAccess` ->
    `VidMmWaitOnAllocationDmaReferences`. That calls
    `WaitOnAllDMAReferencesExceptThis`, which flushes every other busy
    device that has the allocation open, i.e. waits for the packets already
    queued. Nothing in it holds back a packet queued later.
- **VidMm's CPU lock** (`VIDMM_GLOBAL::Lock1`, `WaitOnAllocForceSync`) only
  flushes the allocation's owning device.

Every Microsoft ordering here is stated in packets: a reader waits for the
queued packets to complete, and a writer is held by waits queued on its
context. None of it can see a write that starts in `DxgkDdiPresent`, before
the packet is queued, which is what this driver does today.

The trace adds one more fact. While the probe presents, nobody calls
`DxgkCddSyncGPUAccess`, `DxgkCddLock` or a CDD sync object (dxgkrnl's
profiler events). CDD creates, locks and unlocks the window's allocations
only at window creation. So the per-frame reader of the staging pages holds a
mapping it took earlier, and syncs through none of the paths above. That is
consistent with the submit-time move not changing the tear rate. Who that
reader is remains open: candidates are win32k's sprite and redirection code
reading the CDD surface's mapping at DWM's request.

## The path taken: software device bitmaps, and the address the app's device maps

`PresentationCaps` is `SupportSoftwareDeviceBitmaps` and nothing else (the
kernel-mode command buffer experiment is withdrawn: CDD never accelerated GDI
for this adapter). With the bit CDD makes a window's redirection surface a
GDISURFACE allocation, `d3d9.dll` opens it into the application's device, and
every present names it as `hDstResource`. The two things that kept the first
attempt blank are answered:

- **dxgkrnl rejects the present before `DxgkDdiPresent`** when the destination
  has no GPU virtual address in the presenting process
  (`SubmitPresentWithDmaBuffer`: "the virtual address is not assigned to the
  destination allocation"). `D3d9Device::open_resource` therefore maps the
  opened surface through `pfnMapGpuVirtualAddressCb` on the device's paging
  queue (`PresentedAllocations::keep_addressable`) and frees the address with
  the resource. Nothing on the GPU reads through it - the miniport resolves
  allocations by handle - so the paging fence is not waited for.
- **The miniport had no pages to write.** A `gdi` allocation was left in
  VidMm's own system memory, which is never mapped into the aperture. It is
  pinned in the aperture and mapped cached like a staging one
  (`Allocation::report_to`), so `DxgkDdiPresent` writes the redirected blt
  through its pages and the `PresentWrites` ordering of the window's reader
  holds as before.

Not measured yet: that the D3D9 runtime fills `pfnMapGpuVirtualAddressCb`
(`d3d9.dll` imports `D3DKMTMapGpuVirtualAddress` and `D3DKMTFreeGpuVirtualAddress`
beside the paging queue calls the device already uses), and that `DxgkDdiPresent`
is reached with the destination rectangle the client area sits at.

## The same for a Direct3D 10/11 blt-model window

With the bit, DXGI opens the redirection surface of a `DXGI_SWAP_EFFECT_DISCARD`
window into the application's D3D11 device as well (`D3d11Device::open_resource`
-> `open_guest_surface`, the path DWM's reads use) and names it as `hDstResource`
of `Present`/`Present1`. Two things differ from the D3D9 path:

- **The surface is addressed when first presented into, not when opened.**
  DWM's device opens the same kind of surface, reads it and never presents into
  it, so a residency wait and a GPU virtual address there would only cost its
  compose. `D3d11Device::address_destination` runs `keep_addressable` before the
  first present naming the surface and the address is freed in
  `destroy_resource`.
- **A blt-model present with a destination is read back like one without**
  (`present_pages`): the host's pixels are staged at a point named in the
  present's private data, or read into the buffer's pages, and the miniport
  moves them into the surface (`Adapter::move_presented_pixels`). Before, a
  present with a destination went to the kernel with no frame staged at all.

The host texture and CPU shadow `open_guest_surface` makes for the application's
open are unused by it (nothing binds the surface); they are one allocation and
one upload at open.

Not measured: where the hung `Present1` waited. Everything this driver does in
the present before and after `pfnPresentCb` is bounded (`PresentOrder::submitted`
waits for nothing on a blt chain, `PresentWaiters::admit_write` for 20 ms), so
the wait is inside dxgkrnl's present of a destination that was neither resident
nor addressable.

### The first Direct3D 11 present is lost with `Blt|RedirectedBlt|CrossAdapter`

Guest, `d3d11_flip blt`: the first `IDXGISwapChain::Present` returns
`DXGI_ERROR_DEVICE_REMOVED` (reason `DEVICE_HUNG`), `D3DKMTPresent` answers
`0xC01E0200`, and dxgkrnl marks the device in error with
`VidSchErrorSubmittingRenderForNonResidentAllocation`. ETW `Present` flags are
`0x4011081`.

- **`CrossAdapter` (`D3DKMT_PRESENT` bit 0x04000000) is the runtime's, and
  dxgkrnl does not act on it before the miniport.** In `DXGCONTEXT::Present`
  (build 26200, decompiled) its only reader is an assertion that a device with
  no display adapter carries it ("`GetDisplayAdapter() != NULL ||
  pPresent->Flags.CrossAdapter`"); `SubmitPresent` and
  `SubmitPresentWithDmaBuffer` never branch on it. `d3dkmthk.h` says what it
  is for: `hAdapter` is "iGpu adapter for PHT redirection. Valid only when the
  CrossAdapter flag is set" - the present history token goes to another
  adapter. So DXGI believes the window's compositor is not this device's
  adapter. That is not decided by anything avumd reports (the Direct3D 11 DDI
  has no cross-adapter capability); why it holds for the adapter that has the
  output is **not established**.
- **What `SubmitPresentWithDmaBuffer` checks on a GPU-virtual-address context**
  (not a flip): the destination's GPU virtual address in the presenting
  process - zero rejects the present with `STATUS_INVALID_PARAMETER`, "the
  virtual address is not assigned to the destination allocation" - and
  nothing of the source's, whose address it looks up and hands on. Both
  allocations are named in the DMA buffer's allocation list, so both have to be
  resident for the device. The destination is (`address_destination`); the
  source was asked at its creation (`PresentedAllocations::make`) with the
  answer dropped, so `D3d11Device::keep_present_resident` asks again, once, at
  the first present naming any buffer or surface, and says it once
  (`a present's allocation made resident`). Not shown to be the cause: a
  device already resident never errors for it.
- **CDD's type 8 is back.** `D3DKMDT_GDISURFACETYPE_TEXTURE_CPUVISIBLE_CROSSADAPTER`
  (8) is described as type 7 is (`StandardAllocation::describe_gdi`): pitch
  `align(width * bpp, 128)`, which dxgkrnl checks, and dxgkrnl itself asks for
  the global share handle. An opened `cross_adapter` surface now gets a GPU
  virtual address range as a GDI one does, so a present into it is not
  dropped.
- **Risks, unmeasured.** A `cross_adapter` allocation is not pinned in the
  aperture (its pages are the process's own, lent to the host), so the miniport
  has no pages to write when one is a present's destination: `Adapter::present`
  logs `present surfaces reachable ... destination 0 kind 6` and moves nothing,
  and DWM's `avumd` reads it through the host lending, not through
  `open_guest_surface`. Direct3D 9 refuses an opened allocation that is not a
  `gdi` one (`D3d9Device::refuse_open`), so if CDD made type 8 for a window
  that was type 6 while it was refused, Direct3D 9 windows now fail to open it.
  Both are said in the log line each prints.

## What GDI reads with the bit on: `GetDC` on a DXGI surface

With the bit, `IDXGISurface1::GetDC` on a GDI-compatible texture calls the
driver's `ResolveSharedResource` and hands GDI the allocation's own pages. There
is no staging copy, and nothing calls the driver at `ReleaseDC`. Direct2D's DC
render targets take this path, and a no-op there leaves GDI drawing on an empty
surface and the GPU never seeing what GDI drew. WizTree (a Delphi VCL style
engine) scales its caption-button skin through Direct2D into DIBs and alpha
blends those into its title bar, so it had no minimize, maximize or close
button and no combo box arrow. It looked like a first-launch-after-boot bug
because a bit-off driver had filled those pages for the rest of the boot.

`D3d11Device::show_to_gdi` reads the host's pixels into the pages at the resolve
and keeps a copy of the rows; `take_gdi_writes` compares the pages to the copy
at later calls and uploads what changed. Cold boot, WizTree first: the buttons
are drawn, and `d2dprobe` reads 2560 of 2560 pixels back on every Direct2D case.

The driver cannot ask whether a texture is GDI-compatible, or keyed: the runtime
keeps `D3D11_RESOURCE_MISC_GDI_COMPATIBLE` and `SHARED_KEYEDMUTEX` from `MiscFlags`,
and both kinds arrive as `misc=0x2` with the same bind flags. It resolves on every
keyed `ReleaseSync` as well, for the other process, which reads through the host's
object and needs no pages. Read back each time, that was 660 ms a frame at 2560x1440.
What tells them apart is the runtime's own `Flush`: it comes straight after the
resolve on a release (1.3 to 14.8 us from its return, n=12), while the next call
after a `GetDC` is 65.8 us away at the least (n=68), since GDI has made a DC and
released it. A `Flush` within 30 us marks the texture as shared, and it is never
read back again: 21 ms once, then 0.07 ms a frame.

So what is not served: `GetDC` on a texture that is also keyed, or in a second
process that opened it (`allocation_owned`). `show_to_gdi` also goes by the formats
`GetDC` takes, B8G8R8A8 and B8G8R8X8. Nothing measured so far does either.

Direct3D 9's `GetDC` goes through the driver's lock, so it needs none of this:
`d3d9dc` (32 and 64 bit) reads GPU pixels through GDI and GDI pixels through the
GPU on lockable render targets, default-pool surfaces and the back buffer. The
runtime refuses `GetDC` on a non-lockable render target before the driver hears
of it.
