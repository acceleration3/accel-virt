# Prior art: Direct3D 9 user-mode drivers that map the D3D9 DDI onto a D3D9-API backend

Research for accel-virt's native D3D9 UMD in avumd.dll: `D3DDDI_DEVICEFUNCS` (third_party/windows-kit/include/um/d3dumddi.h)
translated into IDirect3DDevice9Ex-level commands that DXVK replays on the host. Target: Portal (Source engine, 32-bit, windowed, SM2/SM3).

Sources read (pinned revisions):

- VirtualBox, GitHub mirror `VirtualBox/virtualbox` @ `51b09acfb1905a9142e5ff1e1a64757b9250dcb1` (2026-09-22), the same revision as `docs/research/d3d-desktop/prior-art.md`.
  Base: `https://github.com/VirtualBox/virtualbox/blob/51b09acfb1905a9142e5ff1e1a64757b9250dcb1/`
  This is the current D3D9 UMD (`VBoxDispD3D.dll`, "Gallium" path): every DDI call is forwarded to an in-process `IDirect3DDevice9Ex` implemented by Mesa's Gallium Nine state tracker over the VMSVGA winsys.
- VirtualBox tag `v6.0.0` @ `e9bf15e085c1bbaf2c730341d90c862ad021fdc8` (2018-12-18).
  Base: `https://github.com/VirtualBox/virtualbox/blob/e9bf15e085c1bbaf2c730341d90c862ad021fdc8/`
  The older path that forwarded the same DDI to Wine's d3d9 (`VBoxWineEx`, Chromium host); same DDI shape, more comments about runtime quirks and a swapchain-reconstruction layer.
- VirtualBox's bundled Mesa 24.0.2 Nine at the first revision, for the VBox patches the UMD relies on.
- Microsoft D3D9On12 (`microsoft/D3D9On12`) @ `f3352d12acb286e09c0b73d44a1fe2dfbb05065e` (2026-09-30), MIT.
  Base: `https://github.com/microsoft/D3D9On12/blob/f3352d12acb286e09c0b73d44a1fe2dfbb05065e/`
  Microsoft's own D3D9 UMD: implements the whole `D3DDDI_DEVICEFUNCS` table on D3D12. It is not API-on-API, but it is the authoritative open reading of what each DDI argument means.
- DXVK v3.1.1 (`third_party/dxvk` submodule @ `b1a1c99ab52b687cf950d62c88bc2fa316b41663`), only to check how the host backend reacts to what VBox forwards.

Abbreviations, relative to the bases above:

| | path |
|---|---|
| `gw` | `src/VBox/Additions/win/Graphics/Video/disp/wddm` @ 51b09acf |
| `ga` | `gw/gallium` |
| `nine` | `src/VBox/Additions/3D/mesa/mesa-24.0.2/src/gallium/frontends/nine` @ 51b09acf |
| `w6` | `src/VBox/Additions/WINNT/Graphics/Video/disp/wddm` @ v6.0.0 |
| `9on12` | D3D9On12 root @ f3352d12 |
| `dxvk` | `third_party/dxvk/src/d3d9` @ b1a1c99 |
| `d3dumddi.h`, `sdk-shared/d3dukmdt.h` | this repository's `third_party/windows-kit/include/um/d3dumddi.h` and `third_party/windows-kit/include/sdk-shared/d3dukmdt.h` |

Mesa's d3d10umd (structure only, covered in `docs/research/d3d-desktop/prior-art.md` §2), ReactOS (its d3d9 is a runtime over the DirectDraw HAL, not a WDDM UMD) and Wine (an API implementation with no DDI layer) have no D3D9 DDI code to read; they are not cited.

---

## 0. Shape of the VirtualBox D3D9 UMD

- `OpenAdapter` queries the KMD (`pfnQueryAdapterInfoCb`), refuses to run 3D when loaded by ddraw.dll (Interface 7 with ddraw-owned callbacks), and for VMSVGA hands back `GaDdiAdapterGetCaps/CreateDevice/CloseAdapter` with `DriverVersion = D3D_UMD_INTERFACE_VERSION_WIN7` (or `_VISTA` on Vista) (`gw/VBoxDispD3D.cpp:48-77`, `:105-125`, `:127-191`; v6.0 always reported `_VISTA`, `w6/VBoxDispD3D.cpp:6707`). Neither revision reaches WIN8, so `pfnCreateResource2`, Blt `Discard/NoOverwrite` and `TexBlt1` never arrive.
- The adapter-level backend (`IDirect3D9Ex` from Nine) is opened once per process under a global lock and reference-counted (`gw/VBoxDispD3DIf.cpp:493-554`).
- `CreateDevice` only records callbacks and fills the function table; the backend `IDirect3DDevice9Ex` is created lazily on first use by `VBOXDISP_D3DEV()` (`gw/VBoxD3DIf.h:141-158`, `ga/GaDdi.cpp:4765-4916`). It is created with a dummy 4x4 `D3DSWAPEFFECT_COPY` windowed swapchain and `HARDWARE_VERTEXPROCESSING | FPU_PRESERVE`, "Gallium backend does not use the implicit swapchain" (`ga/GaWddm.cpp:641-684`; v6.0 did the same against Wine, `w6/VBoxD3DIf.cpp:813-877`). `Flush` explicitly tolerates "Windows 10 can call the Flush when pDevice9If is not yet initialized" (`ga/GaDdi.cpp:540-541`).
- Device-side the UMD keeps only what the API cannot give back: user-pointer streams, the index-buffer binding and stride, a cached viewport and scissor, bound RTs/samplers (`gw/VBoxDispD3D.h:139-184`).
- Command transport: Nine's svga winsys copies SVGA3D commands into the WDDM context command buffer and submits with `pfnRenderCb`, a fence id in the first dword, splitting at command boundaries when the buffer is too small (`ga/GaDrvEnvWddm.cpp:499-576`). Host surfaces are created with a driver escape, not WDDM allocations ("Required, otherwise graphics corruption can happen. No idea why. Eventually we probably have to create allocations for surfaces, as a WDDM driver should do. Then the Escape hack will be removed." `ga/GaDrvEnvWddm.cpp:195-240`). This is the same model as accel-virt: API-level replay, objects named by host ids, WDDM allocations only where dxgkrnl must see one.

---

## 1. Adapter: `pfnGetCaps`

`GaDdiAdapterGetCaps` (`ga/GaDdi.cpp:4961-5234`). It always returns `S_OK`, even when it set `hr = E_INVALIDARG` for a short buffer (`:5233`) - a bug, not a contract.

| D3DDDICAPS_TYPE | VirtualBox answer | cite |
|---|---|---|
| DDRAW (1) | `DDRAW_CAPS` zeroed (no colorkey, no Z blts) | `ga/GaDdi.cpp:4972-4981` |
| DDRAW_MODE_SPECIFIC (2) | zeroed except the caller's `Head` | `:4983-4996` |
| GETFORMATCOUNT / GETFORMATDATA (3/4) | static `gGaFormatOps3D` table (below) | `:4998-5005` |
| GETMULTISAMPLEQUALITYLEVELS (5) | unimplemented: asserts, zero-fills `pData` | `:5207-5225` |
| GETD3DQUERYCOUNT / DATA (6/7) | 2: `EVENT`, `OCCLUSION` | `:4956-4959`, `:5007-5014` |
| GETD3D3CAPS (8) | `D3DHAL_GLOBALDRIVERDATA` with HW rasterization, no T&L, render depths 8/16/24/32 | `:5016-5089` |
| GETD3D5CAPS / D3D6CAPS (9/10) | unimplemented, zero-filled | `:5208-5209` |
| GETD3D7CAPS (11) | `D3DHAL_D3DEXTENDEDCAPS` zeroed except `dwSize` | `:5091-5102` |
| GETD3D8CAPS (12) | first `offsetof(D3DCAPS9, DevCaps2)` bytes of the D3D9 caps; requires exactly that `DataSize` | `:5123-5141` |
| GETD3D9CAPS (13) | the backend's `GetDeviceCaps` plus tweaks (below) | `:5104-5121` |
| GETGAMMARAMPCAPS (34) | `0` | `:5143-5145` |
| decode / extension / content-protection counts | zero | `:5200-5205` |
| video-processor GUIDs / RT formats / caps | real answers only if the KMD reports DXVA | `:5147-5198` |
| everything else (procamp, DXVA-HD 36-41, certificates, 45-51 architecture / min-precision / MPO / simple-instancing / marker caps) | `default:` asserts and leaves the buffer untouched | `:5227-5229` |

There is no display-mode list among the caps types; the formats marked `FORMATOP_DISPLAYMODE | FORMATOP_3DACCELERATION` (X8R8G8B8, R5G6B5, P8 here) are what pairs the KMD's modes with D3D9 adapter formats. (The pairing rule is the runtime's, not visible in VBox source.)

### D3DCAPS9

Not a static table: `IDirect3D9Ex::GetDeviceCaps(HAL)` from Nine, then OR-in what feature level 9_3 needs and the backend does not set (`ga/GaWddm.cpp:834-884`):

