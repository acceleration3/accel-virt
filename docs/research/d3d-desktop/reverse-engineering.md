# DWM flip / MPO / GDI surfaces: what the Microsoft binaries do

Binaries: `re/bin` (Win11 26100), queried through Ghidra (`tools/re/q.py`); every claim names the function and address it was read from, so it can be re-read there. The decompilations themselves are not kept in the tree.
"Verified" means the decompiled or disassembled condition was read. "Inferred" means it was not.

Offsets in DXGADAPTER (dxgkrnl) that come up below. Each one was identified from the code that uses it.

| offset | meaning | evidence |
|---|---|---|
| +0x978 | copy of DXGK_DRIVERCAPS (0x250 bytes) | `DxgkCddGetDriverCaps` @1404070c0 copies 0x250 bytes from `adapter+0x978`. So +0x9ac is PresentationCaps, +0x9b4 FlipCaps and +0x9bc MemoryManagementCaps. |
| +0x280 | driver pfnSetVidPnSourceAddress | `ADAPTER_DISPLAY::DdiSetVidPnSourceAddress` @140024aa8 |
| +0x368 | pfnSetVidPnSourceAddressWithMultiPlaneOverlay (MPO1) | `ADAPTER_DISPLAY::DdiSetVidPnSourceAddressWithMultiPlaneOverlay` @1400272a0 |
| +0x3e8 | pfn...MultiPlaneOverlay2 | `DdiSetVidPnSourceAddressWithMultiPlaneOverlay2` @14006e934 |
| +0x400 / +0x408 | pfnCheckMultiPlaneOverlaySupport3 / pfnSetVidPnSourceAddressWithMultiPlaneOverlay3 | `SupportCheckMultiPlaneOverlaySupport3` @1400483a0, `ADAPTER_DISPLAY_DdiSetVidPnSourceAddressWithMultiPlaneOverlay3` @1400285c0 |
| +0x4c8 | pfnGetMultiPlaneOverlayCaps | `DdiGetMultiPlaneOverlayCaps` @14039e5bc |
| +0xb93 / +0xb94 | effective "DirectFlip supported" / "MPO supported" | `DXGADAPTER::Initialize` @1401a2818 prints "Driver reports SupportMultiPlaneOverlay cap but DirectFlip is not supported" when b94 is set and b93 is clear |
| +0xbe2 / +0xbe3 / +0xbe4 | which MPO DDI generation to use | set only `if (b94)` in `DXGADAPTER::Initialize` |
| +0xc01 | "use MPO3" | `DXGADAPTER::ReadConfig` @1401a73dc |
| +0xac8 | the driver's DXGK_WDDMVERSION (inferred: it is compared with 0x2100, 0x2200 and 0x2300) | `IsLegacyDisplayStateSynchronization` @14005829c |

---

## 1. How a DWM present travels, and why the MPO DDI gets called

### Runtime side (dxgi.dll, d3d11.dll)
- DWM's desktop swapchain is `CDXGISwapChainDWMLegacy`, created by `CreateSwapChainDWM` (dxgi @18008e200). Its
  `PresentDWM` @1800402b0 forwards to `CDXGISwapChain::PresentDWM` @180019130, then to `PresentImpl`, then to
  `PresentImplCore` @180013c50. For DWM's fullscreen swapchain the present becomes
  `PresentFullscreenFlip(..., 2, ..., D3DKMT_PRESENT*)`, and DXGI builds the D3DKMT_PRESENT itself. The flip-model
  back-buffer rotation is done by `CRotateBackBufferIdentities<>::RotateBackBufferIdentities` @18001b680.
- d3d11 `NDXGI::CDevice::PresentImpl` @18004abe0 chooses between two calls, depending on the adapter's DXGI DDI
  version (`*(adapter+0x434) < 3`) or feature level 9:
  - Legacy: one call per surface to the device's DDI at `this+0x280`, inside a loop.
  - Otherwise: a single call to `this+0x300` with the whole surface array.

  Given the argument shapes, that is `pfnPresent` against `pfnPresent1` (DXGI1_6_1_DDI_ARG_PRESENT). This matches
  what was seen live: DWM goes through Present1. **Inferred:** the runtime's DDI copy is not laid out verbatim like
  `DXGI1_6_1_DDI_BASE_FUNCTIONS`, so the slot names cannot be proven from the offsets.
