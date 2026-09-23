# Prior art: how paravirtual WDDM render drivers keep the displayed image on the host

Research for accel-virt (WDDM KMD + D3D11 UMD serializing to a QEMU device that replays on DXVK).
Goal: DWM and D3D11/12 apps display without the per-frame "host texture -> guest allocation -> TRANSFER_TO_HOST_2D" round trip.

Sources read (pinned revisions):

- VirtualBox, GitHub mirror `VirtualBox/virtualbox` @ `51b09acfb1905a9142e5ff1e1a64757b9250dcb1` (2026-09-22).
  Base: `https://github.com/VirtualBox/virtualbox/blob/51b09acfb1905a9142e5ff1e1a64757b9250dcb1/`
  NOTE: in the current tree the Windows additions live under `src/VBox/Additions/win/...`, not `WINNT/`.
- Mesa @ `5263e0674c61d7eb8cd92046e6a3a8546f9f246c` (2026-09-23).
  Base: `https://gitlab.freedesktop.org/mesa/mesa/-/blob/5263e0674c61d7eb8cd92046e6a3a8546f9f246c/`
- Mesa MR !24223 "virgl/win: Virtio GPU 3D acceleration for windows" (draft, head `62f1e49a`), https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/24223
- viogpu3d KMD, `max8rr8/kvm-guest-drivers-windows` branch `viogpu3d` @ `9ed3aab11fb46e55dc835ff008008623b290a6cf` (2025-09-05); upstream PR https://github.com/virtio-win/kvm-guest-drivers-windows/pull/943
  Base: `https://github.com/max8rr8/kvm-guest-drivers-windows/blob/9ed3aab11fb46e55dc835ff008008623b290a6cf/viogpu/viogpu3d/`
- Linux vmwgfx `drivers/gpu/drm/vmwgfx/vmwgfx_stdu.c` (torvalds/linux master, fetched 2026-09-23).

Paths below are relative to those bases; `mp` = `src/VBox/Additions/win/Graphics/Video/mp/wddm`, `dx` = `src/VBox/Additions/win/Graphics/Video/disp/wddm/dx`, `dev` = `src/VBox/Devices/Graphics`.

---

## 1. VirtualBox VMSVGA WDDM (VBoxWddm.sys + VBoxDX.dll D3D11 UMD, "VBOX_WITH_VMSVGA3D_DX")

This is the most relevant prior art: a D3D11 UMD that serializes API-level commands (SVGA3D DX / "VGPU10") to a host that replays them on the host GPU (D3D11 on Windows hosts, DXVK on Linux hosts) - structurally the same as accel-virt.

### Where the image lives

- Every D3D11 resource is a **host surface** (`sid`) created by the KMD: `svgaCreateSurfaceForAllocation` emits `SVGA_3D_CMD_DEFINE_GB_SURFACE_V4` (`mp/gallium/VBoxMPDX.cpp:114-160`). `sid`s are device-global, allocated in the KMD (`SvgaSurfaceIdAlloc`).
- Placement by usage (`svgaCreateAllocationSurface`, `mp/gallium/VBoxMPDX.cpp:222-323`):
  - USAGE_DEFAULT/IMMUTABLE -> segment 3 "Host" (not CPU-visible). Still gets a guest backing MOB (a GBO) only for eviction (`:295-318`, "Allocations in the host VRAM still need guest backing").
  - DYNAMIC and STAGING -> segment 2 (aperture, CPU-visible, MOB-backed; renaming list for DISCARD).
  - **D3D primaries** (`fPrimary`, i.e. DXGI fullscreen/flip primaries) -> segment 1 (CPU-visible VRAM) "because VidPn code currently assumes that they are there" (`:239-248`), plus a host surface bound to a MOB.
