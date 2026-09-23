# The desktop on Direct3D: design

What it takes for DWM, and Direct3D 11 and 12 applications, to run on rvkgpu
with their pixels rendered and shown by the host GPU. Written after the first
boots where DWM ran on `rvkumd.dll`, from three sources of evidence, each kept
beside this note:

- **What the guest did** - live runs on the Win11 26100 guest, the driver's
  trace table and log, ETW traces of the Direct3D 11, DXGI and DxgKrnl
  providers, and bugcheck records read out of guest RAM.
- **What Microsoft's binaries require** - [`research/d3d-desktop/reverse-engineering.md`](research/d3d-desktop/reverse-engineering.md):
  dxgkrnl, dxgmms2, d3d11, dxgi and basicrender read in Ghidra, every claim
  with the function and address it came from.
- **What drivers that solved this do** - [`research/d3d-desktop/prior-art.md`](research/d3d-desktop/prior-art.md):
  VirtualBox's WDDM driver (the same shape as ours: a D3D11 UMD sending API
  commands to a host that replays them on a host GPU), viogpu3d, Mesa's
  d3d10umd and VMware's screen targets, read at pinned revisions.

It replaces nothing in `specs/`; where it disagrees with a feature spec, the
evidence here is newer.

## 1. What is true now

DWM composes on `rvkumd.dll` (the INF names it as a `%13%` path) over one host
session per process (`D3d11Session`), and the guest boots to a desktop that is
partly right. The failures that remain, and what each one is:

| Seen | Cause | Source |
|---|---|---|
| explorer burns a CPU core; the KMD saw 44 150 `CreateDevice` and 19 013 `CreateAllocation` in one boot | explorer calls `IDXGIDevice2::OfferResources`; our `pfnOfferResources1` answers `E_NOTIMPL`; the runtime logs *Removing device due to driver error* (0x887A0020) and explorer makes the device again, ~200 times a second | ETW `Microsoft-Windows-Direct3D11` journal, pid 4836; `DxgiBaseTable::until_later` |
| 7 of 22 DXGI base-function slots answer `E_NOTIMPL`: `OfferResources1`, `ReclaimResources`, `ReclaimResources1`, both `PresentMultiplaneOverlay`s and both reserved slots | the D3D11 driver overrides 8 slots and leaves the portable table's `until_later` placeholder in these | `fill_dxgi_funcs`, `fill_dxgi_base_functions`, `DxgiBaseTable::until_later` |
| 0x119/0xB after DWM restarts | on the device owning the primary going away, dxgmms2 flips a *disable-planes* entry through `SetVidPnSourceAddressWithMultiPlaneOverlay` for every WDDM 2.3+ driver, MPO caps or not, and bugchecks on any failure | RE §1 (`VidSchiSubmitDisablePlanesFlip`, `VidSchiExecuteMmIoFlip`) |
| (latent) that flip can never complete | a multiplane flip entry only retires on `CRTC_VSYNC_WITH_MULTIPLANE_OVERLAY` (7); our synthetic vsync reports only `CRTC_VSYNC` (3), which never matches it | RE §5 (`VidSchiIsExpectedVSyncCookie` @14003a1ec) |
| the screen shows noise after a DWM restart | the scanout attached backing once and never again (the host refuses a second attach: 9 092 refusals in one boot), and never noticed new pages at an address it already showed | fixed in `efd3928a` |
| DWM uses ~725 CPU-seconds in a few minutes | every present reads the whole 4 MB back from the host into the guest allocation, and the scanout copies it back to the host (`TRANSFER_TO_HOST_2D`) | `D3d11Device::publish`, `ScanoutResource::flush` |
| one `OpenResource` of a resource no session made fails | GDI windows reach DWM through surfaces win32k's CDD creates through the KMD; DWM opens them and the UMD receives the KMD's own standard-allocation blob, which it does not parse | RE §4 (`DxgkCddCreateAllocation`, `DXGDEVICE::CreateStandardAllocation`) |

The kernel bugs this work already found - a DPC reporting fences twice, a
preemption answered while the host still ran a batch, an invariant judged
outside its lock, flips with no DMA buffer refused - share one cause with the
table above: the fakes the unit tests run against encoded what we believed
dxgkrnl does, not what it does.

## 2. Principles

Each is taken from the evidence, and each rules something out.

1. **A frame never crosses guest memory unless the CPU drew it.** Both open
   drivers present by *naming a host image*: VirtualBox's flip emits
   `BLIT_SURFACE_TO_SCREEN(sid)`, viogpu3d's scanout thread sends
   `SET_SCANOUT(res)` for the host 3D resource (prior art §VirtualBox,
   §viogpu3d). The per-present readback goes.