- The UMD's `pfnPresentCb` reaches d3d11 `NDXGI::CDevice::PresentCB` @18006f850. That function copies the 0x5d8-byte
  D3DKMT_PRESENT that DXGI prepared and fills in hContext, hSrcAllocation/hDstAllocation and the broadcast contexts
  from the DXGIDDICB_PRESENT. It then calls `D3DKMTPresent` through the adapter thunk at +0xe8.
  - In one case PresentCB calls D3DKMTPresent a second time and sets `Flags = (Flags & 0xffbfef3a) | 2`. That
    happens when `local_e40 == -1` and a second target (`local_e18`) exists. It is a follow-up present whose flags
    DXGI arranged. It is not an MPO present.
  - After the present, PresentCB also calls the MPO3 path (`this->vtbl+0x138`) if DXGI had staged a
    D3DKMT_PRESENT_MULTIPLANE_OVERLAY3 (`this+2000`).
- `NDXGI::CDevice::CheckDirectFlipSupport` @18007f070 checks DirectFlip support in two steps:
  1. `D3DKMTQueryAdapterInfo(type 0x13 = KMTQAITYPE_DIRECTFLIP_SUPPORT)`.
  2. Whether the UMD fills the device DDI at context-table +0xa60, which is `pfnCheckDirectFlipSupport`. If that slot
     is NULL, the answer is "not supported".

  A UMD that leaves this slot NULL is therefore never asked to DirectFlip an application surface.

### dxgkrnl side (the DMA-buffer half of a flip)
In `DXGCONTEXT::SubmitPresentWithDmaBuffer` @14031b804, `DxgkDdiPresent` is always called with an allocation
pointer. The first pass has `bVar31 = true`, and the argument depends on the context:
- **Context with GPU virtual addressing** (`this[0x1b2] != 0`):
  - `pAllocationInfo` points at a zeroed `DXGK_PRESENTALLOCATIONINFO[3]` (0x60 bytes).
  - Entry [1] (`DXGK_PRESENT_SOURCE_INDEX`) is the source: hDeviceSpecificAllocation and GPU VA. For a **Flip**
    (`Flags & 4`), VidMm callback +0xc0 also fills the source's PhysicalAddress and SegmentId.
  - Entry [2] (`DXGK_PRESENT_DESTINATION_INDEX`) is the destination, for non-flip presents only. If a non-flip
    destination has no VA, the present is refused: "Rejecting Present because the virtual address is not assigned
    to the destination allocation".
  - `pPatchLocationListOut = NULL` and `PatchLocationListOutSize = 0`.
  - I did not see `NumSrcAllocations`/`NumDstAllocations` being written on this path, so they can be 0 even though
    entries [1] and [2] are valid.
- **Context without GPU VA**: the argument is the VidMm DMA allocation list.

Two more rules on this path:
- A flip with no source allocation is refused: "Source allocation cannot be NULL for Flip present".
- A flip must not return STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER. dxgkrnl logs "Driver must not return
  STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER for Flip" and turns the result into STATUS_UNSUCCESSFUL.

So "no allocation list" on DWM's Flags=0xC flips most likely means the driver read `NumSrcAllocations`, which is 0, or
index 0. The source is at `pAllocationInfo[1]`. The flip itself is queued in VidSch and later programmed through
SetVidPnSourceAddress / the MPO DDI.

### dxgmms2 side (choosing which DDI programs the flip)
`VidSchiExecuteMmIoFlip` @14000f440 builds the argument and runs one of these DDIs under `DpSynchronizeExecution`,
chosen by the order of checks below:

```
if (VIDSCH_GLOBAL[0x9c])                           -> VidSchiExecuteMmIoFlipMultiPlaneOverlay3 (MPO3 DDI)
else if ((entry->Flags & 0x10) == 0)               -> VidSchiExecuteMmIoFlipAtISR   (DxgCoreInterface+0x100 -> SetVidPnSourceAddress)
else  /* multiplane entry */
    if (adapter[0xbe2]) ... -> ...AtISR  (MPO1, 0x70-byte plane records)
    else if (adapter[0xbe3]) -> ...AtISR2 (MPO2 DDI, DxgCoreInterface+0x1e8)
    else if (!adapter[0xbe4] || !adapter[+1000]) -> ...AtISR (MPO1 DDI, DxgCoreInterface+0x180 -> adapter+0x368)
    else -> ...AtISR2 (MPO2)
```

**Why a driver with no MPO support still gets MPO1.** `DXGADAPTER::Initialize` clears be2/be3/be4 and sets them only
when `adapter[0xb94]` (SupportMultiPlaneOverlay) is set. Without MPO they all stay 0, so a multiplane entry always
falls through to **MPO1: `DxgkDdiSetVidPnSourceAddressWithMultiPlaneOverlay`**. Nothing checks whether the pfn is
NULL.

**Where multiplane entries come from when the driver claims no MPO.** One source is
`VidSchiSubmitDisablePlanesFlip` @140004868. It builds a flip-queue entry with
`entry->Flags |= 0x10` (then `|0x190`), no allocation, and plane mask `((1<<planes)-1) << 10` ("layers to disable").
It then calls `VidSchiExecuteMmIoFlip` directly. Its only caller is `VidSchiSetFlipDevice` @140005e2c:

```
if (param_5 == 6 /*flip status: flush*/) { ...; if (param_7 /*disable planes*/) { st = VidSchiSubmitDisablePlanesFlip(dev, src);
    if (st == 0xC000022D && !present_info[0x13464]) BugCheck(0x119, 0xF001 ...) } }
```

The call chain that reaches it:
1. `VidSchFlushQueuePacketsInternal(..., bool disablePlanes)`
2. called by `VidSchFlushPresentReferencesAndDisableOverlays` @1400f46e0, which passes one disable bit per source
3. called by dxgkrnl `ADAPTER_RENDER::FlushPresentReferencesAndDisableOverlays` @140344430

The dxgkrnl function's callers are:
- `ADAPTER_DISPLAY::DisablePrimaryOnDevice` @140344024 (a device that owned the primary is destroyed, e.g. DWM exits
  or restarts)
- `ADAPTER_DISPLAY::DisablePrimaryAllocation` @140342e60
- `ADAPTER_DISPLAY::RemoveVidPnOwnership` @1403439d0
- `SetVidPnSourceVisibility` @140386440
- `VIDPN_MGR::SetTimingsFromVidPn`
- `DxgkCddDisable`

In `DisablePrimaryOnDevice` the gate is:

```
if (adapter+200 == 1) { if (!DXGADAPTER::IsLegacyDisplayStateSynchronization()) FlushPresentReferencesAndDisableOverlays(render, srcMask, 0, ownedMask, 0);
                        else FlushScheduler(render, 8, src, 0); }
```

`IsLegacyDisplayStateSynchronization` @14005829c:

```
return (adapter[0xac8] /*WDDMVersion*/ < 0x2300) && (adapter[0xc04]==0 || !UsingSetTimingsFromVidPn());
```

**So any driver reporting WDDM 2.3 or later gets a "disable all planes" flip, through the MPO1 DDI, whenever the
primary's owning device goes away, with or without MPO caps.** That fits the crash seen after DWM restarted.

**What the MPO DDIs must return.**
- MPO1/MPO2: any nonzero status is fatal. Right after the `DpSynchronizeExecution(...AtISR/AtISR2)` in
  `VidSchiExecuteMmIoFlip` the code does `if (local_80 != 0) { WdLogSingleEntry5(0,0x119,0xb); ... }`. That is exactly
  **bugcheck 0x119 / 0xB** (dec-mms-flip.c:623). The dxgkrnl wrapper also logs "Driver returned an invalid NTSTATUS"
  for anything other than 0.