- The UMD tags `D3D10_DDI_BIND_PRESENT` resources with `SVGA3D_SURFACE_SCREENTARGET | BIND_RENDER_TARGET` so the host may use them as blit targets (`dx/VBoxDX.cpp:1208-1210`). Swapchain buffers therefore are ordinary host surfaces.
- The desktop (GDI/DXGK standard allocations) primary is `D3DKMDT_STANDARDALLOCATION_SHAREDPRIMARYSURFACE` in VRAM segment 1 (`mp/VBoxMPWddm.cpp:2652-2675`).
- The *screen* itself is a legacy SVGA **screen object** whose backing is a VRAM offset: `GaVidPnSourceReport` -> `SvgaScreenDefine(offVRAM, ...)` -> `SVGA_CMD_DEFINE_SCREEN` (`mp/gallium/VBoxMPGaWddm.cpp:2365-2388`, `mp/gallium/Svga.cpp:569-597`). The KMD has wrappers for GB screen targets (`Svga3dDefineGBScreenTarget`/`Svga3dBindGBScreenTarget`, `Svga.cpp:622-696`) but **nothing calls them** (grep finds only the definitions) - VBox does not bind a guest surface as the scanout.

### DxgkDdiPresent (`SvgaDxgkDdiPresent`, `mp/gallium/VBoxMPGaWddm.cpp:1007-1061`)

Present only *emits commands into the DMA buffer*; they execute on the host in submission order on the single node.

- **Blt** (`gaPresentBlt`, `:577-908`), by (src, dst) type:
  - shadow/staging (CPU-drawn GDI surface in VRAM) -> SHAREDPRIMARY: `SVGA_CMD_DEFINE_GMRFB` pointing at the shadow's VRAM offset (patched via patch list) + `SVGA_CMD_BLIT_GMRFB_TO_SCREEN` per sub-rect (`:606-658`).
  - D3D surface -> SHAREDPRIMARY: `SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN(sid, screenId, rects)` (`:659-691`).
  - SHAREDPRIMARY -> shadow: `BLIT_SCREEN_TO_GMRFB`; D3D surface -> shadow: `SVGA_3D_CMD_SURFACE_DMA` READ_HOST_VRAM into the shadow's VRAM (`:698-770`).
  - D3D -> D3D (DWM redirection / windowed DXGI blt-model presents; staging surfaces): `SVGA_3D_CMD_DX_PRESENTBLT(srcSid, dstSid, boxes)` entirely on the host; **only if** the destination is `SVGA3D_SURFACE_HINT_RT_LOCKABLE` (i.e. the DXGK staging surface GDI will read) is a `SURFACE_DMA` readback to guest VRAM appended (`:773-880`).
- **Flip** (`svgaPresentFlip`, `:929-993`): src must be a D3D primary; emits one `SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN(sid, full rect -> screen VidPnSourceId, clip = pDstSubRects)`. No guest-side copy.
- **ColorFill**: not implemented, logs and returns success (`:996-1004`).
- The UMD side is thin: `dxgiPresent` flushes and calls `pfnPresentCb(hSrc, hDst, pDXGIContext)` (`dx/VBoxDXDDI.cpp:4034-4060`); `dxgiBlt` emits `DX_PRESENTBLT` itself in the UMD command stream with sid patch locations (`dx/VBoxDXDDI.cpp:4183-4196`, `dx/VBoxDX.cpp:4521-4545`, `dx/VBoxDXCmd.cpp:1828-1858`); `dxgiSetDisplayMode` -> `pfnSetDisplayModeCb` (`dx/VBoxDXDDI.cpp:4078-4095`); `RotateResourceIdentities` swaps `pKMResource` pointers and redefines views - no copy (`dx/VBoxDX.cpp:4086-4200`).

### How a flip reaches the host screen