- `Caps |= D3DCAPS_READ_SCANLINE` ("(Apparently) needed for Windows Media Player"), `Caps2 |= CANRENDERWINDOWED | CANSHARERESOURCE`, `DevCaps |= FLOATTLVERTEX` ("obsolete but must be set by the driver") (`:852-857`).
- `PrimitiveMiscCaps |= FOGINFVF | INDEPENDENTWRITEMASKS`, `RasterCaps |= SUBPIXEL | STIPPLE | ZBIAS`, `TextureCaps |= TRANSPARENCY | TEXREPEATNOTSCALEDBYSIZE`, `(Volume)TextureAddressCaps |= MIRRORONCE`, `VertexTextureFilterCaps |= MINFPOINT | MAGFPOINT` (`:858-868`).
- `PS20Caps.Caps |= D3DPS20CAPS_NOTEXINSTRUCTIONLIMIT` "Required for Shader Model 3.0 but not set by Gallium backend" (`:871-872`); `Caps3 |= D3DCAPS3_DXVAHD` if the KMD has DXVA-HD (`:874-875`).
- The checklist of what each of 9_1/9_2/9_3 requires (flags and minimum values) is in `gw/VBoxDispD3DIf.cpp:379-481`; it is the compact reference for what the DXGI 9_x feature-level path checks.
- `NumSimultaneousRTs` from the backend sizes the device's RT array (`gw/VBoxDispD3DIf.cpp:514-517`, `ga/GaDdi.cpp:4776-4783`).
- v6.0 (Wine backend) forced more: GuardBand ±8192, `VS20/PS20.DynamicFlowControlDepth = 24`, `NumTemps = D3DVS20_MAX_NUMTEMPS`, `StencilCaps |= TWOSIDED`, `DeclTypes |= FLOAT16_2 | FLOAT16_4`, `SEPARATEALPHABLEND | MRTINDEPENDENTBITDEPTHS`, `MaxVertexW = 1e10` ("Required by D3D feature level 9.3"), and clamped `Max{Vertex,Pixel}Shader30InstructionSlots` to 32768 as a "workaround for wine not returning InstructionSlots correctly for shaders v3.0" (`w6/VBoxDispD3DIf.cpp:1144-1216`).

### FORMATOP list (`ga/GaWddm.cpp:886-1183`)

All entries have `FlipMsTypes = BltMsTypes = PrivateFormatBitCount = 0`, so **no multisampling is exposed**.

| formats | ops |
|---|---|
| A8R8G8B8 | TEXTURE, VOLUME, CUBE, OFFSCREEN_RT, SAME_FORMAT_RT, SAME_FORMAT_UP_TO_ALPHA_RT, CONVERT_TO_ARGB, OFFSCREENPLAIN, SRGBREAD, SRGBWRITE, MEMBEROFGROUP_ARGB, VERTEXTEXTURE |
| X8R8G8B8 | as above minus UP_TO_ALPHA, plus DISPLAYMODE, 3DACCELERATION |
| R5G6B5 | texture/RT ops + DISPLAYMODE, 3DACCELERATION (no sRGB) |
| A2R10G10B10, X1R5G5B5, A1R5G5B5, A4R4G4B4 | texture + offscreen RT (+ UP_TO_ALPHA for alpha formats) |
| L16, A8L8, A8, L8, A4L4 | TEXTURE, VOLUME, CUBE, OFFSCREENPLAIN, VERTEXTEXTURE (no RT) |
| D16, D24S8, D24X8, X8D24, D32F_LOCKABLE, S8D24 | TEXTURE, ZSTENCIL, ZSTENCIL_WITH_ARBITRARY_COLOR_DEPTH |
| D16_LOCKABLE | ZSTENCIL only |
| DXT1-5 | TEXTURE, CUBE, OFFSCREENPLAIN, SRGBREAD, VERTEXTEXTURE (no volume) |
| X8L8V8U8, A2W10V10U10, V8U8, V16U16 | texture + BUMPMAP |
| Q8W8V8U8, Q16W16V16U16 | texture + offscreen RT + BUMPMAP (+DMAP for Q16) |
| CxV8U8 | NOFILTER, NOALPHABLEND, NOTEXCOORDWRAPNORMIP only |
| R16F, R32F, G16R16F, G32R32F, A16B16G16R16F, A32B32G32R32F, G16R16, A16B16G16R16 | texture + offscreen RT + SAME_FORMAT_RT |
| P8 | DISPLAYMODE, 3DACCELERATION, OFFSCREENPLAIN |
| UYVY, YUY2 | CONVERT_TO_ARGB, OFFSCREENPLAIN, NOFILTER, NOALPHABLEND, NOTEXCOORDWRAPNORMIP |
| X8B8G8R8 | full texture/RT set + AUTOGENMIPMAP, OVERLAY, DMAP |
| A2B10G10R10 | texture/RT + AUTOGENMIPMAP, DMAP |
| BINARYBUFFER | OFFSCREENPLAIN |

Absent: A8B8G8R8, every vendor FOURCC (INTZ, DF16, DF24, RAWZ, NULL, ATI1, ATI2, RESZ), MULTI2_ARGB8, and AUTOGENMIPMAP on A8R8G8B8/X8R8G8B8. The 2D-only `gVBoxFormatOpsBase` (`gw/VBoxDispD3DIf.cpp:37-45`) is dead code.

---

## 2. Device-function map (VirtualBox, Gallium path)

Function table: `ga/GaDdi.cpp:4810-4916`. `pfnLockAsync`, `pfnUnlockAsync`, `pfnRename` and the DXVA-HD slots are left NULL (`:4906-4916`).

| DDI slot | VirtualBox implementation | cite |
|---|---|---|
| SetRenderState | `SetRenderState((D3DRENDERSTATETYPE)State, Value)`, a plain cast with "@todo not entirely correct" | `ga/GaDdi.cpp:2830-2845`, `gw/VBoxD3DIf.cpp:62-66` |
| SetTextureStageState | table lookup: TSS -> `SetTextureStageState`, sampler states -> `SetSamplerState(Stage, …)` (§5) | `ga/GaDdi.cpp:2871-2895`, `gw/VBoxD3DIf.cpp:68-114` |
| SetTexture | resource -> its `IDirect3DBaseTexture9`, `SetTexture(Stage, …)`; stages 256-260 (DMAP, vertex samplers) tracked in slots 16-20 | `ga/GaDdi.cpp:2897-2957`, `gw/VBoxDispD3D.h:40-45` |
| UpdateWInfo, ValidateDevice, SetPriority, QueryResourceResidency | return `S_OK`, do nothing (ValidateDevice leaves `NumPasses` untouched) | `ga/GaDdi.cpp:2847-2869`, `:3135-3149`, `:4515-4531` |
| SetPixelShader / SetVertexShaderFunc / SetVertexShaderDecl | handle is the API object pointer, `Set*()` | `:2959-2974`, `:1497-1513`, `:3512-3528` |
| SetPixelShaderConst / ConstI / ConstB, SetVertexShaderConst / I / B | `Set{Pixel,Vertex}ShaderConstant{F,I,B}(Register, data, Count)`; `Count` is vec4 count for F/I and BOOL count for B | `:2976-2990`, `:1667-1699`, `:1531-1582`; arg comment `d3dumddi.h:291-299` |
| SetStreamSource | `SetStreamSource(Stream, vb, Offset, Stride)`; clears any user-pointer stream | `:3566-3623` |
| SetStreamSourceUm | records pointer+stride only; unbinds the API stream | `:2992-3031` |
| SetIndices | `SetIndices(ib)`, remembers `Stride` (2/4) | `:3033-3067` |
| SetIndicesUm | records pointer+size, `SetIndices(NULL)` | `:3069-3098` |
| DrawPrimitive, DrawIndexedPrimitive, DrawPrimitive2, DrawIndexedPrimitive2 | §6 | `:2424-2828` |
| DrawRectPatch, DrawTriPatch | `E_NOTIMPL` | `:4569-4605` |
| VolBlt | lock both volume textures per level, CPU copy | `:424-528` |
| BufBlt | `E_NOTIMPL` | `:3100-3115` |
| TexBlt | `UpdateTexture` (whole texture) or `UpdateSurface` (level 0 sub-rect) | `:279-377` |
| StateSet | `E_NOTIMPL` | `:3117-3133` |
| Clear | `Clear(NumRect, (D3DRECT*)pRect, Flags, FillColor, FillDepth, FillStencil)` | `:3151-3179` |
| UpdatePalette, SetPalette | `E_NOTIMPL` | `:3181-3216` |
| MultiplyTransform, SetTransform, SetMaterial, SetLight, CreateLight, DestroyLight | `E_NOTIMPL` (no fixed-function vertex pipeline from the DDI) | `:3218-3252`, `:3296-3366` |
| SetViewport + SetZRange | merged into one cached `D3DVIEWPORT9`, `SetViewport` on each | `:3254-3294` |
| SetClipPlane | `SetClipPlane(Index, Plane)` | `:3368-3382` |
| GetInfo | only `D3DDDIDEVINFOID_VCACHE` ('CACH', size 0) | `:3384-3419` |
| Lock / Unlock | §4 | `:639-1428` |
| CreateResource / DestroyResource / OpenResource | §3, §10 | `:1743-2422` |
| SetDisplayMode | `pfnSetDisplayModeCb(hPrimaryAllocation)` | `:3421-3448` |
| Present | flush, `pfnPresentCb(src, dst, hContext)` (§9) | `:572-637` |
| Flush | flush the backend pipe | `:530-570` |
| CreateVertexShaderFunc / CreatePixelShader / DeleteX | §7 | `:1430-1665` |
| CreateVertexShaderDecl / DeleteVertexShaderDecl | §7 | `:3464-3545` |
| SetScissorRect | cached, `SetScissorRect` | `:3547-3564` |
| SetStreamSourceFreq | `SetStreamSourceFreq(Stream, Divider)` | `:3625-3639` |
| SetConvolutionKernelMono, ComposeRects, DepthFill | `E_NOTIMPL` | `:3641-3675`, `:3710-3726` |
| Blt | §8 | `:90-277` |
| ColorFill | `ColorFill(surface, &DstRect, Color)`; asserts `Flags == 0` | `:3677-3708` |
| CreateQuery / DestroyQuery / IssueQuery / GetQueryData | §9 | `:3728-3851` |
| SetRenderTarget / SetDepthStencil | §5 | `:3899-3997` |
| GenerateMipSubLevels | `E_NOTIMPL` | `:3999-4015` |
| decode, extension device, capture | `E_NOTIMPL` | `:4017-4278`, `:4533-4567` |
| video-process device | through `GaDxva.cpp` | `:4143-4213` |
| CreateOverlay / UpdateOverlay / FlipOverlay / DestroyOverlay | straight to the `pfn*OverlayCb` callbacks with dirty region | `:4290-4513` |
| DestroyDevice | releases the backend device | `:4607-4630` |