- MPO3 (`ADAPTER_DISPLAY_DdiSetVidPnSourceAddressWithMultiPlaneOverlay3`) accepts 0, STATUS_DEVICE_BUSY (0x80000011)
  and STATUS_RETRY (0xC000022D).
- Plain SetVidPnSourceAddress accepts 0, STATUS_INVALID_PARAMETER and STATUS_NO_MEMORY. On STATUS_INVALID_PARAMETER
  VidSch logs "Driver failed SetVidPnSourceAddress" and fails that flip; it does not bugcheck.

**Which MPO generation a driver that does claim MPO gets** (`DXGADAPTER::Initialize`, dec-k-init.c ~780):

```
if (b94) { if (ddiVer < 0x300b) fail("...not compiled with expected header files");
  if (ddiVer < 0x4000) be2=1; else if (ddiVer==0x4000) be3=1;
  else { if (MaxOverlayPlanes==0) fail; if (MaxOverlayPlanes<9 && ddiVer>0x5000) be4=1; ... }
  if (!pfnMPO1 && !pfnMPO2 && !pfnMPO3) log; else if (!b93 /*DirectFlip*/) fail("SupportMultiPlaneOverlay cap but DirectFlip is not supported") }
```

`ReadConfig`:

```
c01 = pfnMPO3 && (WDDMVer>=0x2200 || (WDDMVer>=0x2100 && (caps&0x200 || regOverride)));
if (b94 && !c01 && !pfnMPO1 && !pfnMPO2) b94 = 0;
```

`SupportCheckMultiPlaneOverlaySupport3 = pfn(+0x400) && c01`. `SupportGetMultiPlaneOverlayCaps = pfn(+0x4c8) && c01`.
`GetMultiPlaneOverlayCaps` reports MaxPlanes=1 unless `IsMultiPlaneOverlaySupported()` (b94) and the caps DDI both
hold. **Inferred:** VIDSCH_GLOBAL[0x9c] (MPO3 in dxgmms2) mirrors adapter[0xc01].

**Consequence for avgpu.** The driver registers only MPO1 (slot 81, `src/miniport/driver/miniport.cpp:91`). The MPO1
DDI must:
- always return STATUS_SUCCESS;
- treat a disabled plane (or PlaneCount 0) as "turn the plane off / keep the last frame";
- complete the flip as described in section 5.

---

## 2. DirectFlip / independent flip against composition: what a driver must report

- **DirectFlip of an application swapchain:**
  - Runtime: `CheckDirectFlipSupport` above needs KMTQAITYPE_DIRECTFLIP_SUPPORT to be true and the UMD
    `pfnCheckDirectFlipSupport` to be filled, and that UMD function has to answer yes for the resource pair.
  - Kernel: dxgkrnl has `PinDirectFlipResources`, `ValidateDirectFlipResource`,
    `InsertDirectFlipAllocationToRequestedPinnedList` and the related functions.
  - A failed SetVidPnSourceAddress to a DirectFlip resource returns 0xC000000D, with the triage message "cannot flip
    to DirectFlip resource" (`VidSchiExecuteMmIoFlip`).
  - SupportDirectFlip=0 plus no `pfnCheckDirectFlipSupport` keeps every application window composed. DWM then only
    flips its own swapchain buffers, which are primaries.
- **Independent flip (iFlip):** `DXGADAPTER::ReadConfig` sets adapter[0xbff] only when FlipCaps (`adapter+0x9b4`)
  bit 0x10 = **FlipIndependent** is set and the WDDM version is 2.0 or later. With FlipIndependent clear the
  iFlip path stays off. **Inferred** from the gate: I did not trace the functions it enables
  (`VidSchEnterIndependentFlip`, `CheckIndependentFlipAttributes`).
