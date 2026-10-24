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
   sequence the end compares (the branch's history has it, `PagesReadRecord`,
   `PresentWaiters`, `PresentWrites`), or better, the host-side copy of
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