The v6.0 Wine path fills the same table with the same mappings (`w6/VBoxDispD3D.cpp:6259-6360`), and returns `E_FAIL` instead of `E_NOTIMPL` for StateSet and GenerateMipSubLevels (`w6/VBoxDispD3D.cpp:3240-3251`, `:5492-5504`).

---

## 3. CreateResource

`GaDdiCreateResource` (`ga/GaDdi.cpp:1743-2057`) and `GaD3DIfCreateForRc` (`ga/GaWddm.cpp:387-639`). Every `D3DDDI_SURFACEINFO` in `pSurfList` becomes one "allocation" record (`ga/GaDdi.cpp:1797-1841`); the API object is chosen from `Flags`, in this priority:

| condition (first match) | API call | sub-resource mapping |
|---|---|---|
| `Texture`, **or `Flags.Value == 0`**, or video RT | `CreateTexture(w, h, Levels = SurfCount, usage, fmt, pool, …)` | i-th surface = mip i (`ga/GaWddm.cpp:412-484`, `gw/VBoxDispD3D.h:56`) |
| … with `CubeMap` | `CreateCubeTexture(edge, SurfCount/6, …)`; rejects w != h or `SurfCount % 6` | **face-major**: index = face * levels + level (`gw/VBoxDispD3D.h:273-275`, `ga/GaWddm.cpp:420-447`) |
| … with `Volume` | `CreateVolumeTexture(w, h, d, SurfCount, usage \| D3DUSAGE_DYNAMIC, …)` "because we have to lock it in GaDdiVolBlt" | i = mip (`ga/GaWddm.cpp:448-460`) |
| `RenderTarget` or `Primary` | one `CreateRenderTarget(w, h, fmt, ms, quality, Lockable = !NotLockable)` per surface | each surface separate (`:501-546`) |
| `ZBuffer` | `CreateDepthStencilSurface(…, Discard = TRUE)` per surface | (`:547-568`) |
| `VertexBuffer` | `CreateVertexBuffer(Width bytes, usage & ~DYNAMIC, Fvf, pool)` per surface | width is the byte size (`:569-589`) |
| `IndexBuffer` | `CreateIndexBuffer(Width, usage, Format = INDEX16/32, pool)` | (`:590-608`) |

- `Flags.Value == 0` is what `CreateOffscreenPlainSurface` produces; VBox turns it into a 1-level texture, so there is no `CreateOffscreenPlainSurface` call anywhere. Lockable RTs/primaries become `CreateRenderTarget`, never a texture.
- Usage from flags: `Dynamic, AutogenMipmap, DMap, WriteOnly, NPatches, Points, RenderTarget, RtPatches, TextApi, ZBuffer` (`ga/GaWddm.cpp:268-294`). `HintStatic`, `NotLockable` (except for RTs), `DiscardRenderTarget`, `MightDrawFromLocked`, `Primary`, `SharedResource` do not become usage bits. Vertex buffers drop `D3DUSAGE_DYNAMIC` ("avoid using dynamic to ensure wine does not switch do user buffer", `w6/VBoxD3DIf.cpp:713-716`; kept with a "need for Gallium?" todo at `ga/GaWddm.cpp:577-583`).
- Pool: `SYSTEMMEM -> D3DPOOL_SYSTEMMEM`; `VIDEOMEMORY / LOCALVIDMEM / NONLOCALVIDMEM -> D3DPOOL_DEFAULT` ("@todo what would be proper here?") (`gw/VBoxD3DIf.cpp:44-60`). The DDI never sees `MANAGED`/`SCRATCH`: the runtime keeps those itself and uploads with TexBlt.
- Format and multisample type are plain casts (`gw/VBoxD3DIf.cpp:32-42`).
- Width for `SYSTEMMEM` resources is widened to `SysMemPitch / bpp` (`d3dWidth`) so the backend's row pitch equals the runtime's (`ga/GaDdi.cpp:1830-1840`; v6.0 asserts this was only ever seen for non-cube textures, `w6/VBoxDispD3D.cpp:4436-4457`).
- `SYSTEMMEM` textures and cube textures are created **on the runtime's memory**: `pSharedHandle = &aAllocations[0].pvMem`, a VBox-patched Nine feature (`ga/GaWddm.cpp:431-437`, `:463-474`; `nine/texture9.c:73-80`, `nine/cubetexture9.c:50-200`). "It is expected that allocations are in continous memory blocks… 'Some apps expect the memory to be allocated in continous blocks'". Other `SYSTEMMEM` resources are copied into the backend right after creation (`ga/GaWddm.cpp:615-621`, `:153-266`). v6.0 passed a per-level pointer array to a Wine extension instead (`w6/VBoxD3DIf.cpp:438-452`).
- WDDM allocations (`pfnAllocateCb`) are made **only** for shared resources (one KM resource) and for RT/primary resources (per-surface allocations, no KM resource); textures, buffers and depth buffers have no kernel allocation at all (`ga/GaDdi.cpp:1843-1869`, `:1962-2008`). For RT/primary/shared the host surface id goes into the allocation's private data (`:1924-1954`). "For some reason shared resources are created with zero km resource handle on Win7+" (`:1968-1969`).
- Allocation-time private data: one `VBOXWDDM_RCINFO` for the resource plus one `VBOXWDDM_ALLOCINFO` per surface, laid out in a single buffer (`ga/GaDdi.cpp:1706-1741`).

---

## 4. Lock / Unlock

`GaDdiLock` / `GaDdiUnlock` (`ga/GaDdi.cpp:639-1428`). There are no guest-side shadows of DEFAULT-pool resources: Lock calls the backend's `LockRect/LockBox/Lock` and hands its pointer to the runtime.

- Flags: `Discard, NoOverwrite, ReadOnly, DoNotWait` map to `D3DLOCK_*`; `WriteOnly`, `MightDrawFromLocked`, `NotifyOnly` do not (`gw/VBoxD3DIf.cpp:147-159`).
- Region: `AreaValid -> RECT`, `BoxValid -> D3DBOX`, `RangeValid -> (Offset, Size)` for buffers; absent means whole resource (`ga/GaDdi.cpp:726-731`, `:848-853`, `:932-937`). Output `pSurfData`, `Pitch`, `SlicePitch` (0 for 2D) (`:826-833`, `:911-917`). Buffer `Pitch` is the buffer width (`:954-955`).
- Sub-resource: mip index for textures, face-major index for cubes, mip for volumes (`:791-814`).
- Re-entrant locks: Nine allows nested buffer locks but not nested surface locks, so a second lock with the same ReadOnly/area reuses the mapping, a different one unlocks and relocks; a lock count is kept and only the last Unlock reaches the backend (`:733-776`, `:1217-1248`).
- `NotifyOnly` (pre-allocated `SYSTEMMEM` memory): quoting the docs, "for preallocated system memory surfaces, the runtime ignores the driver-set memory and pitch" (`:674-677`). Policy (`:666-681`):
  - index/vertex buffers: copy backend -> runtime memory on lock (unless Discard), runtime memory -> backend on unlock (unless ReadOnly) (`:983-1010`, `:1273-1293`);
  - textures/cubes: nothing, they alias the memory;
  - volumes and surfaces: brute-force sync of **all** sub-resources, because "Windows always passes pData->SubResourceIndex == 0 for NotifyOnly locks when locking a textures, cubemaps and volumes" (`:679-681`, `:690-704`, `:1335-1349`).