- **DWM's own flip needs:**
  - FlipCaps.FlipOnVSyncMmIo. basicrender sets FlipCaps |= 2; it is also what avgpu reports.
  - A primary (SharedPrimary / DWM swapchain buffer) that is flippable.
  - SetVidPnSourceAddress, plus the vsync completion in section 5.

  MPO caps are not needed. `VidSchiFlipOverwriteEligible` = `presentInfo->state != 3 && (entry->Flags & 0x180) == 0x80`
  only affects whether a queued flip can be overwritten.
- **CheckMultiPlaneOverlaySupport* / GetMultiPlaneOverlayCaps** are only called when b94 and c01 are set. A driver
  without MPO is never asked, and DXGI's `GetMultiplaneOverlayCaps` sees MaxPlanes=1.

---

## 3. A render+display driver whose framebuffer is not GPU-visible, and what basicrender does

- **basicrender.sys (WARP KMD) is render-only.**
  - `WarpKMQueryChildRelations` @140006580 is a 6-byte function that returns success with no children.
  - It has no SetVidPnSourceAddress, no VidPn DDIs and no MPO DDIs.
  - Scan-out belongs to basicdisplay.sys, a KMDOD that is presented with `DxgkDdiPresentDisplayOnly`. dxgkrnl runs
    that path through `ADAPTER_DISPLAY::PresentDisplayOnly` @1403a4a44 / `DdiPresentDisplayOnly` @1403a4db0 (a
    system-memory copy).
  - So WARP never flips. `WARPKMCONTEXT::Present` @140003ddc does `if (Flags & 4 /*Flip*/) return STATUS_INVALID_PARAMETER;`.
- **What WarpKMPresent @140003d50 accepts.**
  - It refuses a present that has `pPrivateDriverData`/`PrivateDriverDataSize`
    (`if (+0xa0 != 0 || +0x98 != 0) return 0xC00000F0`).
  - Otherwise it builds a `WARPGPUCMDPRESENT` in the DMA buffer. `WARPGPUCMDPRESENT::Run` @140004770 does a CPU
    `WARPKMALLOCATION::Blt` (row memcpy via `RtlCopyToUserFromUser`) between the two allocations' CPU mappings, or
    between UM pointers after `KeAttachProcess`.
  - Its DMA buffer carries Blt/ColorFill only. The worker thread does the copy.
- **WARP DRIVERCAPS** (`WarpKMQueryAdapterInfo` @140001b20, type 1):

  | field | value |
  |---|---|
  | HighestAcceptableAddress | -1 |
  | FlipCaps | \|= 2 (FlipOnVSyncMmIo) |
  | SchedulingCaps | \|= 0x15 |
  | PresentationCaps | `(x & 0xfff6fffb) \| 0x1006c003`: NoScreenToScreenBlt, NoOverlapScreenBlt, texture size shifts, **SupportSoftwareDeviceBitmaps (bit 28)**, with SupportKernelModeCommandBuffer cleared |
  | MemoryManagementCaps | \|= 0x8010 (CrossAdapterResource plus bit 15), and \|= 0xa8 (SectionBackedPrimary, VirtualAddressingSupported, IoMmu) when configured |
  | SupportDirectFlip, SupportMultiPlaneOverlay | never set |

- **WARP standard allocations** (`WarpKMGetStandardAllocationDriverData` @140002c10). One 0x60-byte private blob
  format serves both the UMD and standard allocations. When the pointer is NULL the function only returns the size
  (0x60).
  - SHAREDPRIMARY (1) gets flags 1. `WARPKMALLOCATION::Initialize` @1400027a0 then creates two flip KEVENTs for it
    (`OpenPrimaryFlipEvents`).
  - SHADOW (2) gets flags 2.
  - STAGING (3) gets flags 4 and B8G8R8A8.
  - GDISURFACE (4):
    - types 7/8 (TEXTURE_CROSSADAPTER / CPUVISIBLE_CROSSADAPTER) get flags 0xa08 and a 128-byte-aligned pitch;
    - types 1/6 get even-rounded dimensions and 16-byte-aligned size;
    - type 5 (EXISTINGSYSMEM) gets \|0x20.

  Every WARP allocation is system memory. `WarpKMOpenAllocation` @1400021c0 just wraps the handles.