2. **The host owns what is scanned out.** For a console with only a 2D
   surface, the host copies the image into it on the host GPU and reads it
   back there (VirtualBox's model, `DevVGA-SVGA3d-dx-dx11.cpp:3975-4060`); for
   a GL console it hands the image over as a dma-buf (viogpu3d, vmwgfx screen
   targets). One wire command serves both.
3. **Every image the kernel knows has a host identity that travels with the
   kernel allocation.** VirtualBox writes a device-global surface id into the
   allocation; a process opening another's surface then names the same host
   object with no import (prior art, `VBoxDX.cpp:1110-1120`). We have the
   mechanism already - the 16-byte resource private data carries a shared
   key - and use it for shared resources only.
4. **Answer every DDI slot on purpose.** A slot the runtime can call answers
   what the driver does, or the documented "none" of it; `E_NOTIMPL` from a
   slot the runtime considers mandatory removes the device.
5. **A refused command removes one device, not the process.** Today one
   refused command loses the session, which is every device in the process
   (DWM's whole composition).
6. **The fakes encode dxgkrnl as read, with the address.** A rule a fake
   enforces cites the function it was read from, as `FakeVidSch` now does for
   preemption.

## 3. Target design

### 3.1 Host image identity

Every Direct3D resource that gets a kernel allocation (presentable, primary,
shared, and anything the KMD may be asked to scan out) is made on the host
under a **host key** and registered in the GPU's `D3dSharedResources`, so any
session on that GPU can name it. The key is the one the resource private data
already carries (`resource_private_bytes = 16`: tag and key); today it is
non-zero only for `MISC_SHARED` resources, and it becomes non-zero for every
kernel-visible one. The KMD keeps it on the `Allocation` when the allocation is
made (`AllocationDescriptor` already parses the private data), so a primary the
KMD is asked to show can be named to the host by key.

Allocations no UMD made - CDD's GDI surfaces and dxgkrnl's standard
allocations - have no key. The KMD's `GetStandardAllocationDriverData` writes a
blob with a *standard* tag and the geometry (standard type, GDI surface type,
width, height, pitch, format), and the UMD's `OpenResource` recognises it
(§3.5).

### 3.2 Presenting without pixels

- **Flip presents** (DWM's swapchain, flip-model applications): the UMD sends
  `PresentImage11(resource)`, synchronous, whose host handler submits the
  context's work to the host GPU queue, and then calls `pfnPresentCb` with the
  source allocation. No readback. The synchronous round trip is what orders
  the host's rendering before dxgkrnl programs the flip; it costs a round
  trip, not a frame.
- **Blt presents** into a destination with a host key are a host copy
  (`CopySubresourceRegion` on the host), as VirtualBox's `DX_PRESENTBLT`.
- **Readback survives only for CPU destinations**: a GDI shadow or staging
  surface with no host image (VirtualBox adds its readback exactly there).

### 3.3 The scanout names a host image

`DisplayScanout` gains a third backing beside the 2D resource and the blob:
when the primary `SetVidPnSourceAddress` (or a multiplane flip's base plane)
names has a host key, the KMD sends **`SET_SCANOUT_IMAGE(head, key, width,
height)`** on the control queue when the shown key changes, and
**`SCANOUT_IMAGE_FLUSH(head)`** when a frame was presented. A primary without a
key (the boot and basic-display desktop, CDD primaries) keeps the guest-page
path.

On the host, a device-level **`ImageScanout`** keeps each head's key and, on a
flush, asks the executor for the pixels through a new executor ABI operation,
`read_image(key, destination, stride, width, height)`: the executor copies the
image into a host staging texture on the D3D11 device, maps it, and writes
into QEMU's console surface, off the main loop, which then announces the
update (`dpy_gfx_update`). With a GL console the same key is exported as a
dma-buf (`export_object` already exists for Vulkan objects) and shown through
`DisplayHeads`. Nothing is read back to the guest.

### 3.4 Flip completion

`SyntheticVsync` reports what dxgmms2 matches (RE §5):

- after `SetVidPnSourceAddress`: `CRTC_VSYNC` with the programmed address
  (today's behaviour);
- after a multiplane flip that disables the base plane: `CRTC_VSYNC_WITH_MULTIPLANE_OVERLAY`
  with zero layers, which is the only report that retires a disable-planes
  entry;
- after a multiplane flip with the base plane enabled: type 7 with layer 0
  enabled at that address.

`DisplaySources`, which already keeps each source's primary and address
atomically for the vsync, keeps the kind of the last programmed flip (and a
base plane's attributes, which dxgmms2 compares) beside them. MPO stays unreported in the caps; the only
multiplane flips are the ones dxgmms2 makes on its own.

### 3.5 GDI surfaces DWM opens

The UMD's `OpenResource`, given the KMD's standard blob, makes a host texture
of that geometry under a key the KMD assigns to the allocation (the KMD owns
the allocation, so it can mint a key the host has never seen and hand it back
in the blob), and fills it from the allocation's guest pages. GDI is the CPU,
so this is the one place pixels cross guest memory by design. When they cross
- at open, and again when DWM composes after GDI drew - is settled by the
measurement in §6.

### 3.6 Failure model

- The host keeps a removed reason per device (per context state). A command
  the host refuses removes the *selected device* and is otherwise ignored; the
  session and the other devices go on.
- Every synchronous reply carries the device's removed reason; the UMD passes
  a non-`S_OK` one to the runtime through `pfnSetErrorCb`, which is how D3D
  applications (DWM included) learn to recreate a device.
- Only a transport failure - the ring or the kernel refusing a submission -
  loses the session.

### 3.7 DDI completeness

Every slot of every table the UMD hands the runtime has a decision recorded in
code: what it does, or the documented "none". The gaps are the three
residency slots: `OfferResources1` and `ReclaimResources*` accept and never
discard, since the host's memory is not the guest's to reclaim. The
multiplane-overlay presents and the reserved slots stay refusals, because the
caps say one plane and the runtime never calls them - which a test states. A
unit test walks the filled tables and fails on a null slot, or on an
`E_NOTIMPL` from any slot the caps make reachable, for a valid device.

### 3.8 Capacity

Measured at the desktop: 7 processes load `rvkumd.dll`; each holds one session
(one doorbell of the adapter's 64), a 4 MiB ring and a 64 MiB blob arena. The
arena is guest memory the host imports: 7 × 64 MiB is 448 MiB of a 4 GiB
guest. The arena drops to 16 MiB, and an upload larger than it crosses in
arena-sized pieces rather than failing.

## 4. Classes

| Class | Kind | Where | State | Responsibility |
|---|---|---|---|---|
| `HostImageScanout` | object | `src/core/display/` | the key shown and whether a frame waits | shows a keyed primary through `SET_SCANOUT_IMAGE` |
| `ImageScanout` | system | `src/device/display/` | each head's key and its surface | keeps each head's console surface showing its image |
| `D3dHostImages` | system | `src/device/executor/` | a staging texture per size | reads a keyed image into memory the device gives |
| `DeviceRemoval` | object | `src/device/executor/` | removed reason per device | says which device a refused command removed |
| `StandardAllocationBlob` | object | `src/proto/resources/` | the fields of the KMD's standard blob | encodes and decodes it for the KMD and the UMD |

Extended: `D3dBackend` (`PresentImage11`, removal), `D3dChannel`/`D3d11Device`
(present, error path), `DisplayScanout` (third backing), `DisplaySources`
(the kind of the last flip), `SyntheticVsync` (report kind), the executor ABI (`read_image`, version 7), the D3D wire
(`PresentImage11`, removal reasons in replies), the DXGI table.

## 5. Order of work

Each step ends in a guest run judged by numbers, not by eye.

1. **What exists, made correct.** The DXGI table complete (§3.7); flip
   completion for multiplane entries (§3.4); the failure model (§3.6); the
   arena at 16 MiB with pieced uploads (§3.8). *Done when* explorer makes one
   device per use instead of 200 a second, DWM survives three restarts
   without a bugcheck or a stalled flip, and the desktop still renders
   through today's readback.
2. **The host scans out host images.** Keys for every kernel-visible resource
   (§3.1), `PresentImage11` and presenting without readback (§3.2),
   `SET_SCANOUT_IMAGE` and the host's `ImageScanout`/`read_image` for the 2D
   console (§3.3). *Done when* a screendump after boot shows the wallpaper and
   taskbar with no noise, the device counts zero `TRANSFER_TO_HOST_2D` for
   DWM's frames, and DWM's CPU time falls by an order of magnitude.
3. **GDI windows.** The standard blob and `OpenResource` of it (§3.5). *Done
   when* a GDI application's window (Notepad) appears in a screendump with its
   text.
4. **Applications.** The D3D11 probe run in the interactive session (not the
   agent's session 0, where a windowed swap chain is unavailable) presents
   its colour, and a D3D11 sample renders. Then D3D12 over vkd3d-proton,
   designed from the same evidence.
5. **GL consoles.** The dma-buf export of a keyed image to `DisplayHeads`.

## 6. Open questions, and what settles each

| Question | Settled by |
|---|---|
| Which GDI surface types CDD requests with our caps (`PresentationCaps = 0` takes its legacy path, RE §4) and which of them DWM opens | log each `GetStandardAllocationDriverData` request and each UMD `OpenResource` of a keyless allocation for one boot |
| When a GDI surface's pixels change, and so when §3.5 must refresh the host copy | the same log, with the CDD presents (`DxgkDdiPresent` blts from GDI surfaces) that follow |
| Whether dxgkrnl accepts a type 7 vsync from an adapter without MPO caps | the first DWM restart after step 1: the disable-planes flip completes or the flip-queue timeout fires |
| What the flip-queue timeout escalates to | not needed if the above holds |
| Whether `read_image` at 60 Hz costs the host less than the guest round trip it replaces | the host's frame time in the device's state report, before and after step 2 |