- Vertex/index buffer with `MightDrawFromLocked && (Discard || NoOverwrite)`: the backend buffer is **not** locked at all (`:942-949`, `:1268-1270`); such a buffer is then drawn through `DrawPrimitive2` from `pvMem` (§6). This only works for `SYSTEMMEM` buffers (where `pvMem` exists); for a DEFAULT buffer the returned `pSurfData` is whatever `LockedRect.pBits` held.
- `D3DERR_WASSTILLDRAWING` is expected only on the non-3D path (`:1145`); the 3D path never returns it itself.

---

## 5. State

- **Render states**: `D3DDDIRENDERSTATETYPE` is cast to `D3DRENDERSTATETYPE` (`gw/VBoxD3DIf.cpp:62-66`). The DDI enum has no values < 7 and contains DDI-only or legacy states that the API never defined: `LINEPATTERN (10)`, `ZVISIBLE (30)`, `EDGEANTIALIAS (40)`, `COLORKEYENABLE (41)`, `OLDALPHABLENDENABLE (42)`, `ZBIAS (47)`, `TRANSLUCENTSORTINDEPENDENT (51)`, `SCENECAPTURE (62, "DDI only to replace SceneCapture")`, `STIPPLEPATTERN00-31 (64-95)`, `COLORKEYBLENDENABLE (144)`, `DELETERTPATCH (169)` (`d3dumddi.h:74-225`). VBox forwards them unfiltered; DXVK accepts any value 7-255, stores it and logs "Unhandled render state" once (`dxvk/d3d9_device.cpp:2314-2319`, `:2660-2665`), so the cast is harmless there.
- **Texture-stage vs sampler states**: the DDI has no `SetSamplerState`. Sampler state arrives through `pfnSetTextureStageState` with DDI numbering, which VBox splits with a lookup table (`gw/VBoxD3DIf.cpp:68-114`, enum at `d3dumddi.h:245-283`):

  | D3DDDITSS | value | API |
  |---|---|---|
  | COLOROP … TEXCOORDINDEX | 1-11 | same-valued `D3DTSS_*` |
  | (unused) | 12 | - |
  | ADDRESSU, ADDRESSV, BORDERCOLOR, MAGFILTER, MINFILTER, MIPFILTER, MIPMAPLODBIAS, MAXMIPLEVEL, MAXANISOTROPY | 13-21 | `D3DSAMP_` ADDRESSU (1), ADDRESSV (2), BORDERCOLOR (4), MAGFILTER (5), MINFILTER (6), MIPFILTER (7), MIPMAPLODBIAS (8), MAXMIPLEVEL (9), MAXANISOTROPY (10) |
  | BUMPENVLSCALE, BUMPENVLOFFSET, TEXTURETRANSFORMFLAGS | 22-24 | `D3DTSS_*` |
  | ADDRESSW | 25 | `D3DSAMP_ADDRESSW` |
  | COLORARG0, ALPHAARG0, RESULTARG | 26-28 | `D3DTSS_*` |
  | SRGBTEXTURE, ELEMENTINDEX, DMAPOFFSET | 29-31 | `D3DSAMP_*` |
  | CONSTANT | 32 | `D3DTSS_CONSTANT` |
  | TEXTUREMAP (0), DISABLETEXTURECOLORKEY (33), TEXTURECOLORKEYVAL (34) | | no API equivalent; table entry is `FORCE_DWORD` and only an `Assert` guards it |

  `Stage` is passed through unchanged, so a sampler state for `D3DDMAPSAMPLER (256)` or `D3DVERTEXTEXTURESAMPLER0-3 (257-260)` arrives with that stage number (`ga/GaDdi.cpp:2883-2890`; `SetTexture` handles the same range, `gw/VBoxDispD3D.h:43-45`).
- **Viewport**: the DDI splits `SetViewport` (X, Y, W, H) from `SetZRange` (MinZ, MaxZ); VBox caches one `D3DVIEWPORT9`, defaults `{0,0,1,1,0,1}`, and calls `SetViewport` from both (`gw/VBoxDispD3D.h:159-169`, `ga/GaDdi.cpp:3254-3294`, `:4795-4803`).
- **SetRenderTarget**: `IDirect3DDevice9::SetRenderTarget` resets viewport and scissor, the DDI does not expect that, so VBox re-applies the cached viewport and scissor after every RT change (`ga/GaDdi.cpp:3936-3945`). `RenderTargetIndex` is bounds-checked against `NumSimultaneousRTs` (`:3908`). The RT is a (resource, `SubResourceIndex`) pair; for textures the surface is `GetSurfaceLevel(i)`/`GetCubeMapSurface(face, level)` (`gw/VBoxD3DIf.h:81-136`).
- **SetDepthStencil**: `SetDepthStencilSurface` on sub-resource 0 (`ga/GaDdi.cpp:3956-3997`).
- Clip planes, scissor and stream frequency are direct (`:3368-3382`, `:3547-3564`, `:3625-3639`).

---

## 6. Draws

Argument differences from the API (struct comments at `d3dumddi.h:316-355`):

| DDI | fields | VirtualBox mapping |
|---|---|---|
| DrawPrimitive | `PrimitiveType, VStart, PrimitiveCount` (primitives, as in the API) | bound VB: `DrawPrimitive(type, VStart, count)`; one user-pointer stream: `DrawPrimitiveUP(type, count, pUm + VStart*stride, stride)`; several user-pointer streams: not implemented, logs and draws nothing (`ga/GaDdi.cpp:2424-2507`) |
| DrawIndexedPrimitive | `BaseVertexIndex, MinIndex, NumVertices, StartIndex, PrimitiveCount` - same as API | bound VB+IB: `DrawIndexedPrimitive(...)` unchanged; user-pointer vertices: `DrawIndexedPrimitiveUP(type, MinIndex, NumVertices, count, idx + stride*StartIndex, INDEX16/32 by stride, pUm, stride)` with indices from `SetIndicesUm` or from a SYSTEMMEM IB's `pvMem`; then re-binds the IB because DIPUP clears it (`:2510-2618`). `BaseVertexIndex` is ignored on the UP path. |
| DrawPrimitive2 | `FirstVertexOffset` is a **byte** offset into stream 0; only stream 0 is used ("Stream zero contains transform vertices and is the only stream that should be accessed") | VB locked with MightDrawFromLocked: `DrawPrimitiveUP` from `pvMem + streamOffset + FirstVertexOffset`, then re-bind stream 0; otherwise `DrawPrimitive(type, FirstVertexOffset / stride, count)` (`:2621-2672`) |
| DrawIndexedPrimitive2 | `BaseVertexOffset` (**bytes**, may be negative), `StartIndexOffset` (**bytes**), plus `dwIndicesSize` and a user `pIndexBuffer` | stream 0 from `SetStreamSourceUm` or a SYSTEMMEM VB's `pvMem` (DEFAULT VB is "unsupported!!"); `DrawIndexedPrimitiveUP(type, MinIndex, NumVertices, count, pIndexBuffer + StartIndexOffset, …, pVB + BaseVertexOffset, stride)`; `PrimitiveCount == 0` is a no-op; then re-binds stream 0 and the IB (`:2712-2828`) |

- DrawPrimitive2/DrawIndexedPrimitive2 are the runtime's software-vertex-processing / pre-transformed path: stream 0 holds vertices the runtime processed (the WDK sentence VBox quotes at `:2632`, `:2753-2755`).
- v6.0 rebased DIP2 indices to zero because "wine does not handle MinVertexIndex correctly": it copies the indices, subtracts `MinIndex`, and passes the vertex pointer advanced by `BaseVertexOffset + MinIndex*stride` (`w6/VBoxDispD3D.cpp:3010-3058`). The index count comes from `vboxWddmVertexCountFromPrimitive` (list ×3, strip/fan +2, line list ×2, strip +1, points ×1) (`ga/GaDdi.cpp:2675-2709`). DXVK's `DrawIndexedPrimitiveUP` copies vertices `[0, MinVertexIndex + NumVertices)` from the pointer it is given (`dxvk/d3d9_device.cpp:3185-3215`), so the rebasing is also what keeps a remote UP draw from shipping the unused prefix.
- 16-bit vs 32-bit indices are chosen from the stride/size recorded by SetIndices/SetIndicesUm or `dwIndicesSize`; anything other than 2 or 4 is rejected (`ga/GaDdi.cpp:2547`, `:2741-2745`).
- Point sprites: no special handling anywhere; `D3DUSAGE_POINTS` and the point render states pass through (`ga/GaWddm.cpp:281-282`).

---

## 7. Shaders and constants