- **dxgkrnl does not CPU-copy on behalf of a full WDDM driver.** PresentDisplayOnly is used only for KMDOD adapters.
  The CDD primary path (`PresentFromCdd`, `PresentCddPrimary`, `DxgkCddPresent`) sends GDI/CDD blts to the
  primary through the driver's own DxgkDdiPresent.
- **What this means for avgpu.**
  - DWM's presents arrive as flips of its own swapchain buffers: DxgkDdiPresent with Flags=Flip, then
    SetVidPnSourceAddress with that allocation's segment address.
  - Nobody copies pixels for the driver. The picture the host shows has to follow `SetVidPnSourceAddress`'s
    `hAllocation`.
  - The cheapest correct design is to key the host's scan-out on the flipped allocation's identity (host image
    bound to that KMD allocation) at SetVidPnSourceAddress. The alternative is to keep the UMD's readback into that
    allocation.

  This is design advice drawn from the evidence above. No Microsoft binary does it this way.

---

## 4. GDI windows, redirection surfaces, and the OpenResource the UMD cannot handle

- **GDI surfaces are created by CDD (cdd.dll, for win32k) through dxgkrnl, not by any UMD.** The entry point is
  `DxgkCddCreateAllocation(... D3DKMDT_GDISURFACETYPE type ...)` @1403d92e0, which goes through
  `DxgkpExpandStackAndCreateStandardAllocation` to `DXGDEVICE::CreateStandardAllocation` @1402edf4c. The branch it
  takes depends on the render adapter's PresentationCaps (`adapter+0x9ac`):

  ```
  if (!(PresentationCaps & 4 /*SupportKernelModeCommandBuffer*/) && (!(PresentationCaps>>28 & 1 /*SupportSoftwareDeviceBitmaps*/) || type not in {6,8}))
       -> legacy path (types 1/3/6 and 2 created as a staging-style standard allocation)
  else -> D3DKMDT_STANDARDALLOCATION_GDISURFACE (4) with D3DKMDT_GDISURFACEDATA{W,H,Fmt,Type};
          type 7/8 (TEXTURE_CROSSADAPTER / TEXTURE_CPUVISIBLE_CROSSADAPTER) get create flags |0x21 and must come back
          with a global share handle ("NULL != CreateStandardAlloc.hGlobalShare")
  ```

  **Inferred:** the exact internal flag bits. The caps gate and the type dispatch are verified.
- **CreateStandardAllocation's contract with the KMD:**
  - It calls `DdiGetStandardAllocationDriverData` twice: once for sizes, where both being 0 is an error, and once
    for data.
  - For cross-adapter types it checks the pitch the driver returned:
    `pitch == align(width*bpp, 128)` unless adapter flag 0x1bc&8 is set. Otherwise it fails with "Driver returned
    invalid pitch ... for the cross adapter allocation".
  - It then calls `CreateAllocation`, so the KMD's DxgkDdiCreateAllocation receives the blob it produced itself.
  - Afterwards it requires a **CPU-visible** allocation for SHAREDPRIMARY, SHADOW, STAGING and GDISURFACE type 2
    (STAGING_CPUVISIBLE). Otherwise: "Driver did not create CPUVisible allocation for standard allocation type".
    The one exception is SHAREDPRIMARY when MemoryManagementCaps.NonCpuVisiblePrimary (`0x9bc & 0x200`) is set.
- **What DWM does with these surfaces.** DWM opens the shared GDI surfaces (redirection bitmaps, cross-adapter
  textures) through d3d11: `OpenSharedResource`, `QueryAndOpenSharedResource`, then `D3DKMTOpenResource(2)`, then
  the UMD's `pfnOpenResource`. The UMD therefore gets a D3D10DDIARG_OPENRESOURCE for a resource no UMD created:
  - Its per-allocation private data is the blob **the KMD's DxgkDdiGetStandardAllocationDriverData** wrote.
  - Its resource private data is whatever that DDI returned for `pResourcePrivateDriverData` (possibly size 0).

  WARP handles this by using one blob format for both producers (0x60 bytes, the same struct its UMD writes).
  **Consequence:** avumd's OpenResource must accept the KMD's standard-allocation blob (format, size, pitch,
  standard type, GDI surface type) and map it to a host resource. Otherwise GDI windows never reach DWM.
  **Not established:** which GDI surface types cdd.dll actually requests for a given caps set (cdd.dll is not in
  `re/bin`), and the exact D3D10DDIARG_OPENRESOURCE contents for these opens (d3d11's
  `OpenSharedResourceInternalImpl` was not decompiled).