- Guest: `DxgkDdiSetVidPnSourceAddress` records the new primary and re-issues `DEFINE_SCREEN` with its VRAM offset (`mp/VBoxMPWddm.cpp:4163-4250`, VMSVGA branch at `:4236-4246`). Offsets not in segment 1 are ignored (`vboxWddmAddrVRAMOffset`, `mp/gallium/VBoxMPGaWddm.cpp:2212-2217`). The pixels arrive via the Flip's `BLIT_SURFACE_TO_SCREEN`, not via the address change.
- Host, `SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN` -> `vmsvga3dSurfaceBlitToScreen` (`dev/DevVGA-SVGA-cmd.cpp:5728-5744`, `dev/DevVGA-SVGA3d.cpp:1088-1188`):
  - Modern backend (`pScreen->pHwScreen`, DX11 backend): `pfnSurfaceBlitToScreen` = **GPU blit** of the surface into a per-screen host texture `pScreenTexture` (`dev/DevVGA-SVGA3d.cpp:1427-1445`, texture created in `vmsvga3dHwScreenCreate`, `dev/DevVGA-SVGA3d-dx-dx11.cpp:3508-3565`). Then an **asynchronous readback**: `dxStartScreenReadback` converts into "output targets" (BGRX or I420 for recording), copies to staging, `End(query)` and bumps a readback fence; `dxProcessPendingUpdates` polls the query and, when done, copies to the frontend's memory buffer and calls `vmsvgaR3UpdateScreen` (`dev/DevVGA-SVGA3d-dx-dx11.cpp:3975-4060`; design comment at top of `dev/DevVGA-SVGA3d-dx-dx11-output.cpp`: "Render -> Copy -> Query -> Readback").
  - Legacy backend: `vmsvga3dSurfaceDMA(READ_HOST_VRAM)` straight into the host-side VRAM buffer at the screen's offset, then `vmsvgaR3UpdateScreen` (`dev/DevVGA-SVGA3d.cpp:1111-1187`). Either way the copy is host-GPU -> host-process memory; the guest never touches the pixels.

### GDI content

- Classic GDI/desktop path: GDI draws with the CPU into SHADOWSURFACE (VRAM segment 1, CPU-visible); dxgkrnl issues Present(Blt shadow -> SHAREDPRIMARY) which becomes `BLIT_GMRFB_TO_SCREEN` from VRAM (host reads its own VRAM buffer - no DMA from guest RAM) (`mp/gallium/VBoxMPGaWddm.cpp:606-658`). VBox's "VRAM" is a PCI BAR backed by host memory, so the CPU write is the only copy.
- With DX, the DXGK **staging surface** (what GDI reads when the primary is not CPU-accessible) is created as a host surface with `RT_LOCKABLE` (`mp/VBoxMPWddm.cpp:2717-2770`), and presents into it add a `SURFACE_DMA` readback (above).
- GDI-in-DWM (redirection bitmaps for GDI windows): no VBox-specific code; it arrives at the driver as D3D resources/Blt presents via DWM's D3D device. Whether Windows uploads GDI window content with UpdateSubresourceUP, Map, or a staging Blt could **not be established** from VBox source.
- UMD Map of D3D resources: `pfnLockCb` on the MOB/aperture allocation; READ triggers `vgpu10ReadbackSubResource` first; Unmap after write emits `vgpu10UpdateSubResource` (`dx/VBoxDX.cpp:3284-3380`) - explicit upload/readback commands at map boundaries, not per frame.

### Shared / cross-process resources