- `CreateVertexShaderFunc(pData, pCode)` / `CreatePixelShader(pData, pCode)`: `pCode` is the raw D3D9 token stream, version token first, `Size`/`CodeSize` in bytes (VBox requires ≥ 8). It goes straight into `CreateVertexShader/CreatePixelShader`; the returned API object pointer is the DDI handle (`ga/GaDdi.cpp:1430-1495`, `:1584-1648`).
- Version bump: `vs_2_0` (`0xFFFE0200`) and `ps_2_0` (`0xFFFF0200`) are copied and patched to x.1 "because Gallium is strict and rejects 2.0 shaders which use 2.1 instructions" (`:1455-1470`, `:1608-1623`). The runtime evidently passes 2.x-only instructions in 2.0-tagged shaders.
- `CreateVertexShaderDecl(pData, pVertexElements)`: `D3DDDIVERTEXELEMENT` is layout-identical to `D3DVERTEXELEMENT9` (compile-time checked, `:3450-3462`), but the array is `NumVertexElements` long and **may lack the `D3DDECL_END()` terminator**, so VBox appends one when it is missing (`:3464-3510`).
- No FVF path: there is no `SetFVF` in the DDI and VBox never calls one; the runtime turns FVFs into declarations.
- Fixed-function vertex state (`SetTransform`, `MultiplyTransform`, `SetMaterial`, `SetLight`, `Create/DestroyLight`) is `E_NOTIMPL` (`:3218-3252`, `:3296-3366`); fixed-function texture-stage and render states are forwarded. What the runtime does with an FF vertex pipeline when the driver returns `E_NOTIMPL` is not visible in VBox source.
- Constants: F and I registers are `Count` vec4s, B registers `Count` BOOLs (`d3dumddi.h:291-301`); forwarded unchanged.

---

## 8. Clear, Blt, ColorFill, TexBlt, VolBlt, BufBlt, GenerateMipSubLevels

- **Clear**: `RECT` and `D3DRECT` have the same layout (asserted, `ga/GaDdi.cpp:3151-3159`), and `NumRect/pRect` go to `IDirect3DDevice9::Clear` unchanged together with `Flags, FillColor, FillDepth, FillStencil` (`:3161-3179`). The DDI flags may contain `D3DCLEAR_COMPUTERECTS` (`d3dumddi.h:408`), which VBox forwards. DXVK clips to viewport and scissor itself and fails a Z/stencil clear with no depth buffer bound (`dxvk/d3d9_device.cpp:1895-1925`).
- **Blt** (`D3DDDIARG_BLT`, `ga/GaDdi.cpp:90-277`), by the two pools:
  - SYSTEMMEM -> SYSTEMMEM: `E_NOTIMPL`. The WDK text VBox quotes: "the driver should synchronize as necessary but should not copy the contents of the source surface. The Direct3D runtime copies the contents after the call to pfnRenderCb returns." (`:139-152`).
  - SYSTEMMEM -> DEFAULT: `UpdateSurface(src, &SrcRect, dst, &DstRect.topleft)`; asserts no stretch (`:153-167`).
  - DEFAULT -> SYSTEMMEM: lock both and memcpy (`gaSurfaceCopyD2S`, formats must match) (`:36-87`, `:171-189`).
  - DEFAULT -> DEFAULT: `StretchRect(src, &SrcRect, dst, &DstRect, filter)`, filter from `Point`/`Linear` flags (`gw/VBoxD3DIf.cpp:161-178`); `BeginPresentToDwm/ContinuePresentToDwm/EndPresentToDwm` (0x100/0x200/0x400) ignored; any other flag (color key, mirror, rotate, LinearToSrgb) only trips an assert (`ga/GaDdi.cpp:195-198`). If the source is an RT and the destination is not, `StretchRect` would fail, so VBox goes `GetRenderTargetData` into a SYSTEMMEM temp texture and `UpdateSurface` (`:218-259`).
  - "Gallium flips the image vertically if scaling is applied" - an old comment pointing at a "Hack. Flip src Y." in Nine's StretchRect (`:202-214`); that hack is no longer in the pinned Nine (grep finds no match), the comment is stale.
  - Sub-resource selection: `GetSurfaceLevel`/`GetCubeMapSurface` on the subresource index; "if texture is used as a destination, we should update sub-layers as well which is not done currently" (`gw/VBoxD3DIf.h:100-102`).
- **ColorFill**: `IDirect3DDevice9::ColorFill(surface(SubResourceIndex), &DstRect, Color)`; `Color` is A8R8G8B8 (`d3dumddi.h:802-809`); `PresentToDwm` flags unhandled (`ga/GaDdi.cpp:3677-3708`).
- **TexBlt** (`D3DDDIARG_TEXBLT`: dst, src, `CubeMapFace`, `DstPoint`, `SrcRect`; source SYSTEMMEM, destination not) (`ga/GaDdi.cpp:279-377`):
  - same size, same format, `DstPoint == 0`, `SrcRect` = whole level 0: `AddDirtyRect(&SrcRect)` (per face for cubes) and `UpdateTexture(src, dst)`. The dirty rect is forced because a SYSTEMMEM texture created on application memory and written "without Lock/Unlock" has no dirty region of its own (`:309-346`).
  - otherwise: `UpdateSurface(level 0 of src, &SrcRect, level 0 of dst, &DstPoint)`; asserts both are 1-level, "@todo Miplevels" (`:347-373`).
  - DXVK's `UpdateTexture` requires SYSTEMMEM -> DEFAULT, equal formats, and source levels ≥ destination levels (`dxvk/d3d9_device.cpp:1040-1060`).
- **VolBlt**: lock source box READONLY and destination box DISCARD per mip level, halving the boxes each level, CPU row copy (`ga/GaDdi.cpp:424-528`).
- **BufBlt**, **DepthFill**, **GenerateMipSubLevels**, **ComposeRects**: `E_NOTIMPL` (`:3100-3115`, `:3710-3726`, `:3999-4015`, `:3659-3675`). AUTOGENMIPMAP is advertised only for X8B8G8R8 and A2B10G10R10 (§1), which keeps the runtime off GenerateMipSubLevels for common formats.

---

## 9. Present, Flush, queries

- **Present** (`ga/GaDdi.cpp:572-637`): resolve src/dst allocations (either may be absent), flush the backend, then `pfnPresentCb({hSrcAllocation, hDstAllocation, hContext})`. `Flags` (Blt/ColorFill/Flip) and `FlipInterval` are not inspected. All pixel movement happens in the KMD's `DxgkDdiPresent` (`docs/research/d3d-desktop/prior-art.md` §1, "DxgkDdiPresent").
- The D3D9 DDI has no swapchain object: the runtime creates back buffers as `RenderTarget` (and for fullscreen `Primary`) resources and only ever presents an allocation. v6.0 had to rebuild swapchains for Wine by matching presented allocations on size, alpha-less format and `VidPnSourceId` (`w6/VBoxDispD3D.cpp:975-1047`), presenting through `IDirect3DSwapChain9::Present` (`:1771-1808`). It also assumed "presenting shared resource is only possible when 3d app is rendering with composited desktop on, no need to do anything else since dwm will present everything for us" - a shared-surface present was only a flush (`:1810-1824`). The Gallium path dropped all of this.
- Display mode: `SetDisplayMode(hResource, SubResourceIndex)` -> `pfnSetDisplayModeCb(hPrimaryAllocation)` (`ga/GaDdi.cpp:3421-3448`).
- Flush: `GaFlush()` on the backend (`:530-570`).
- **Queries** (`:3728-3851`): `D3DDDIQUERYTYPE` is cast to `D3DQUERYTYPE` (`gw/VBoxD3DIf.cpp:180-183`); only EVENT and OCCLUSION are advertised. Issue maps `Begin/End` to `D3DISSUE_BEGIN/END` (`gw/VBoxD3DIf.cpp:185-193`). GetQueryData calls `GetData(pData, GetDataSize(), 0)` (no flush flag) and returns its `S_OK`/`S_FALSE` unchanged (`ga/GaDdi.cpp:3816-3833`).

---

## 10. OpenResource and shared resources

`GaDdiOpenResource` (`ga/GaDdi.cpp:2144-2422`):

- Private data size tells the kinds apart: `sizeof(VBOXWDDM_ALLOCINFO)` per allocation = a VBox D3D9 allocation; `sizeof(VBOXDXALLOCATIONDESC)` = a VBox D3D11 (VBoxDX) allocation (`:2169-2245`).
- No resource-level private data = a **standard allocation** (shared primary, shadow, staging): opened as a Primary/LOCALVIDMEM surface with no API object yet (`:2247-2295`); the API object is created lazily on first use (`gw/VBoxD3DIf.h:59-77`, `ga/GaWddm.cpp:708-828`).
- A VBox-created shared resource: rebuild `RcDesc` from `VBOXWDDM_RCINFO`, create a **new** API object in this process, then escape `VBOXESC_GASHAREDSID(newSid -> originalSid)` so the KMD substitutes the original host surface in this process's command stream (`:2351-2401`). Destroying an opened resource sends `u32SharedSid = ~0` to drop the mapping (`:2075-2103`).
- A D3D11 resource opened in D3D9 (e.g. DWM interop) goes the same way with `VBOXESC_SVGAGETSID` to learn the original sid (`:2188-2237`, `:2296-2346`).
- On creation, a shared resource's `hSharedHandle` is set to its host surface id (`:1950-1954`).
- The earlier d3d-desktop note applies: this per-process-surface + redirection-escape design is what to avoid; a device-global host handle in the KMD allocation removes it (`docs/research/d3d-desktop/prior-art.md` §1, "Shared / cross-process resources").