---

## 5. Flip completion: what the vsync must carry

**The DPC walks the flip queue newest to oldest.** `VidSchiProcessDpcVSyncCookie` @14003301c:
- It walks from the newest entry backwards (index `q+0x48` downward).
- Entries in state set 0x1241 are skipped.
- Only entries in state 5 or 0xF (programmed, waiting for vsync) are tested with `VidSchiIsExpectedVSyncCookie`. The
  first match is completed through `VidSchiProcessVsyncCompletedFlipEntry`, which retires it and everything older,
  and then `VidSchiExecuteNextFlipQueueEntry` runs.
- Any other state ends the walk.

**`VidSchiIsExpectedVSyncCookie` @14003a0d4** (verified in disassembly):
- **DXGK_INTERRUPT_CRTC_VSYNC (3)** on a single-plane entry matches only if `cookie.PhysicalAddress` (lo/hi) equals
  the address programmed for that flip, i.e. the PrimaryAddress given to SetVidPnSourceAddress.
- **Type 3 on a multiplane entry (`Flags & 0x10`) never matches.** At `14003a1ec: CMP EDX,3 / JNZ; TEST EAX(entry&0x10),EAX; JNZ -> XOR EAX,EAX`.
  So a disable-planes flip, or any flip issued through the MPO1 DDI, **can never complete while the driver only
  reports CRTC_VSYNC**. The flip queue stalls there.
- **Type 7 (CRTC_VSYNC_WITH_MULTIPLANE_OVERLAY) or 0x12** on a multiplane entry: the cookie's enabled-layer mask
  (built in `VidSchiProcessIsrVSync` @140030b04 from each reported layer with Enabled != 0; the count must be under 10)
  must satisfy:

  ```
  (entryDisableMask & cookieMask) == 0 && (entryEnableMask & cookieMask) == entryEnableMask
  ```

  and then each enabled layer's address and rects must match.
  - For the disable-planes entry (enable mask 0), **a type-7 interrupt with MultiPlaneOverlayVSyncInfoCount = 0, or
    with layer 0 reported as Enabled = FALSE, completes it.**
  - For ordinary single-plane entries, type 7 matches only if the mask is exactly layer 0 and layer 0's address
    equals the flipped address.
- So the driver needs to remember the kind of the last programmed flip:
  - After SetVidPnSourceAddress: CRTC_VSYNC with that address.
  - After an MPO1 call that disables the plane: CRTC_VSYNC_WITH_MULTIPLANE_OVERLAY with 0 enabled layers.
  - After an MPO1 call with plane 0 enabled: type 7 with layer 0 {Enabled, address}.
- **Not verified:**
  - that dxgkrnl's DxgkCbNotifyInterrupt accepts type 7 from an adapter without MPO caps;
  - the exact per-layer record layout the ISR reads when be2 = be3 = 0. It reads 0x68-byte records. Reporting
    count 0 avoids the layout question.

**Early completion.** A vsync that reports an address no programmed entry holds matches nothing and completes
nothing, so reporting "early" cannot retire a flip that has not been programmed. Reporting the old address
indefinitely leaves the new entry pending, which ends in the flip-queue timeout (`VidSchiCheckFlipQueueTimeout` @14002b410
calls `VidSchiCheckHeadTimeout`). **Not established:** what that timeout escalates to (a TDR or a 0x119 subcode).

**Two bugchecks seen along the way:**
- 0x119/0xF001 in `VidSchiSetFlipDevice` if the disable-planes flip returns STATUS_RETRY while
  `present_info[0x13464]` is clear.