- `sid` is device-global and owned by the KMD allocation, so a resource opened by another process (DWM opening an app's shared surface) is simply the same allocation: `ddi10OpenResource` -> `vboxDXOpenKMResource` rebuilds the descriptor from the allocation's private driver data (`dx/VBoxDX.cpp:1371-1381`, `:1110-1120`). The UMD writes `SVGA3D_INVALID_ID` and a patch location; `DxgkDdiDXPatch` writes `pAllocation->dx.sid` at submission (`mp/gallium/VBoxMPDX.cpp:1472-1507`). No host-side "import" step.
- The older Gallium (D3D9/Nine) path needed an explicit sid-redirection escape `VBOXESC_GASHAREDSID` -> `SvgaSharedSidInsert` (`mp/gallium/Svga.cpp:950-1011`, `mp/gallium/VBoxMPGaWddm.cpp:2082-2097`), because there each process created its own host surface.

### Fences and vsync

- Fences: one node (ordinal 0, shared by GDI and D3D contexts, `mp/VBoxMPWddm.cpp:1853-1866`). `GaDxgkDdiSubmitCommand` hands the DMA buffer to the device (command-buffer mode carries `SubmissionFenceId` in the header; otherwise appends `SVGA_CMD_FENCE`) (`mp/gallium/VBoxMPGaWddm.cpp:1477-1630`). The ISR reads `SVGA_REG_FENCE` and reports `DXGK_INTERRUPT_DMA_COMPLETED` (`:471-520`, `:1632-1679`). UMD-visible 64-bit fences use `SVGA_3D_CMD_DX_MOB_FENCE_64` writing into a miniport MOB (`mp/gallium/VBoxMPDX.cpp:760-793`).
- VSync: **synthetic**. A KTIMER at `1000/g_RefreshRate` ms fires a DPC that raises `DXGK_INTERRUPT_CRTC_VSYNC` for every connected target, with `CrtcVsync.PhysicalAddress` left zero; `GetScanLine` fakes a raster position from elapsed time (`mp/VBoxMPMisc.cpp:1403-1500`). Not tied to host presentation.

### DRIVERCAPS (`mp/VBoxMPWddm.cpp:1804-1876`)

`WDDMVersion = WDDMv1_2`; `FlipCaps = 0`, `MaxQueuedFlipOnVSync = 0`, `PresentationCaps.NoScreenToScreenBlt/NoOverlapScreenBlt`, `SchedulingCaps.MultiEngineAware`, one node, no paging node, no GPU VA. No `SupportDirectFlip`, no MPO/multiplane caps (grep finds none).

---

## 2. Mesa `d3d10umd` (the open "VMware-style" D3D10 UMD)

- Upstream it is a **software** UMD only ("Currently it only supports SW rasterizers", `src/gallium/frontends/d3d10umd/README.md`; target README `src/gallium/targets/d3d10umd/README.md` says it is a WARP-like DLL you can rename over `vm3dum_10.dll`). There is **no svga/WDDM winsys in Mesa** (`src/gallium/winsys/svga/` contains only `drm`). VMware's real hardware UMD (`vm3dum*.dll`) is closed; its Present path could not be examined.
- `_Present` flushes and calls `screen->flush_frontbuffer(resource, pDXGIContext)` (`src/gallium/frontends/d3d10umd/DxgiFns.cpp:55-68`); the target casts `pDXGIContext` to `D3DKMT_PRESENT*`, takes `hWindow`, and does `GetDC` + `StretchDIBits` (`src/gallium/targets/d3d10umd/d3d10_gdi.c:42-56`, `src/gallium/winsys/sw/gdi/gdi_sw_winsys.c:238-249`). Nothing goes to `pfnPresentCb`.
- `CreateDevice` returns **`DXGI_STATUS_NO_REDIRECTION`** "to not use the shared resource presentation path when communicating with DWM" (`src/gallium/frontends/d3d10umd/Device.cpp:297-304`) - it opts out of DWM redirection entirely.
- `_SetDisplayMode` and `_Blt` are unimplemented stubs (`DxgiFns.cpp:114-120`, `:256-261`); `_RotateResourceIdentities` copies contents through a temp resource (`:175-240`).
- Relevance: low as a model; useful only as the DXGI DDI skeleton.

### 2b. viogpu3d (virtio-gpu + virgl + Mesa d3d10umd on WDDM) - the closest open virtio-gpu analogue

- UMD (Mesa MR !24223): removes the `NO_REDIRECTION` return, adds a `gdikmt_d3dddi` layer whose `present` calls `pfnPresentCb(hSrcAllocation, hDst = NULL, pDXGIContext)` and whose `setdisplaymode` calls `pfnSetDisplayModeCb`; `RotateResourceIdentities` becomes a pointer rotation (MR diff of `DxgiFns.cpp`, `gdikmt_d3dddi.cpp:257-290`).
- KMD `VioGpuDevice::Present` (`viogpu_device.cpp:151-240`):
  - **Flip: returns success with no commands** (`:155-158`).
  - Blt: optional `VIOGPU_CMD_TRANSFER_TO_HOST` if the *source* is a "coherent" (guest-backed CPU) resource, then virgl `RESOURCE_COPY_REGION` per sub-rect on the host, then `TRANSFER_FROM_HOST` only if the *destination* is coherent (`GenerateBltPresent`, `:42-149`). Shadow and staging standard allocations are flagged `VIRGL_RESOURCE_FLAG_MAP_COHERENT`; the shared primary is not (`viogpu_allocation.cpp:164-225`), so D3D -> primary blits stay on the host.
  - ColorFill and other ops: NOP.
- Scanout: `SetVidPnSourceAddress` only latches `hAllocation` and sets a flag (`viogpu_vidpn.cpp:2012-2019`). A kernel thread ticking every 16.7 ms (`FlipThread`, `:1986-2003`) calls `Flip()`: if flagged, `FlushToScreen` = virtio-gpu `SET_SCANOUT(resource_id)` + `RESOURCE_FLUSH` of the **3D (virgl) resource itself** (`viogpu_allocation.cpp:115-133`); then always raises `DXGK_INTERRUPT_CRTC_VSYNC` with `PhysicalAddress = m_sourceAddress` (`viogpu_vidpn.cpp:1962-1984`). With QEMU's GL display (virgl/dma-buf), that is zero-copy scanout of the host texture.
- DRIVERCAPS (`viogpu_adapter.cpp:485-514`): `WDDMv1_3`, `FlipCaps.FlipOnVSyncMmIo`, `MaxQueuedFlipOnVSync = 1`, `SupportDirectFlip = 1`, `MemoryManagementCaps.SectionBackedPrimary`, `MultiEngineAware`, `PreemptionAware`; one aperture segment with `Flags.DirectFlip` (`:540-565`).
- Known gaps (PR text): no preemption, glitches; VBlank is a timer, not host feedback.

---

## 3. SVGA3D protocol concepts that matter here

- **Screen object** (legacy, `SVGA_CMD_DEFINE_SCREEN`): a monitor whose backing is a VRAM offset; `BLIT_GMRFB_TO_SCREEN` / `SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN` push pixels into it. VBox uses this model.
- **Screen target (STDU)**: `DEFINE_GB_SCREENTARGET(stid, w, h, x, y, flags, dpi)`, `BIND_GB_SCREENTARGET(stid, SVGA3dSurfaceImageId)`, `UPDATE_GB_SCREENTARGET(stid, rect)` (`src/gallium/drivers/svga/include/svga3d_cmd.h:1428-1474`, ids 1124-1127 at `:140-143`). The scanout *is* a host surface; a page flip is a re-bind, and a damage notification is UPDATE. Linux vmwgfx does exactly this: `vmw_stdu_primary_plane_atomic_update` binds the new framebuffer's surface to the screen target (`vmw_stdu_bind_st`) and then sends UPDATE for dirty rects; only when the FB size differs from the mode (`SEPARATE_SURFACE`) or the FB is a CPU buffer object (`SEPARATE_BO`) does it copy (`SURFACE_COPY` / `UPDATE_GB_IMAGE`) (`vmwgfx_stdu.c:57-61`, `:990-1010`, `:1395-1440`, `:256-280`).
- **`SVGA_3D_CMD_DX_PRESENTBLT`** (`svga3d_dx.h:696-710`): host-side stretch/format-converting blit between two surfaces with SRGB/XR-bias mode bits - the primitive both DWM-redirection Blt presents and UMD `pfnBlt` map onto.

---

## Per-design summary

| | VBox VMSVGA-DX | viogpu3d (virgl) | Mesa d3d10umd upstream | vmwgfx STDU (Linux, reference) |
|---|---|---|---|---|
| Displayed image lives | Host surface; scanout = host "screen texture" (DX11 backend) or host VRAM buffer | Host virgl resource set as scanout | Guest CPU memory (software) | Host surface bound as screen target |
| Guest copies per frame | None for D3D/DWM content | None for D3D content | Full (StretchDIBits) | None if FB matches mode |
| Host copies per frame | GPU blit surface -> screen texture + async GPU->CPU readback for the 2D frontend | None with GL display (QEMU scans out the texture); 2D display would read back | n/a | Host-internal |
| Flip -> host op | `BLIT_SURFACE_TO_SCREEN` emitted by Present(Flip) in the DMA stream; SetVidPnSourceAddress only re-defines screen | Present(Flip) empty; SetVidPnSourceAddress latches, timer thread does `SET_SCANOUT(res)` + `RESOURCE_FLUSH` | n/a | `BIND_GB_SCREENTARGET` + `UPDATE_GB_SCREENTARGET` |
| Blt (DWM/windowed) | `DX_PRESENTBLT` on host; readback only if dst is RT_LOCKABLE staging | virgl `RESOURCE_COPY_REGION`; transfers only for CPU-coherent src/dst | NO_REDIRECTION: DWM path disabled | n/a |
| GDI content | CPU-drawn shadow in BAR VRAM -> `BLIT_GMRFB_TO_SCREEN` | Coherent shadow: `TRANSFER_TO_HOST` of dirty cover rect then host copy | GDI itself | Dumb BO -> `UPDATE_GB_IMAGE`/DMA |
| Cross-process sharing | Device-global sid in the KMD allocation, patched at submit | Global virgl res id per allocation | n/a | dma-buf/prime |
| Fences | Submission fence in cmd-buffer header / `SVGA_CMD_FENCE`, IRQ -> DMA_COMPLETED | virtio completion | n/a | vmw fences |
| VSync | Timer DPC, `PhysicalAddress` = 0 | 60 Hz kernel thread, reports latched address | n/a | host-driven |
| Flip caps | none (`FlipCaps = 0`, no DirectFlip/MPO) | `FlipOnVSyncMmIo`, `MaxQueuedFlipOnVSync=1`, `SupportDirectFlip` | n/a | n/a |

---

## What transfers to accel-virt

1. **Present(Flip) should emit a host command naming the source resource, never pixels.** Both open drivers do this (VBox: `BLIT_SURFACE_TO_SCREEN` in the Present DMA buffer; viogpu3d: `SET_SCANOUT(resource)` at SetVidPnSourceAddress). For us: Present(Flip) (or SetVidPnSourceAddress) -> a wire command "scan out host image X for source N", resolved via the allocation's host handle. The guest allocation for a D3D primary needs no CPU-visible content.
2. **Put the "which image is on screen" state in the host device, and let the QEMU display pull from it.** VBox's split is the useful one for a *plain 2D* QEMU console: host GPU blit into a per-screen host texture, then an async, query-fenced readback into the console surface, only for dirty rects (`dxStartScreenReadback`/`dxProcessPendingUpdates`). For a GL/dma-buf capable QEMU display, do what viogpu3d/vmwgfx STDU do: export the Vulkan image (or a per-screen copy of it) and hand it to the console as the scanout (zero-copy). The same wire command serves both; only the device-side backend differs.
3. **Blt presents (DWM redirection, windowed blt-model swapchains, `pfnBlt`) should be host-to-host copies** (`DX_PRESENTBLT` / `RESOURCE_COPY_REGION` analogue), adding a readback only when the destination is a CPU-accessible surface GDI will read (VBox's `RT_LOCKABLE` staging rule; viogpu3d's "coherent" rule). This is the rule that removes our current per-frame readback.
4. **GDI stays a CPU path, but with the upload bounded to dirty rects of shadow/staging surfaces** (VBox: `BLIT_GMRFB_TO_SCREEN` from shadow; viogpu3d: `TRANSFER_TO_HOST` of the sub-rect cover before the copy). Our TRANSFER_TO_HOST_2D is appropriate only for these standard allocations, not for D3D primaries.
5. **Cross-process sharing via a device-global host handle stored in the KMD allocation** and written into command streams at submit (VBox `DxgkDdiDXPatch`). DWM opening an app's surface then needs no host-side import. VBox's older per-process-surface design needed a redirection escape; avoid that.
6. **VSync**: both open drivers fake it with a timer. viogpu3d reports the latched `PhysicalAddress` in `CRTC_VSYNC` (dxgkrnl uses this to retire flips); VBox leaves it zero and reports no flip caps. If we report `FlipOnVSync*`/`MaxQueuedFlipOnVSync`, we must report the scanned-out address, ideally from a host "presented" completion rather than a free-running timer.
7. **Caps**: neither open driver reports MPO; VBox reports no flip caps at all and still runs DWM. viogpu3d reports `SupportDirectFlip` + a DirectFlip segment, which lets DWM flip app swapchain buffers directly (the "independent flip" path) - worth copying only once flip = rebind scanout is in place.
8. **Avoid Mesa d3d10umd's `DXGI_STATUS_NO_REDIRECTION`** approach; it forces GDI BitBlt presentation of every D3D frame.

## Not established

- VMware's closed `vm3dum`/`vm3dmp` behaviour (whether they use screen targets for D3D flips on Windows).
- How Windows 8+ delivers GDI window contents into DWM redirection surfaces on these drivers (no driver-specific code found in VBox).
- Whether VBox's `vmsvgaR3UpdateScreen` consumers (VRDE, recording, GUI) can take a GPU texture directly; the code shown always ends in a memory buffer.