---

## 11. Pitfalls named in the code

1. **D3DDDIFMT vs D3DFMT**: numerically identical for every API format; VBox casts (`gw/VBoxD3DIf.cpp:32-36`, "@todo check they are all equal"). DDI-only values exist that the API never defines: `D3DDDIFMT_G8R8 (91)`, `R8 (92)`, `A1 (118)`, `A2B10G10R10_XR_BIAS (119)`, the DXVA compressed-buffer range 150-181, `BINARYBUFFER (199)`, `MULTI2_ARGB8` (`sdk-shared/d3dukmdt.h:879-1015`).
2. **Render-state numbering**: DDI-only and legacy states (§5) reach the driver; the backend must tolerate or filter them.
3. **Sampler states come through SetTextureStageState** with DDI numbers 13-21, 25, 29-31 (D3D8-era texture-stage numbering); those are not `D3DTSS_*` values in the D3D9 API, so a straight cast to `SetTextureStageState` silently drops every filter and address mode (`gw/VBoxD3DIf.cpp:68-114`). Stage numbers 256-260 reach both SetTexture and SetTextureStageState.
4. **Flags.Value == 0 is a resource kind** (offscreen plain), not "no flags" (`gw/VBoxDispD3D.h:56`).
5. **Cube sub-resource order is face-major** (`gw/VBoxDispD3D.h:273-275`).
6. **SetRenderTarget must not reset viewport/scissor** as the API does (`ga/GaDdi.cpp:3936-3945`), and the DDI splits viewport from Z range (`gw/VBoxDispD3D.h:159-166`).
7. **DIP2/DP2 offsets are bytes**; `BaseVertexOffset` may be negative (`d3dumddi.h:343-355`); `PrimitiveCount == 0` happens and must be a no-op (`ga/GaDdi.cpp:2747-2751`).
8. **After a UP draw, stream 0 and the index buffer are unbound by the API**: re-bind them, or the next non-UP draw uses NULL (`ga/GaDdi.cpp:2580-2588`, `:2805-2823`).
9. **NotifyOnly locks of SYSTEMMEM textures/volumes come with SubResourceIndex 0** regardless of which level is touched (`ga/GaDdi.cpp:679-681`).
10. **SYSTEMMEM pitch can exceed Width × bpp**; size the backend surface from `SysMemPitch` or rows misalign (`ga/GaDdi.cpp:1830-1840`).
11. **Shader version tokens lie**: ps/vs 2.0 shaders contain 2.x instructions (`ga/GaDdi.cpp:1455-1458`).
12. **Vertex declarations may arrive without D3DDECL_END** (`ga/GaDdi.cpp:3479-3491`).
13. **Blt between SYSTEMMEM surfaces must not copy**; the runtime does it after the call (`ga/GaDdi.cpp:141-146`).
14. **TexBlt from SYSTEMMEM application memory has no dirty region** unless the driver adds one (`ga/GaDdi.cpp:324-338`).
15. **D3DCAPS2_CANRENDERWINDOWED, CANSHARERESOURCE, D3DCAPS_READ_SCANLINE, DevCaps FLOATTLVERTEX** are required by the runtime / DXGI 9_x path even if the backend does not report them (`ga/GaWddm.cpp:847-872`).
16. **GetCaps must handle types it does not know** (the current header goes to 51, `d3dumddi.h:2087-2144`); VBox's `default:` returns `S_OK` without writing (`ga/GaDdi.cpp:5227-5233`). Whether the runtime prefers an error there is not visible in VBox source.
17. **Flush can arrive before any backend device exists** (`ga/GaDdi.cpp:540-541`).
18. VMSVGA-specific: Nine clears a new RT on creation, which clobbered shared originals; VBox removed the clear (`nine/surface9.c:203-210`).

---

## 12. D3D9On12 (Microsoft's D3D9 UMD over D3D12)

**Caveat first.** 9on12 is an "enlightened" driver: d3d9.dll enters it through `OpenAdapter_Private`/`OpenAdapter2_Private`, and the public `OpenAdapter` returns `E_NOTIMPL` ("This should never be called") (`9on12/src/9on12Adapter.cpp:157-204`). The runtime also hands it fixed-function work already converted to shaders. Choices marked **[enl]** below depend on that and are not safe for a plain UMD such as ours.

### Adapter and caps (`9on12/src/9on12Adapter.cpp:16-115`)

- `DriverVersion = min(runtime Version, D3D_UMD_INTERFACE_VERSION)`: it echoes the runtime, capped at the WDK constant (`:319`).
- D3DCAPS9 is a static table (`9on12/include/9on12Caps.h:7-556`, limits in `9on12/include/pch.h:72-96`):
  - Shaders: VS/PS 3.0, 256 VS float constants, PS limits 224 F / 16 I / 16 B. Both `*Shader30InstructionSlots` are 32768; `MaxV/PShaderInstructionsExecuted` are infinite.
  - VS20: predication, 24/32/4. PS20: arbitrary swizzle, gradients, predication, no dependent-read limit, no tex-instruction limit, 512 slots (`:485-512`, `:544-551`).
  - Textures: 16384², volume extent 16384, NPOT unrestricted (no POW2 or NONPOW2CONDITIONAL), `MaxAnisotropy` 16. `NumSimultaneousRTs` 4, `MaxStreams` 16, `MaxStreamStride` 255 (`:206-225`, `:344-355`, `:436-442`, `:477`).
  - `Caps2 = CANAUTOGENMIPMAP | FULLSCREENGAMMA | DYNAMICTEXTURES | CANSHARERESOURCE` (no CANMANAGERESOURCE). `Caps3` includes `LINEAR_TO_SRGB_PRESENTATION` and `COPY_TO_VIDMEM/SYSTEMMEM` (`:19-35`).
  - `DevCaps` claims HWTRANSFORMANDLIGHT and PUREDEVICE **[enl]**. `DevCaps2 = STREAMOFFSET | VERTEXELEMENTSCANSHARESTREAMOFFSET` (`:46-72`, `:452-456`).
  - All D3D9 `DeclTypes`; `StretchRectFilterCaps` are point/linear min/mag only (`:466-483`).
  - `GuardBand` ±1e8, `MaxVertexW` 1e10 (`:357-365`).
- Caps types (`9on12/src/9on12Adapter.cpp:16-115`, `9on12/include/9on12Caps.h:558-1382`):
  - **D3D8 caps**: D3DCAPS9 truncated and masked to DX8-valid bits, VS 1.1 / PS 1.4.
  - **D3D7 / D3D3 caps** are derived tables. **DDRAW** reports ZBLTS, BLTDEPTHFILL, colorkey and mirror blts.
  - **GAMMARAMPCAPS**: RGB256x3x16.
  - **MULTISAMPLEQUALITYLEVELS**: NONE → 0, NONMASKABLE → 3, other counts from D3D12 `CheckFeatureSupport`.
  - **Queries**: VCACHE, EVENT, OCCLUSION, TIMESTAMP, TIMESTAMPDISJOINT, TIMESTAMPFREQ.
  - **Unknown types**: `DebugBreak(); E_FAIL`.
- FORMATOP list (`9on12/src/9on12Adapter.cpp:445-621`). The explicit table is ORed with ops derived from D3D12 format support, and formats D3D12 lacks are dropped (`:541-603`).
  - **RGB and float**: A8R8G8B8, **A8B8G8R8**, X8R8G8B8, R5G6B5, the 1555/4444/10:10:10:2 variants, 16-bit and float formats, L8/L16/A8/A8L8.
  - **Bump formats** have render-target ops suppressed.
  - **Block-compressed**: DXT1-5, **ATI1/ATI2** (`PrivateFormatBitCount = 8`, `:485-497`).
  - **Depth**: D16/D24S8/D24X8/D24FS8, plus **INTZ, DF16, DF24**.
  - **Capability markers**: **NULL** (texture and RT ops); **RESZ** and **ATOC** ("not a format you can create… reporting lets apps know", `:523-526`).
  - **Video**: AYUV/YUY2/NV12/YV12/P010.
  - **Cut on purpose**: RAWZ; P8/A8P8 are not listed (`:500-501`, `:518-519`).
  - **Multisample**: `FlipMsTypes`/`BltMsTypes` are bitmasks `1 << (type-1)` for 2/4/8 samples and NONMASKABLE (`:388-393`, `:609-610`). An oddity not to copy: a multisample bit is also ORed into `Operations` (`:585-590`).

### Device table (`9on12/include/9on12DDI.h:203-349`, `9on12/src/9on12DDIStub.cpp`)