- 0x119/0xE in `VidSchiProcessIsrVSync` when a reported layer index is at or above the adapter's plane count
  (`>= VIDSCH_GLOBAL+0x98`, in the type-10 path), and for other malformed cookies.

---

## 6. DWM's DisplayCore path and the MPO3 DDI

Build 26100.9444 (udwm, dwmcore, ddisplay, d3d11, dxgkrnl, dxgmms2, all with their PDBs).

**Which path DWM takes.** `CDWMDisplay::IsDDisplayCompatible` (udwm.dll) drives a display through
DisplayCore (ddisplay.dll) instead of a DXGI swap chain when the driver is WDDM 3.2 or later and its vendor is
not AMD (or is AMD with a driver major version of 32 or more). `HKLM\SOFTWARE\Microsoft\Windows\Dwm
DisableDDisplay=1` turns that off. avgpu reports the host GPU's vendor, so an Intel or NVIDIA host gets
DisplayCore and an AMD one does not.

**Why DisplayCore failed on avgpu.** `CDDisplaySwapChain::Present`, the legacy present, is
`mov eax, E_NOTIMPL; ret`. `COverlayContext::LegacyPresentRequired` (dwmcore.dll @1801d741c) asks for it when the
overlay context has no planes and anything forces composition (a Windows.Graphics.Capture session does), unless
the driver version is at least 2200 and bit 0x40000 of the overlay caps is set. DWM then fail-fasts in
`COverlayContext::Present`.

**Where bit 0x40000 comes from.** `CDDisplaySwapChain::GetMultiplaneOverlayCaps` sets it from
`IsComplexScanoutSubFeatureSupported(2)` on the DisplayCore device. `CDDisplayDevice::IsComplexScanoutSubFeatureSupported`
answers features 0 to 3 when `CDDisplayAdapterInfo::SupportsMpo3Ddi()` is true. That is `SAdapterDesc+0x20`,
which `SAdapterDesc::SAdapterDesc` fills from `D3DKMTQueryAdapterInfo(KMTQAITYPE_MPO3DDI_SUPPORT = 43)`.

**What dxgkrnl answers.** `DxgkQueryAdapterInfoImpl` answers type 43 with `adapter[0xc01]`, which
`DXGADAPTER::ReadConfig` sets when `DxgkDdiSetVidPnSourceAddressWithMultiPlaneOverlay3` is registered and the
driver is WDDM 2.2 or later (section 1). `SupportMultiPlaneOverlay`, `MaxOverlayPlanes` and the caps and check
DDIs are not needed. Without `SupportMultiPlaneOverlay`, dxgkrnl reports one plane.

**What the driver must then do.**
- dxgmms2 flips every primary through the MPO3 DDI, DWM's swap chain too. The DDI accepts only success, busy and
  retry.
- A flip made through it retires only on `CRTC_VSYNC_WITH_MULTIPLANE_OVERLAY2` (10).
  `VidSchiIsExpectedVSyncCookie` compares, for each layer of the flip, the PresentId the flip gave it with the one
  the vsync reports for that layer. A stored PresentId of 0 matches anything.
- DisplayCore presents through `IDXGIDeviceInternal3::PresentMultiplaneOverlay` (d3d11.dll,
  `NDXGI::CDevice::PresentMultiplaneOverlay`). That calls the UMD's `pfnPresentMultiplaneOverlay1` from DXGI
  DDI 4 when the UMD fills it, `pfnPresentMultiplaneOverlay` otherwise.
- The UMD answers through `pfnPresentMultiplaneOverlay1Cb`, one `DXGIDDI_MULTIPLANE_OVERLAY_PLANE_INFO` per plane,
  disabled ones included. `PresentMultiplaneOverlay1CBImpl` refuses a count that differs from the planes it built
  and reads every entry.
- A driver that ships its own display UMD (`GetDisplayUmdFilename`) is presented through `DisplayUmdDevice`
  instead. avgpu does not ship one.