- **Uses the newer entry points.** `pfnCreateResource`, `pfnPresent`, `pfnFlush`, `pfnTexBlt`, `pfnBufBlt` and `pfnVolBlt` are NULL; `pfnCreateResource2`, `pfnPresent1`, `pfnFlush1` and the `*Blt1` variants are used instead. `LockAsync`/`UnlockAsync`/`Rename` are real, and are nulled when single-threaded (`9on12/src/9on12Device.cpp:528-561`).
- **`S_OK` no-ops**: StateSet, SetTransform, MultiplyTransform, SetMaterial, Set/Create/DestroyLight **[enl]**, SetDisplayMode ("DXGI already seems to do the right thing") (`9on12/src/9on12DDIStub.cpp:33-115`, `9on12/src/9on12Device.cpp:150-161`).
- **`E_NOTIMPL`**: patches, GetInfo, ResolveSharedResource, SetPriority, palettes, overlays, capture, crypto, and the public OpenResource (`9on12/src/9on12DDIStub.cpp:9-587`, `9on12/src/9on12Resource.cpp:328-333`).
- **ValidateDevice** sets `NumPasses = 1` (`9on12/src/9on12Device.cpp:163-176`).
- Fixed function is not implemented: `SetVertexShaderFunc(NULL)` and `SetPixelShader(NULL)` return `E_INVALIDARG`, and every draw needs a PS (`9on12/src/9on12VertexStage.cpp:34-55`, `9on12/src/9on12PixelStage.cpp:21-42`, `:844-881`). The runtime must be supplying converted FF shaders **[enl]**.

### Resources and Lock (`9on12/src/9on12Resource.cpp`)

- `Format == D3DFMT_NULL` gets a NULL driver handle (`:271-275`).
- **Dimension**: VB/IB are buffers, width in bytes. `Depth > 1` makes a 3D texture; everything else is 2D (`9on12/include/9on12Util.h:1407-1434`).
- **Sub-resource layout**: `arraySize = SurfCount / max(MipLevels, 1)`, and `pSurfList` is array-major, mip-minor. A cube is a 6-slice array with index `face*Levels + mip`, the same face-major order VirtualBox uses (`:821-823`, `:929-952`, `:1287-1302`).
- **AUTOGENMIPMAP**: the full chain is allocated, but the app sees only level 0 (`:825-840`).
- **Offscreen plain** (no Texture/RT/ZBuffer flag) is an ordinary 2D resource. Every 2D texture is SRV-capable so it can be a StretchRect source (`:1007-1031`).
- **Primaries**: X8R8G8B8 is created as A8R8G8B8 with alpha disabled, because "X8R8G8B8 is not a DISPLAYABLE format in DX12" (`:791-799`).
- **SYSTEMMEM**: no GPU resource is created; `pSysMem`, `SysMemPitch` and `SysMemSlicePitch` are recorded (`:924-998`).
  - As a vertex or index source, the data is copied to an upload ring on each draw (`9on12/src/9on12InputAssembly.cpp:84-102`).
  - As a texture, it is copied into a backing resource when bound (`9on12/src/9on12Resource.cpp:1675-1713`).
- **Lock basics**:
  - `NotifyOnly` is the only lock expected on SYSTEMMEM, and returns `S_OK` without mapping (`:1323-1341`).
  - A recursive lock keeps the first lock's flags (`:1329-1339`).
  - Area, box and range are clamped to the resource, because "Validation … is debug-only in 9, so apps can pass whatever they want" (`:1387-1413`).
- **Lock map types and stalls**:
  - `Discard` maps to WRITE_DISCARD and `NoOverwrite` to WRITE_NOOVERWRITE on write-only resources; anything CPU-readable maps READWRITE (`9on12/include/9on12Util.h:2325-2370`).
  - `DoNotWait` makes a busy Map return `D3DERR_WASSTILLDRAWING` (`:1513-1517`).
  - `D3DLOCK_DISCARD` is legal only on level 0 and discards the whole resource ("Taken from the dx9 runtime") (`:1360-1363`).
  - `MightDrawFromLocked` is ignored (`:986-990`).

### State, draws, shaders (cross-checks against VirtualBox)

- **Render states** use API numbering, and the DDI-only values are stored but ignored (`SCENECAPTURE`) (`9on12/src/9on12PipelineState.cpp:61`, `:244-252`). Garbage values are clamped rather than passed on: fill/cull outside 1-3, booleans ≥ 2, blend ≥ 32 (`9on12/src/9on12PixelStage.cpp:318-351`, `:479-532`).
- **Magic render-state values carry vendor hacks**:
  - `POINTSIZE == 0x7FA05000` (RESZ) resolves the bound depth into sampler 0.
  - `POINTSIZE 'A2M1'/'A2M0'` and `ADAPTIVETESS_Y 'ATOC'` toggle alpha-to-coverage.

  (`9on12/src/9on12PipelineState.cpp:145-193`)
- **Texture-stage states**:
  - The same TSS/sampler split as VirtualBox; `State >= 35` is `E_INVALIDARG`; legacy `D3DTSS_ADDRESS (12)` sets both U and V (`9on12/src/9on12PipelineState.cpp:36-48`).
  - Bad filter, address-mode and anisotropy values are clamped "because some apps pass bad data … device removed" (`:285-380`).
  - Stage 256 maps to internal 16 (DMAP, writes ignored) and stages 257-260 to vertex samplers 17-20 (`9on12/include/9on12PipelineStateStructures.h:510-525`, `:275-280`).
- **Viewport, render targets and scissor**:
  - The viewport is merged with `SetZRange` at draw time: "The viewport must be deferred because they can set the z range independently" (`9on12/src/9on12VertexStage.cpp:160-199`).
  - SetRenderTarget does not touch viewport or scissor (`9on12/src/9on12PixelStage.cpp:258-311`), which matches VirtualBox's re-apply.
  - Scissor is applied only while `SCISSORTESTENABLE` is set (`9on12/src/9on12VertexStage.cpp:100-126`).
- **Draws**:
  - **DP/DIP**: same as the API (`9on12/include/9on12draw.inl:61-111`, `:462-508`).
  - **DP2**: `FirstVertexOffset` is a byte offset baked into stream 0 only (`:113-154`).
  - **DIP2**: `StartIndexOffset` is in bytes and the index buffer is a user pointer every call. **A negative `BaseVertexOffset` is the software-vertex-processing case**: "if a MinIndex is passed into the API, the D3D9 runtime generates a smaller vertex buffer that skips over the vertices below the MinIndex… the runtime passes a negative vertex offset" (`:156-224`, quoted `:178-189`). This explains the field's signedness, which VirtualBox does not.
  - **User pointers**: `SetStreamSourceUm`/`SetIndicesUm` pointers are copied at the next draw only, so they stay valid until that draw (`9on12/src/9on12InputAssembly.cpp:23-82`, `:429-484`).
  - **Index cut**: 0xFFFF is an ordinary index (`IBStripCutValue` disabled, `9on12/src/9on12PipelineState.cpp:445`).
  - **Fans and points**: triangle fans become generated or converted lists (`9on12/include/9on12draw.inl:226-460`). Point sprites and PSIZE use a generated GS (`9on12/src/9on12Shader.cpp:332-385`).
  - **Pre-transformed vertices**: `POSITIONT` declarations get a generated TL vertex shader (`9on12/src/9on12InputLayout.cpp:119-124`).
- **Shaders**:
  - Shader bytecode is the raw D3D9 token stream, size in bytes, version token first. Comment and NOP tokens are skipped by the parser, so they may be present (`9on12/src/9on12Shader.cpp:8-89`, `9on12/ShaderConverter/ShaderConv/vsconv.cpp:63-161`).
  - Vertex declarations are `NumVertexElements` long with **no terminator** (`9on12/src/9on12InputLayout.cpp:7-42`), which confirms VirtualBox's append.
  - The CreateDevice `Version` is the API version and changes shader semantics: inline constants need version ≥ 9, and fog is saturated below 9 (`9on12/src/9on12Device.cpp:535`, `vsconv.cpp:113-114`, `:1031-1033`).
- **Clear** clears every bound RT and the DSV with the DDI rect list (0 rects means the whole surface), ignoring viewport and scissor; `D3DCLEAR_COMPUTERECTS` is never referenced (`9on12/src/9on12Resource.cpp:402-440`). It honors `SRGBWRITEENABLE` (`:1212-1234`).
- **Blt** never reads `Flags`, so filter, mirror, rotate and PresentToDwm are all ignored. It requires `ColorKey == 0`, uses a copy for same size and a blit shader otherwise, and resolves MSAA → 1 (`9on12/src/9on12Blit.cpp:683-739`, `:172-228`, `:585-679`).
- **TexBlt1** copies **all mips of the face** from `SrcRect`/`DstPoint`. "DX9 allows uploading from a src with different mip levels" (`:363-370`, `:741-786`, `:841-874`).
- **Other fills and copies**:
  - **DepthFill** clears depth only (`9on12/src/9on12Resource.cpp:240-260`).
  - **GenerateMipSubLevels** uses point or linear filtering (`:522-544`).
  - **ColorFill** on a NULL resource is a no-op (`:363-400`).
- **Present1** never reads `D3DDDI_PRESENTFLAGS`. A non-NULL `hDstResource` means a blt-model present into that resource; NULL means a window or flip present (`9on12/src/9on12SwapChain.cpp:8-168`). **[enl]** It calls the runtime's `pfnPresentCb` with an empty argument and gets the real present back through a private callback.
- **Queries**:
  - GetQueryData has no flags in the DDI and returns `S_FALSE` while not ready, never `WASSTILLDRAWING`.
  - Result sizes: EVENT is a BOOL, OCCLUSION a DWORD, FREQ a UINT64. TIMESTAMP is truncated to a DWORD, which looks like a bug.

  (`9on12/src/9on12Query.cpp:132-289`)
- **Shared resources** use KMT (GDI-style) handles because "The DX9 runtime only works with GDI handles" (`9on12/src/9on12Resource.cpp:1077-1101`).

### Format semantics D3D9 apps depend on (`9on12/include/9on12Util.h`)

- Missing channels read as 1, and A8 reads (0,0,0,A).
- **Depth textures sample as (d,1,1,1)**: "games depend on this behavior (most notably is Diablo III)" (`:1319-1329`).
- L8 and A8L8 use swizzles.
- A2R10G10B10 and A2B10G10R10 share one DXGI format, with R/B fixed up everywhere (`:1059-1074`, `:1223-1345`).
- **Hardware shadow mapping**: D16/D24X8/D24S8 bound as textures turn samples into `SampleCmp LESS_EQUAL`, while INTZ/DF16/DF24 sample raw depth (`9on12/src/9on12PixelStage.cpp:88-107`, `9on12/src/9on12Resource.cpp:2111-2123`).
- The D3D9 half-pixel offset is emulated in every converted VS (`9on12/ShaderConverter/ShaderConv/vsconv.cpp:989-1026`). DXVK does this itself for us.

Bugs not to copy: GetFormatData writes before checking capacity, a stream-upload loop increments twice, `TIMESTAMP` is truncated, `VolBlt1` always returns `S_OK`, and the render-state index is not bounds-checked (`9on12/src/9on12Adapter.cpp:605-613`, `9on12/src/9on12InputAssembly.cpp:448-463`, `9on12/src/9on12Query.cpp:248`, `9on12/src/9on12Blit.cpp:788-812`, `9on12/src/9on12PipelineState.cpp:61`).

---

## What transfers to accel-virt

1. **VirtualBox's shape is ours.** Forward each DDI call to an `IDirect3DDevice9Ex`, use the API object as the DDI handle, create the backend device lazily, and create WDDM allocations only where dxgkrnl must see one. Our handles are host ids rather than pointers; the rest carries over. Unlike VBox, every resource that Present, OpenResource or a sharing path can name needs a KMD allocation that carries the device-global host id (d3d-desktop note §1).
2. **Caps.** Take `GetDeviceCaps` from DXVK on the host and OR in what the runtime requires:
   - the four flags in §11.15;
   - VBox's 9_3 list (`gw/VBoxDispD3DIf.cpp:379-481`).

   Report a FORMATOP list closer to 9on12's than VBox's. Source engine games probe FOURCC depth formats (INTZ/DF24/DF16), NULL, ATI2 and multisample types; VBox advertises none of these. DXVK supports all of them. The table should come from host `CheckDeviceFormat` (per format and usage), not from a static list.
   - Fill `FlipMsTypes` and `BltMsTypes` from `CheckDeviceMultiSampleType`.
   - Answer GETMULTISAMPLEQUALITYLEVELS from the host as well.

   Whether Portal actually probes these formats is inferred from the engine family, not shown in any source read here.
3. **Map entry points and argument numbering explicitly; never cast where the DDI differs from the API.**
   - Translate sampler states through a table, as in §5 (VBox) and `9on12/src/9on12PipelineState.cpp:256-432`.
   - Filter or drop DDI-only render states before forwarding. DXVK tolerates them, but they waste wire bytes.
   - Merge the viewport with ZRange.
   - Re-apply the viewport and scissor after SetRenderTarget.
   - Append `D3DDECL_END`.
4. **Resources.**
   - Flags priority: Texture/0 → texture; CubeMap → cube (face-major sub-resources); Volume; RT/Primary → `CreateRenderTarget`; ZBuffer; VB/IB.
   - `Flags.Value == 0` (offscreen plain) maps to `CreateOffscreenPlainSurface`. That keeps StretchRect, ColorFill and Lock legal on DXVK, where VBox's texture choice relied on Nine's leniency.
   - MANAGED and SCRATCH never reach the driver.
   - SYSTEMMEM resources stay in the guest: a guest pointer cannot be shared with the host, so VBox's alias-the-memory trick does not apply. Upload them on TexBlt/Blt/UpdateSurface or at the draw that reads them, following 9on12 (`9on12/src/9on12Resource.cpp:997-998`, `9on12/src/9on12InputAssembly.cpp:84-102`), and use `SysMemPitch` for row stride.
5. **Lock** is the hardest part for a remote backend. VBox could return the backend's pointer; we cannot.
   - DEFAULT-pool buffers and lockable surfaces need guest-visible staging. `Discard` and `NoOverwrite` give the renaming and append semantics that let dynamic VB/IB updates avoid stalls.
   - `ReadOnly` locks (and `GetRenderTargetData`-like Blts to SYSTEMMEM) need a host readback.
   - `DoNotWait` must return `D3DERR_WASSTILLDRAWING` rather than block.
   - Clamp lock regions.
   - Handle NotifyOnly locks with SubResourceIndex 0 as "the whole resource".
6. **Draws.**
   - DP and DIP map 1:1.
   - For the UP and `*2` paths, ship only the referenced vertices `[MinIndex, MinIndex+NumVertices)` and rebase indices, as VBox v6.0 does.
   - Treat a negative `BaseVertexOffset` as the software-vertex-processing compaction it is.
   - Re-bind stream 0 and the IB after any `*UP` call on the host.
   - Return `S_OK` for `PrimitiveCount == 0`.
   - Support more than one user-pointer stream; VBox draws nothing in that case.
7. **Shaders.** Pass the token stream through unchanged. DXVK accepts 2.x instructions in 2.0 shaders, as far as this reading shows, so VBox's version bump is Gallium-specific; keep it in mind if validation fails. Hash and deduplicate on the guest, as 9on12 does, so each shader crosses the wire once.
8. **Blt.**
   - DEFAULT → DEFAULT: StretchRect with the Point/Linear filter; ignore the PresentToDwm bits.
   - SYSTEMMEM → DEFAULT: an upload.
   - DEFAULT → SYSTEMMEM: a readback.
   - SYSTEMMEM → SYSTEMMEM: no copy at all (§11.13).
   - TexBlt copies all mips of the face from `SrcRect`/`DstPoint` (9on12). VBox's level-0-only fallback is incomplete.
   - Implement GenerateMipSubLevels on the host if any format advertises AUTOGENMIPMAP.
9. **Present.**
   - Flush, then `pfnPresentCb(src, dst, hContext)`; `Flags` can be ignored, as both drivers do.
   - Windowed Portal under DWM reaches us as a blt-model present (`hDstResource` = the redirection surface or NULL). It should become the host-to-host copy that `docs/research/d3d-desktop/prior-art.md` "What transfers" item 3 describes.
   - No swapchain reconstruction (VBox v6.0's layer) is needed, because the host never needs a real `IDirect3DSwapChain9`.
10. **Queries.** Advertise at least EVENT and OCCLUSION (Source uses both). TIMESTAMP is optional. GetQueryData returns `S_FALSE` until the host answers; never block.
11. **StateSet and fixed function.** Both open drivers no-op or fail these, and neither shows what d3d9.dll does in that case. Portal is shader-only, so this is not on the critical path. Test whether the runtime emulates state blocks itself before implementing StateSet.

## Not established

- What the non-enlightened d3d9.dll does when a UMD returns `E_NOTIMPL` or `S_OK` for StateSet, SetTransform, SetLight or SetMaterial: whether it emulates state blocks and converts fixed function itself. Neither open driver shows it.
- Exactly when the runtime chooses DrawPrimitive2/DrawIndexedPrimitive2 over DrawPrimitive/DrawIndexedPrimitive, beyond software vertex processing and 9on12's multithreading comment (`9on12/src/9on12InputAssembly.cpp:337-338`).
- Whether the runtime expects an error rather than `S_OK` for unknown GetCaps types (VBox returns `S_OK`, 9on12 `E_FAIL`).
- How the runtime builds the adapter's mode list from FORMATOP_DISPLAYMODE formats; there is no driver code for it in either tree.
- Which FOURCC formats and query types Portal actually probes. The recommendations above rest on the engine family, not on a capture; a guest trace of `CheckDeviceFormat`/`CreateQuery` calls would settle it.
