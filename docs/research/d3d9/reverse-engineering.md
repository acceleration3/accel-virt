# Direct3D 9 runtime (d3d9.dll) and the D3D9 UMD: what the Microsoft binary requires

Binaries: `re/d3d9/d3d9_x86.dll` (SysWOW64, the copy a 32-bit game such as Portal loads), `re/d3d9/d3d9_x64.dll`
and `re/d3d9/d3d9on12_x86.dll`, all pulled from the Win11 guest (build 26200.8037). The public PDBs were fetched by
the GUID/age in each debug directory (`d3d9.pdb` 5DEC80FF1E3B2F14DEBAAA6F9A22918F/1 for x86,
268A5B5B3AC8301C2D0E1D44FBD8E1E7/1 for x64) and saved beside each DLL. All three are imported, with symbols, into
the Ghidra project `re/d3d9-proj/d3d9`.

Every address below is in **d3d9_x86.dll** (image base 0x10000000) unless it says otherwise. The x64 build has the
same structure: `CreateDeviceLHDDI` @18006ce44 holds the same OpenAdapter version table, and `ValidateUMDeviceFuncs`
@18006e4e0 performs the same slot checks.

"Verified" means the decompiled or disassembled condition was read. "Inferred" means it was not, or the meaning of a
runtime-internal field was deduced from how the code uses it. Structure offsets inside the runtime are the runtime's
own. DDI slot offsets are offsets into `D3DDDI_DEVICEFUNCS` (x86, 4 bytes per slot, taken from
`third_party/windows-kit/include/um/d3dumddi.h`). The runtime keeps its copy of the UMD's `D3DDDI_DEVICEFUNCS` at
`DDobj+0x6a0` and its copy of `D3DDDI_ADAPTERFUNCS` at `DDobj+0x8dc`, so runtime offset `0x6a0+N` is device-funcs
slot `N`.

---

## Summary: what a minimal but correct D3D9 UMD must answer

**Adapter open**
- Export `OpenAdapter`.
- `OpenAdapter` must accept `Interface == 9`. The runtime tries `Version` = 0x11000 first and then walks down a
  table of older versions; the UMD returns success on the first one it accepts.
- On return it must set `hAdapter` and `pAdapterFuncs->{pfnGetCaps, pfnCreateDevice, pfnCloseAdapter}`.
- It must also set `DriverVersion`. That value is the runtime's main switch (§1.4):
  - DriverVersion ≥ 2: the runtime creates no GPU context. The UMD creates its own and passes `hContext` to
    `pfnPresentCb` and `pfnRenderCb`.
  - DriverVersion > 0x4001: `pfnCreateResource2`, `pfnBufBlt1`, `pfnTexBlt1` and `pfnVolBlt1` must be non-NULL, and
    `pfnCreateResource`, `pfnBufBlt`, `pfnTexBlt` and `pfnVolBlt` **must be NULL**. Otherwise `pfnCreateDevice` is
    treated as a failure.
  - DriverVersion > 0x6000: `pfnAcquireResource` and `pfnReleaseResource` must be non-NULL.
- **`pfnCreateDevice` runs before any `pfnGetCaps`, and it already runs inside `Direct3DCreate9`.**

**`pfnGetCaps`**

The types below are asked for every DirectDraw-object the runtime creates. That is at least one object per adapter
at `Direct3DCreate9`, and more at `CreateDevice`.

| Type | DataSize | Required? |
|---|---|---|
| GETD3D9CAPS (13) | 0x130 | mandatory |
| GETFORMATCOUNT (3) | 4 | mandatory |
| GETD3DQUERYCOUNT (6) | 4 | optional |
| GETGAMMARAMPCAPS (34) | 4 | only if the adapter is render-capable; optional |
| GETFORMATDATA (4) | count×0x14 | mandatory |
| GETD3DQUERYDATA (7) | count×4 | mandatory if the count was non-zero |
| GETMULTISAMPLEQUALITYLEVELS (5) | 0x10 | only from CheckDeviceMultiSampleType with a non-NULL pQualityLevels |

DDRAW (1) and DDRAW_MODE_SPECIFIC (2) are never asked for.

**D3DCAPS9 must pass the runtime's HAL validation** (§2.3). For a Shader Model 2.0 part these are hard requirements:
- `DevCaps2 & D3DDEVCAPS2_STREAMOFFSET`
- `Caps2 & D3DCAPS2_DYNAMICTEXTURES`
- `RasterCaps & D3DPRASTERCAPS_SCISSORTEST`
- `NumSimultaneousRTs` in 1..4
- `MaxVertexIndex ≥ 0xFFFE`
- `MaxStreams ≥ 8` (vs_2_0)
- `TextureCaps != 0` and `FVFCaps != 0`
- `PrimitiveMiscCaps & 0x2000` (an undocumented bit, see §2.3)
- `PrimitiveMiscCaps` must not have `LINEPATTERNREP` (4)
- the VS20Caps and PS20Caps limits must lie in their legal ranges
- `D3DVTXPCAPS_NO_VSDT_UBYTE4` must be clear
- every shader-3.0-only field must be 0 below 3.0

**The format list** (§2.4) must have:
- `D3DFMT_X8R8G8B8` with `DISPLAYMODE|3DACCELERATION` (0xC00);
- render-target ops on the backbuffer format;
- **no** `3DACCELERATION` on any alpha format (A8R8G8B8, A1R5G5B5);
- `DISPLAYMODE` never without `3DACCELERATION`;
- for every multisample mask that is non-zero, bit 0 (NONMASKABLE) set.

**Device functions**
- **There is no NULL-tolerance for the core slots** (§3.3). Every slot from `pfnSetRenderState` through
  `pfnDestroyDevice`/`pfnOpenResource` is called through unconditionally, the first time an API path reaches it.
- Only the video, overlay, content-protection, async-lock and WDDM1.3 groups are checked for NULL.
- `pfnStateSet` has to be real, unless the user sets the `EmulateStateBlocks` registry value (§4.4). The runtime
  records each state block into the driver with BEGIN/END and later only sends EXECUTE.

**What the runtime does by itself**
- It **emulates the fixed-function pipeline**. Fixed-function vertex work becomes generated vs_2_0 shaders when the
  driver reports VS ≥ 2.0 and HW T&L; fixed-function pixel work becomes generated ps_2_0 shaders when PS ≥ 2.0.
- It **emulates software vertex processing** and owns **D3DPOOL_MANAGED**: it clears `D3DCAPS2_CANMANAGERESOURCE`
  and uploads with TexBlt and BufBlt.
- A windowed Present is a blt. The runtime calls `pfnFlush` and then `pfnPresent` with `Flags.Blt`. The UMD calls
  `pfnPresentCb` with its `hContext` and `hSrcAllocation`, and the runtime builds the whole `D3DKMT_PRESENT` itself.

**Top risks** (§6):
1. With a UMD name present there is **no fallback to D3D9On12**. Any later failure (load, OpenAdapter, CreateDevice,
   caps validation, format validation) leaves the adapter without a HAL, and `CreateDevice(HAL)` fails.
2. Getting the old/new pairs in `ValidateUMDeviceFuncs` wrong makes `pfnCreateDevice` fail.
3. One wrong FORMATOP bit disables the whole driver: an alpha format with 3DACCELERATION, both R5G6B5 and X1R5G5B5 as
   display modes, or a reversed depth format next to its normal twin.
4. Several UMD adapter/device instances exist per process: enumeration, CheckDeviceMultiSampleType, ValidateCreateDevice
   and CreateDevice each create one.
5. A windowed device also creates a **Primary** resource. That resource may be opened through `pfnOpenResource` on the
   kernel's shared GDI primary.

---

## 1. Adapter open: UMD or D3D9On12

### 1.1 Who decides (`CEnum::CEnum` @100586b6)
`Direct3DCreate9` → `Direct3DCreate9Impl` @10059524 → `CEnum::CEnum` @100586b6. For each display device found by
`EnumDisplayDevicesA`, the constructor runs these steps in order:
1. `_D3D9GetAdapterHandle` (KMT adapter handle and LUID).
2. `CEnum::Get9on12OverrideSettingsForAdapter` @100596a3. This only returns something when the app called
   `Direct3DCreate9On12(Ex)` with `D3D9ON12_ARGS` for this LUID (matching done in the cold part @1009bdb3).
3. Otherwise `_D3D9RequestsD3D9on12(hAdapter)` @10058f19. Its result becomes `D3D9ON12_ARGS.Enable9On12` for the
   adapter, stored at adapter-info +0x2c and reference-counted by `_AddRefD3D9on12Args`.
4. `_InternalDirectDrawCreate` @100576b2 for the adapter. **This already opens the UMD and creates a UMD device**
   (§1.3).
5. `GetDX8HALCaps` @10058f8b → `IsD3DHALSupported` @10059077, which validates the caps (§2.3).

`CEnum::ReparentDevice` @100b6506 (hybrid-GPU reparenting, reached through `CEnum::AdjustEnumForReparenting` @10059f1a)
is the only other caller of `_D3D9RequestsD3D9on12`.

### 1.2 `_D3D9RequestsD3D9on12` @10058f19 (verified in disassembly)
```
switch (g_Force9On12 /*@10161264*/) {
  case 0:  return GetUMDFileName(hAdapter, {Version=KMTUMDVERSION_DX9}) == 0x80070002;   // auto
  case 1:  return 1;                                                                     // forced 9on12
  default: return 0;                                                                     // forced native
}
```
- `g_Force9On12` is written only by the export `Direct3D9Force9on12` @100becc0 (ordinal 19), which accepts 0, 1 or 2.
  It is an app-compat/shim hook. Nothing inside d3d9.dll calls it.
- `_GetUMDFileName` @10054fc6 calls `D3DKMTQueryAdapterInfo(KMTQAITYPE_UMDRIVERNAME=1, size 0x20c)` with a
  `D3DKMT_UMDFILENAMEINFO`. It maps the status as follows:
  - `STATUS_INVALID_PARAMETER` (0xC000000D) → 0x80070002. This is the only "use 9on12" answer.
  - `STATUS_DEVICE_REMOVED` (0xC00002B6) → 0x88760870.
  - any other error → E_FAIL.
  - success → 0, and the name is NUL-terminated at +0x20a.
- **So 9on12 is chosen only when the kernel answers STATUS_INVALID_PARAMETER for the DX9 name**, as it does for
  `"<>"` (see `src/km/avgpu.inf`). Any other failure does not pick 9on12. It just leaves the adapter without a HAL,
  through §1.3.
- Separately, `_LoadLHDDIThunks` @100599f7 sets the global `g_bIsLHDriver` ("WDDM adapter"). It first queries the DX9
  name and, if that fails, the KMTUMDVERSION 3 (DX12) name, so a `"<>"` DX9 entry still counts as WDDM.
- No other registry, app-compat or feature check in this chain overrides a present UMD name. The
  `DisplayMuxCompatManager` / `Feature_DisplayMux_MarkD3D9Incompatible` code and the GameDVR
  maximized-windowed shim are about fullscreen and mux handling and do not touch `Enable9On12`. **Inferred**: those
  paths were only skimmed. Hybrid reparenting re-runs the same decision for the new adapter.

### 1.3 Loading the UMD and calling OpenAdapter (`_CreateDeviceLHDDI` @10053cfc, verified)
Every runtime "DirectDraw object" runs the whole sequence below:
`_InternalDirectDrawCreate` @100576b2 → `_FetchDirectDrawData` @100504f5 → `_D3D9CreateDirectDrawObject@88`
@10053a6c → `_CreateDeviceLHDDI`.

1. `DisplayConfigGetDeviceInfo(type 0xFFFFFFFE)` makes a private query. It returns a vendor id at +4 and a driver
   model number at +0x18; the number defaults to 1000 when the call fails.
2. `D3DKMTQueryAdapterInfo(KMTQAITYPE_CHECKDRIVERUPDATESTATUS=0xb)`. A driver update in progress fails with 0x88760870.
3. `D3DKMTCreateDevice` (the runtime's kernel device). Failure → "OsThunkDDICreateDevice failed".
4. `_ChooseLatestOrFallbackVersionIndex` @10054f46 picks where the version walk starts:
   - index 15 (0x3004) when the driver model number is 0x4b0 and the vendor is 0x1002 (AMD) or 'QCOM';
   - index 0 otherwise.

   The version table, in descending order:
   `0x11000, 0x10000, 0xf000, 0xe000, 0xd000, 0xc001, 0xb003, 0xa002, 0x9001, 0x8001, 0x7001, 0x6003, 0x5002, 0x5001, 0x4002, 0x3004, 0x2003`.
5. `_GetUMDFileName` again, this time from `CreateDeviceLHDDI`. Failure → "Could not find the load user mode driver
   name" and the device fails.
6. `LoadLibraryExW(name, NULL, LOAD_WITH_ALTERED_SEARCH_PATH)`. On `ERROR_INVALID_PARAMETER` (a relative path) the
   runtime retries with `LoadLibraryW`. Failure → "Could not load user mode driver DLL".
7. `GetProcAddress(hUmd, "OpenAdapter")`. If it is missing → "Could not find function OpenAdapter in user mode driver
   DLL". The 9on12 path instead uses `d3d9on12.dll`, `GetPrivateDDITable` and `_OpenAdapter9on12@16` @100e479b.
8. The OpenAdapter loop, from the start index to the end of the table:
   ```
   D3DDDIARG_OPENADAPTER a = { .hAdapter = KMT adapter handle (DDobj+0xb70),
                               .Interface = 9, .Version = table[i],
                               .pAdapterCallbacks = &{ QueryAdapterInfoCB @10055040,
                                                       GetMultisampleMethodListCB @100c0330,
                                                       QueryAdapterInfoCB2 @100c2680 },
                               .pAdapterFuncs = DDobj+0x8dc, .DriverVersion = 0 };
   if (SUCCEEDED(OpenAdapter(&a))) break;     // first success wins
   ```
   On success the runtime stores:
   - `a.hAdapter` (the driver's handle) at DDobj+0xb30. It is passed as `hAdapter` to every `pfnGetCaps` and
     `pfnCreateDevice` call.
   - `a.DriverVersion` at DDobj+0x11dc.

   If every version fails, the device fails.

### 1.4 What `DriverVersion` switches (verified unless marked)
- **`< 2`**: the runtime calls `D3DKMTCreateContext` itself and hands the context's command buffer, allocation list and
  patch list to `pfnCreateDevice`.
  **`≥ 2`**: no context is created, and those `D3DDDIARG_CREATEDEVICE` fields are 0 (`CreateDeviceLHDDI` @10053cfc).
  `PresentCB` @10065de0 then takes `hContext` from the UMD's `D3DDDICB_PRESENT.hContext` rather than from the
  runtime's default context.
- **`≥ 0x3003` with `pfnCreateResource2`, or `≥ 0x4001`**: resources are created through `pfnCreateResource2`
  (`_CreateSurfaceLH` @1003e880).
  **`< 0x4001`**: `pfnTexBlt` and `pfnBufBlt`; at or above it, `pfnTexBlt1` and `pfnBufBlt1`
  (`CD3DDDIDX10::TexBlt` @10065770, `CD3DDDIDX10::BufBlt` @1013d450).
- **`ValidateUMDeviceFuncs` @100569ce**, called right after a successful `pfnCreateDevice`.
  `_IsDDIVersioningInvalid` @10056aae accepts a pair only when the **old slot is NULL and the new slot is non-NULL**:
  - `DriverVersion > 0x4001`: (BufBlt 0x44, BufBlt1 0x1e8), (TexBlt 0x48, TexBlt1 0x1ec), (VolBlt 0x40, VolBlt1 0x1e4),
    (CreateResource 0x94, CreateResource2 0x200).
  - If the "WDDM1.3 present path" flag (DDobj+0x11e0, see below) is set: (Flush 0xa4, Flush1 0x210) and
    (Present 0xa0, Present1 0x220).
  - `DriverVersion > 0x6000`: `pfnAcquireResource` (0x234) and `pfnReleaseResource` (0x238) must be non-NULL
    ("UMD failed to provide WDDM2.1 synchronization DDIs").
  - Any violation → E_FAIL → "UMD failed CreateDevice in CreateDeviceLHDDI" and the device is destroyed.
- **DDobj+0x11e0** ("use Flush1/Present1") is set only when all of these hold:
  - `DriverVersion ≥ 0x4001`;
  - `pfnCheckCounterInfo` (0x214), `pfnCheckCounter` (0x218), `pfnUpdateSubresourceUP` (0x21c), `pfnFlush1` (0x210)
    and `pfnPresent1` (0x220) are all non-NULL;
  - when `DriverVersion > 0x4001`, `pfnSetMarker` (0x228) and `pfnSetMarkerMode` (0x22c) are non-NULL as well.

  If any is missing, the runtime silently keeps calling `pfnFlush` and `pfnPresent` (`_Present` @1004c92a,
  `_Flush` @1004c9bf).
- **`> 0x5001`**: `PresentCB` copies the extra WDDM2 fields of `D3DDDICB_PRESENT`.
  **`> 0x6000`**: Present waits and signals through the sync DDIs (`_DoPrePresentWaitViaSyncDDI`).
  **`> 0x7000`**: `PresentCB` also reads the newest fields (`PresentCB` @10065de0).

**Practical consequence.** A UMD that answers `DriverVersion = 0x2003` (`D3D_UMD_INTERFACE_VERSION_WIN7`) gets the
simplest contract:
- old CreateResource, Blt and Present slots;
- no Acquire/Release;
- no Present1.

It still has to create its own context (`≥ 2`). Answering a WDDM2.x or 3.x version is allowed but requires the
"1"-suffixed slots with their old twins left NULL, plus Acquire/Release.

### 1.5 After pfnCreateDevice (`CreateDeviceLHDDI`, verified)
Kernel queries the device needs to succeed (dxgkrnl answers them, not the UMD):

| Query | Size | Failure |
|---|---|---|
| GETSEGMENTSIZE (3) | 0x18 | fatal |
| FLIPQUEUEINFO (5) | 0xc | tolerated |
| 0x1e | 4 | tolerated |
| ADAPTERTYPE (0xf) | 4 | fatal |
| WDDM_1_2_CAPS (0x11) | 0xc | fatal |
| WDDM_1_3_CAPS (0x16) | 4 | fatal |
| 0x39 | 4 | tolerated |

`EnumDisplayMonitors` must also find a monitor for the adapter's display name, otherwise E_FAIL (@1005491a). A
render-only adapter with no monitor gets no D3D9 HAL.

---

## 2. pfnGetCaps: what is asked, when, and what is fatal

### 2.1 Where the calls are
Every `DDobj+0x8dc` (pfnGetCaps) call site in the binary:
- `_QueryLHDDICaps` @10051a83, reached from `_D3D9QueryDirectDrawObject@64` @10051804;
- `_D3D9GetDriverInfo2@20` @1007e9db;
- `_CanRestoreNowLH` @100b733b;
- `CD3DDDIDX10::UpdateDriverInfo` @10082760 (it only copies the pointer);
- the DXVA, overlay and content-protection helpers.

None of them uses `D3DDDICAPS_DDRAW` (1) or `DDRAW_MODE_SPECIFIC` (2).

### 2.2 `_QueryLHDDICaps` @10051a83 (verified)
`_FetchDirectDrawData` @100504f5 calls `_D3D9QueryDirectDrawObject` twice per DirectDraw object:
- **pass 1** with no format or query buffer, to get the counts;
- **pass 2** with buffers allocated from those counts.

Each pass issues, in order:

| # | Type | DataSize | pData | On failure |
|---|---|---|---|---|
| 1 | GETD3D9CAPS (13) | 0x130 | the runtime's D3D9_DRIVERCAPS (D3DCAPS9 at offset 0) | "UMD failed GetCaps call in QueryLHDDICaps", object gets no HAL |
| 2 | GETFORMATCOUNT (3) | 4 | UINT | same, fatal |
| – | (kernel `_GetDisplayModeList`) | | | fatal unless a remote session |
| 3 | GETD3DQUERYCOUNT (6) | 4 | UINT | tolerated, count = 0 |
| 4 | GETGAMMARAMPCAPS (34) | 4 | UINT | only if KMT ADAPTERTYPE.RenderSupported; tolerated. Bit 0 sets D3DCAPS2_FULLSCREENGAMMA |
| 5 | GETFORMATDATA (4), pass 2 only | count×0x14 | FORMATOP[] | fatal |
| 6 | GETD3DQUERYDATA (7), pass 2 only | count×4 | D3DDDIQUERYTYPE[] | fatal |

What the runtime does with the D3D9CAPS answer straight away (all in `_QueryLHDDICaps`):
- It overwrites `Caps`, `Caps2`, `PresentationIntervals` and `CursorCaps` with its own values. It ORs runtime bits
  into `Caps` (0x80000000) and `Caps2` (0x84408800, plus 0x200000 when the kernel FLIPQUEUEINFO reports a queue).
- It **clears `D3DCAPS2_CANMANAGERESOURCE`** (`Caps2 &= 0xefffffff`). The runtime always manages D3DPOOL_MANAGED
  itself (§5).
- It **fails the caps** when `VertexProcessingCaps & 0x80` (`D3DVTXPCAPS_NO_VSDT_UBYTE4`) is set. The message is
  "DX9 Driver should not use D3DVTXPCAPS_NO_VSDT_UBYTE4 …".
- It marks the driver as DX9 (driver-caps +0x15c = 2).

Each FORMATOP becomes a 0x6c-byte runtime record:
- +0x50 Format
- +0x58 Operations
- +0x54 PrivateFormatBitCount
- +0x5c FlipMsTypes (WORD)
- +0x5e BltMsTypes (WORD)

`D3DFORMAT_OP_PIXELSIZE` formats with a bit count are registered with `CPixel::Register`.

`GETMULTISAMPLEQUALITYLEVELS` (5, DataSize 0x10, `DDIMULTISAMPLEQUALITYLEVELSDATA{Format, Flip, MsType, QualityLevels}`)
is requested only by `D3D9GetDriverInfo2` type 0x16, from `CEnum::CheckDeviceMultiSampleType` @1004b000. It happens
only when the app passes `pQualityLevels`, and only after the mask check below passes. To ask, the runtime creates
**another** DirectDraw object (another OpenAdapter/CreateDevice) and caches it at CEnum+0x4dd4.
`CanRestoreNowLH` @100b733b repeats GETFORMATCOUNT and GETFORMATDATA after a device loss and needs the display format
to carry `D3DFORMAT_OP_3DACCELERATION` (0x800).

### 2.3 D3DCAPS9 validation that decides whether the adapter has a HAL
`_FetchDirectDrawData` @100504f5 runs on every object. It sets Caps = 0x2000000, its internal "no HAL" marker, or
deletes the object, when any of these holds:
- `DevCaps2 & D3DDEVCAPS2_STREAMOFFSET` is clear ("DX9 Driver did not set D3DDEVCAPS2_STREAMOFFSET!").
- The query list contains TIMESTAMP (10), TIMESTAMPDISJOINT (11) and TIMESTAMPFREQ (12) only partly. The three must
  come together or not at all (`VerifyCorrectTimestampSupport` @10076485).
- `Caps3 & 0x40` ("DX9 drivers should not set D3D3CAPS_MANAGEDDEVICE").

It also adjusts:
- `MaxPointSize == 1.0` → 0.
- Two-sided stencil is cleared without `D3DPMISCCAPS_CULLNONE`.
- Many fields are masked to legal bits.

`IsD3DHALSupported` @10059077 then runs from `GetDX8HALCaps` @10058f8b in `CEnum::CEnum`. A 0 return registers
DirectDraw-only caps, so there is no HAL device. Offsets are D3DCAPS9 offsets. Conditions for a DX9 driver
(MaxStreams ≠ 0, DevCaps2 ≠ 0):

**Always required**
- `TextureCaps != 0`
- `FVFCaps != 0`
- `MaxVertexIndex ≥ 0xFFFE`
- if `DevCaps & NPATCHES`, `MaxNpatchTessellationLevel ≥ 1.0`
- `Caps2 & D3DCAPS2_DYNAMICTEXTURES`
- `1 ≤ NumSimultaneousRTs ≤ 4`
- `RasterCaps & D3DPRASTERCAPS_SCISSORTEST`
- `PrimitiveMiscCaps & 4` (`LINEPATTERNREP`) **must be clear**
- if VS < 3.0, `MaxVertexShader30InstructionSlots == 0`
- if PS < 3.0, `MaxPixelShader30InstructionSlots == 0`
- for devices that are not REF: every format record with a non-zero FlipMsTypes or BltMsTypes must have bit 0
  (D3DMULTISAMPLE_NONMASKABLE) set in that mask.

**VS ≥ 2.0 or PS ≥ 2.0**
- `PrimitiveMiscCaps & 0x2000`. The bit is not defined in the vendored `d3d9caps.h`; d3d8caps.h calls it
  `D3DPMISCCAPS_FOGINFVF`.
- If PS ≥ 2.0, `TextureCaps` must not be POW2 without NONPOW2CONDITIONAL.

**VS ≥ 2.0**
- `VertexProcessingCaps & 0x80` clear ("HAL Disabled: VS2.0+ hardware must not set D3DVTXPCAPS_NO_VSDT_UBYTE4").
- `MaxStreams ≥ 8` ("HAL Disabled: VS2.0+ hardware must support at least 8 vertex streams").
- `VS20Caps.DynamicFlowControlDepth ≤ 24`, `12 ≤ NumTemps ≤ 32`, `1 ≤ StaticFlowControlDepth ≤ 4`.

**PS ≥ 2.0**
- `PS20Caps.DynamicFlowControlDepth ≤ 24`, `12 ≤ NumTemps ≤ 32`, `StaticFlowControlDepth ≤ 4`,
  `96 ≤ NumInstructionSlots ≤ 512`.

**VS ≥ 3.0** (also needed if you advertise it)
- 512..32768 instruction slots
- VS20 caps at their maximums and `PREDICATION`
- all four guard bands ≥ 8192
- `MaxVertexShaderConst ≥ 256`
- `FOGVERTEX`
- `VertexTextureFilterCaps` with MIN and MAG point
- `FVFCaps & PSIZE`
- `DevCaps2 & VERTEXELEMENTSCANSHARESTREAMOFFSET`
- `DeclTypes & 0x30f`
- 6 clip planes when there is more than one RT

**PS ≥ 3.0** carries a longer list: texture size ≥ 4096, repeat ≥ 8192, anisotropy ≥ 16, full blend, compare and
filter caps, and so on.

Shader Model 2.0 is the safest target for Portal. It avoids the 3.0 lists entirely.

`CEnum::GetAdapterCaps` @10055281 and `CEnum::FillInCaps` @1005661e then serve `GetDeviceCaps` from this cache **with
no driver call**:
- They mask `Caps`, `Caps2` and `Caps3`.
- `PresentationIntervals` = ONE | IMMEDIATE, plus TWO..FOUR when the kernel flip-queue info allows it.

### 2.4 The FORMATOP list (`_FetchDirectDrawData` @100504f5, verified)
These are hard "driver disabled" errors. The object is deleted, so there is no HAL:
- `(ops & 0xC00) == 0x800`: 3DACCELERATION without DISPLAYMODE.
- `ops & 0x800` on D3DFMT_A8R8G8B8 (0x15) or A1R5G5B5 (0x19): an alpha format with 3DACCELERATION.
- R5G6B5 with DISPLAYMODE while X1R5G5B5 also has DISPLAYMODE.
- Both members of D15S1/S1D15, D24S8/S8D24, D24X8/X8D24 or D24X4S4/X4S4D24 listed together.
- `MEMBEROFGROUP_ARGB` (0x80000) on any format other than:
  X1R5G5B5, A1R5G5B5, R5G6B5, R8G8B8, X8R8G8B8, A8R8G8B8, A2R10G10B10, 0x24, A2B10G10R10, A8B8G8R8, X8B8G8R8,
  A16B16G16R16F, A32B32G32R32F.

Fix-ups the runtime applies:
- `OFFSCREEN_RENDERTARGET` (8) also implies `SAME_FORMAT_RENDERTARGET` (0x10) and `SAME_FORMAT_UP_TO_ALPHA` (0x100).
- `ZSTENCIL_WITH_ARBITRARY_COLOR_DEPTH` (0x80) implies `ZSTENCIL` (0x40).

`CEnum::CEnum` @100586b6 looks for an X8R8G8B8 DISPLAYMODE entry (flag 8) and a 16-bit one (flag 4), and prunes the
table when they are absent.

The FORMATOP bit values follow the DDK's `d3dhal.h`, which is **not** vendored here. The runtime's own error strings
confirm DISPLAYMODE = 0x400, 3DACCELERATION = 0x800 and MEMBEROFGROUP_ARGB = 0x80000.

How the enumeration API calls use the list, with no driver call:
- **`CEnum::CheckDeviceType` @10053720**:
  - The adapter format needs `DISPLAYMODE|3DACCELERATION`.
  - Backbuffer == adapter format needs `SAME_FORMAT_RENDERTARGET`.
  - A8R8G8B8 on X8R8G8B8 needs `SAME_FORMAT_UP_TO_ALPHA_RENDERTARGET`.
  - Windowed on a WDDM driver otherwise accepts any backbuffer with `OFFSCREEN_RENDERTARGET`.
  - Windowed with an UNKNOWN backbuffer uses the adapter format.
- **`CEnum::CheckDeviceFormat` @10045400**:
  - It derives the required ops from the resource type and usage: TEXTURE 1, VOLUME 2, CUBE 4, DEPTHSTENCIL 0x40,
    RENDERTARGET 0x10, 8 or 0x100, AUTOGENMIPMAP 0x400000 (a miss returns `D3DOK_NOAUTOGEN`), and so on.
  - It needs the adapter format with ops 0xC00. On a WDDM driver an unknown adapter format is ignored instead of
    failing.
  - `D3DUSAGE_DYNAMIC` textures need `Caps2.DYNAMICTEXTURES`.
- **`CEnum::CheckDeviceMultiSampleType` @1004b000**:
  - Bit `1 << (type-1)` of BltMsTypes when windowed, of FlipMsTypes when fullscreen.
  - The quality level for NONMASKABLE is the popcount of the mask. Exact levels come from GetCaps(5) as above.
- **EnumAdapterModes / GetAdapterModeCount**: built from the kernel mode list in `CEnum::PopulateModeTables`
  @100552e7 → `D3D9BuildModeTable`, not from the UMD. **Inferred**: the kernel half was not traced.

---

## 3. pfnCreateDevice

### 3.1 Arguments (`CreateDeviceLHDDI` @10053cfc, verified)
```
D3DDDIARG_CREATEDEVICE {
  hDevice        = runtime DDobj pointer (in) / driver hDevice (out, kept at DDobj+0x698)
  Interface      = 9
  Version        = the OpenAdapter Version that succeeded
  pCallbacks     = runtime D3DDDI_DEVICECALLBACKS (below)
  pCommandBuffer, CommandBufferSize, pAllocationList, AllocationListSize,
  pPatchLocationList, PatchLocationListSize
                 = from the runtime-created context if DriverVersion < 2, else 0
  pDeviceFuncs   = DDobj+0x6a0 (zeroed, 0x23c bytes) — the UMD fills it
  Flags          = AllowMultithreading = DDobj flag bit 9;
                   AllowFlipBatching (2) when IsLegacyMode, or (flags & 0x80010) == 0x80010 && Version ≥ 0x6001
                   (bit meanings inferred)
  CommandBuffer  = GPU VA, only when DDobj flag 0x4000
}
```
`pfnCreateDevice` is called through `DDobj+0x8e0` with the driver's `hAdapter` from OpenAdapter.

### 3.2 Device callbacks provided (verified, the table written @100543e9..10054637)
`D3DDDI_DEVICECALLBACKS` is filled up to index 64.

**Non-NULL:**
- `Allocate`, `Deallocate`, `SetPriority`, `QueryResidency`, `SetDisplayMode`, **`Present`** (`_PresentCB@8`
  @10065de0), `Render` (`_RenderCB@8`), **`Lock`** (`_LockCB@8` @100c18f0), `Unlock`, `Escape`
- the four overlay callbacks
- `CreateContext`, `DestroyContext`
- the sync-object, SetAsyncCallbacks, SetDisplayPrivateDriverFormat and Offer/Reclaim callbacks
- the `*2` sync callbacks
- `MakeResident`, `Evict`, the CPU/GPU wait and signal callbacks, the paging queue, `Lock2`/`Unlock2`,
  `InvalidateCache`
- the GPU VA reserve/map/free/update callbacks, **`CreateContextVirtual`**, `SubmitCommand`, `Deallocate2`,
  `SignalFromGpu2`
- `ReclaimAllocations2`, `GetResourcePresentPrivateDriverData`, `UpdateAllocationProperty`, `OfferAllocations2`
- index 52 (`ReclaimAllocations3Cb` in the header), which gets `_ReclaimAllocations2Cb_8`
- `AcquireResource`, `ReleaseResource`
- the HwQueue create/destroy/submit/wait/signal callbacks, `SubmitPresentBltToHwQueue`, `SubmitPresentToHwQueue`

**NULL:** `pfnPresentMultiPlaneOverlayCb` (27), `pfnLogUMDMarkerCb` (28), `pfnCreateHwContextCb` (55),
`pfnDestroyHwContextCb` (56) and `pfnSubmitHistorySequenceCb` (64).

**Entries past index 64 lie on uninitialized stack.** A UMD built against a newer header must not read them.

### 3.3 Which D3DDDI_DEVICEFUNCS slots may be NULL
Every `cmp $0, off(reg)` against the 0x6a0..0x8dc range was found and mapped to its function. The only NULL-tested
slots are:

| Slots | Who tests | Meaning of NULL |
|---|---|---|
| 0x178 GetCaptureAllocationHandle, 0x17c CaptureToSysMem | DXVA/overlay helpers | feature off |
| 0x180 LockAsync, 0x184 UnlockAsync, 0x188 Rename | `_D3D9AsyncCallbacksSupported`, `_CreateSurfaceLH` | async locks off unless all three are set |
| 0x18c..0x1a4 (Win7 video processor, authenticated channel) | `CreateDeviceLHDDI` | all-or-nothing: any NULL zeroes the group |
| 0x1a8..0x1c4 (crypto session group) | `CreateDeviceLHDDI` | all-or-nothing |
| 0x1c8..0x1d8 (EncryptionBlt, GetPitch, key refresh, DecryptionBlt) | `_D3D9GetContentProtectionCaps`, `_D3D9GetSurfacePitch` | feature off |
| 0x200 CreateResource2 | `_CreateSurfaceLH`, `_D3D9ResetDevice` | uses CreateResource instead (only legal when DriverVersion ≤ 0x4001) |
| 0x210..0x22c Flush1, CheckCounterInfo, CheckCounter, UpdateSubresourceUP, Present1, SetMarker, SetMarkerMode | `CreateDeviceLHDDI` | WDDM1.3 present path off (§1.4) |
| 0x230 TrimResidencySet | `CreateDeviceLHDDI`, `_D3D9ResetDevice` | no trim notification registration |
| 0x234/0x238 Acquire/ReleaseResource | `ValidateUMDeviceFuncs` | **must be non-NULL above 0x6000** |

**Everything else is called through unconditionally.** That covers:
- all state, draw, shader, decl, query, Lock/Unlock, Blt/ColorFill/DepthFill, Clear, Present/Flush, OpenResource and
  DestroyResource slots;
- **DestroyDevice**;
- for example `CD3DDDIDX10::SetRenderState` @10064080, `CD3DDDIDX10_DrawPrimitive` @1004dfc0 and
  `CD3DDDIDX10::InsertStateSetOp` @1013f510.

A NULL there crashes the CFG-checked indirect call the first time the API path is used. A failing HRESULT in those
wrappers is turned into a device-lost check (`RegisterDeviceLost`) or a thrown E_FAIL, and the API call fails.
**Caveat:** the scan only matched compare-to-zero against memory. A NULL test done through a register load would have
been missed. None was seen in the wrappers read.

---

## 4. CreateDevice (windowed, DISCARD, HAL, HW or MIXED VP) → DDI calls

### 4.1 Flow (verified)
1. `CEnum::CreateDevice` → `CEnum::CreateDeviceImpl` @1004fc02.
2. **`CEnum::ValidateCreateDevice`** @1005730d. It reuses or creates a DirectDraw object (a UMD device), then checks:
   - HW VP needs `DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT`;
   - MIXED needs HWTRANSFORMANDLIGHT;
   - PUREDEVICE needs `DevCaps & D3DDEVCAPS_PUREDEVICE`;
   - then `ValidatePresentParameters`.
3. For a WDDM driver `CreateDeviceImpl` **always** creates a fresh DirectDraw object
   (`_InternalDirectDrawCreate` @1005012c), which is a new OpenAdapter + pfnCreateDevice + the caps passes.
4. Pure device is demoted. With a WDDM driver and VS ≥ 2.0 or PS ≥ 2.0, `D3DCREATE_PUREDEVICE` is stripped and a
   `CD3DHal` (state-shadowing) device is built instead of `CD3DBase`.
5. `CBaseDevice::Init` @1004f36a. It fails with "The specified adapter does not support a D3D9 hardware device" if
   `Caps & 0x2000000`. Then it calls `CBaseDevice::ChooseDDI` @1007d1d3:
   - **DDI type 10 (`CD3DDDIDX10`)** for a WDDM driver;
   - **type 11 (`CD3DDDIDX10TL`)** when `DevCaps & HWTRANSFORMANDLIGHT`.

   It then creates the swap chain (`CSwapChain::Init` → `CSwapChain::CreateWindowed` @1004667b) and the auto depth
   stencil (`CSurface::CreateZStencil`).
6. `CD3DHal::InitDevice` @10041e00 → `CD3DBase::InitDevice` @1008e1d0, which allocates the DDI object for the DDI
   type. Then `CD3DDDIDX10::Init` @10058670 → `CD3DDDIDX9::InitFFConverter` @10057a08. Afterwards the base sets the
   viewport (DDI vtable +0x98) and the scissor (DDI vtable +0x164).

### 4.2 Resources the swap chain creates (`CSwapChain::CreateWindowed` @1004667b, `_DdCreateSurfaceLH@4` @1003e810, `_CreateSurfaceLH` @1003e880, `_ConvertCreateStructures` @1003fa90)
**A primary.** For the device's first swap chain the runtime creates a CDriverSurface of the desktop size in the
display format, with internal flag 0x20 (primary) and usage 0x4000000 (lockable). `_DdCreateSurfaceLH` sends it to
`_CreateSurfaceLH(…, primary=1, 1)`, which builds `Flags.Primary` (0x8000) plus `SharedResource` (0x800) or
`RenderTarget` (1). For a non-Ex device:
- The runtime first calls `D3DKMTGetSharedPrimaryHandle`.
- If that returns a handle and the flags carry SharedResource, the primary is **opened through `pfnOpenResource`**
  (`_OpenResourceLH` @1007b8aa) instead of created.
- An `E_INVALIDARG` from that open is tolerated for a primary: the handle is set to 0 and the call returns success.
- With no shared-primary handle, no driver call is made at all.
- A NULL driver handle is accepted only for a primary.

**Inferred:** which branch a DWM-composed windowed device takes on the guest. It depends on whether the kernel returns
a shared-primary handle.

**Backbuffers.** `BackBufferCount` CDriverSurfaces with usage RENDERTARGET go through `pfnCreateResource2`, or
`pfnCreateResource` below 0x4001. How `_ConvertCreateStructures` @1003fa90 maps D3D usage into
`D3DDDI_RESOURCEFLAGS`:
- RENDERTARGET → `RenderTarget`.
- `NotLockable` unless the usage carries the internal lockable bit (0x4 or 0x4000000, for example
  D3DPRESENTFLAG_LOCKABLE_BACKBUFFER).
- `DiscardRenderTarget` (0x1000) from the internal discard flag.
- DYNAMIC → `Dynamic`. Non-dynamic buffers → `HintStatic`. WRITEONLY → `WriteOnly`. POINTS, RTPATCHES and NPATCHES
  map to their own flags. AUTOGENMIPMAP → `AutogenMipmap`, except in the MANAGED pool.
- Texture, CubeMap, Volume, VertexBuffer and IndexBuffer as named. DEPTHSTENCIL → `ZBuffer`.

Pool mapping:

| D3DPOOL | D3DDDI pool |
|---|---|
| DEFAULT and MANAGED | VIDEOMEMORY |
| SYSTEMMEM | SYSTEMMEM |
| internal 4 | LOCALVIDMEM |
| internal 5 | NONLOCALVIDMEM |
| SCRATCH | never reaches the driver (E_INVALIDARG) |

**SYSTEMMEM resources must have `pSysMem` in every `D3DDDI_SURFACEINFO`**, otherwise the result is
`D3DERR_NOTAVAILABLE` (`_CreateSurfaceLH`). The memory belongs to the runtime.

**After the call:** `hResource` must come back non-NULL for anything but a primary. Otherwise the result is "Driver
did not return a valid resource handle" (0x88760827).

### 4.3 Windowed Present: blt model (`_DdBltLH@4` @1004bee0, `_Present` @1004c92a, `_Flush` @1004c9bf, `_PresentCB@8` @10065de0; verified)
`IDirect3DDevice9::Present` → `CSwapChain::PresentMain` → `DdBltLH`. Per present:
1. `_WaitForQueuedPresent`.
2. Optionally `pfnBlt` (slot 0xdc), when the swap chain needs an intermediate copy (for example a stretch). This is
   conditional on swap-chain state, and the exact trigger was not traced.
3. `_Flush` → `pfnFlush` (0xa4), or `pfnFlush1` (0x210) when the WDDM1.3 present path is on. A failure gives "UMD
   failed Flush call in DdBltLH".
4. The runtime fills its own `D3DKMT_PRESENT` in the DirectDraw object (+0xc0):
   - `hWindow`;
   - `SrcRect`/`DstRect` with `SrcRectValid`/`DstRectValid`;
   - dirty-region subrects;
   - `PresentCount`;
   - `Flags.Blt`, plus `Rotate` (0x2000) in the non-redirected case, or `PresentCountValid|RedirectedBlt` (0x11000)
     on the redirected path.
5. `_Present` calls **`pfnPresent`** (0xa0), or `pfnPresent1` (0x220), with
   `D3DDDIARG_PRESENT{hSrcResource, SrcSubResourceIndex, hDstResource (0 when windowed, unless a redirection surface
   is open), DstSubResourceIndex, Flags = runtime flags & 7 (Blt), FlipInterval}`.
6. The UMD must call **`pfnPresentCb`** from inside that call. `PresentCB`:
   - rejects the call with E_INVALIDARG unless the runtime-prepared flags include Blt, ColorFill, Flip or
     RedirectedFlip;
   - takes `hContext` from `D3DDDICB_PRESENT` (DriverVersion ≥ 2) and `hSrcAllocation`, and copies the
     broadcast contexts;
   - calls `D3DKMTPresent` with the runtime's struct.
7. A UMD that never calls PresentCb shows nothing and gets no error.

`PresentCB` maps these NTSTATUS values:
- `STATUS_GRAPHICS_*` 0xC01E0005 → `S_PRESENT_MODE_CHANGED`.
- 0xC01E0006, 0xC01E0007, 0xC01E0000, 0xC01E000F and 0xC01E000B → `S_PRESENT_OCCLUDED`.
- Device-lost codes → `D3DERR_DEVICELOST`.
- Anything else → "OsThunkDDIPresent failed in PresentCB" and an error.

So the UMD-side contract for a windowed DISCARD swap chain is:
- copy or resolve nothing itself;
- flush pending rendering in `pfnFlush`;
- in `pfnPresent`, submit any pending work and call `pfnPresentCb(hSrcAllocation = the backbuffer's allocation,
  hDstAllocation = 0, hContext)`.

The kernel `DxgkDdiPresent` does the actual blt to the redirected or DWM surface.

### 4.4 State blocks (`CStateSets::Init` @10056499, `CD3DBase::EndStateBlock` @10126230, `CStateBlockAPI::Apply` @10125bf0, `CStateSets::Capture` @1012df4b, `CStateSets::DeleteStateSet` @1012ef1c; verified)
- The runtime always records its own copy. It allocates a `CStateSet` for a CD3DHal device and a `CPureStateSet` for
  a pure one.
- **Unless the registry value `EmulateStateBlocks` (D3D key) is non-zero on a CD3DHal device**, state that the driver
  also holds goes into a second, "driver" buffer.
- At EndStateBlock and CreateStateBlock the runtime calls `CD3DDDIDX10::WriteStateSetToDevice` @10140660:
  - `pfnStateSet(Operation=0 BEGIN)`;
  - replay of the recorded state through the normal DDIs (`CStateSets::TranslateDeviceBufferToDX10DDI` @1013156b);
  - `pfnStateSet(1 END)`;
  - **it keeps the returned `hStateSet`**.
- Apply re-applies the runtime-side buffer and then, if the driver handle is non-zero, updates only its shadow and
  sends `pfnStateSet(3 EXECUTE, h)`.
- Capture sends `4 CAPTURE`. Delete sends `2 DELETE`. `CD3DDDIDX10::CreatePredefinedStateSet` @1013d790 can send
  `5 CREATE`.
- **So the UMD must implement real state sets:**
  - record between BEGIN and END and return a non-NULL handle;
  - re-apply on EXECUTE and re-snapshot on CAPTURE.

  A UMD that returns NULL from END silently loses every render, sampler and transform state in a state block.

### 4.5 Fixed function and software vertex processing (`CD3DDDIDX9::InitFFConverter` @10057a08 + disassembly, `CFFConverterEventHandler::Init` @10057b18, `CD3DDDIDX10TL::SetTransform` @10061fb0, `CD3DDDIDX10TL::SetLight` @1007b430, `CD3DDDIDX10::SetRenderState` @10064080, `CD3DDDIDX10_DrawPrimitive` @1004dfc0)
**Fixed-function vertex pipeline**
- On DDI type 11 (HW T&L) with HW or MIXED VP (no `SOFTWARE_VERTEXPROCESSING`) and a driver `VertexShaderVersion ≥
  2.0`, `InitFFConverter` creates the runtime's fixed-function→VS converter. `Init` sets handler+0x13c, which is
  DDI+0x3b0.
- With that flag set, `SetTransform` and `SetLight` store into the converter's constants and **never call
  `pfnSetTransform` or `pfnSetLight`**.
- `SetRenderState` forwards only the states the converter does not consume.
- At draw time the runtime generates a vertex shader and sets it through the normal shader DDIs.

**Fixed-function pixel pipeline**
- For DDI type ≥ 10 and `PixelShaderVersion ≥ 2.0`, the fixed-function→PS converter is created and enabled
  (`EnableUsePsConverter`).
- **Inferred:** that texture-stage state is then fully consumed. `SetTSS` itself was not decompiled.

**Software vertex processing**
- `SOFTWARE_VERTEXPROCESSING` (behavior 0x20), or DDI type 10 without HW T&L, zeroes the VS version passed to the
  converter. The runtime then transforms with its own PSGP and VS virtual machine (`CD3DHal::InitDevice` @10041e00:
  `pfnFEContextCreate`, `D3DFE_PROCESSVERTICES`).
- **Inferred:** the driver then receives already-transformed vertices.
- MIXED switches at run time (`CD3DHal::SwitchVertexProcessingMode`, `CFFConverterEventHandler::EnableUseVsConverter`).

**Net:** a vs_2_0/ps_2_0 UMD that reports HW T&L does not need fixed-function T&L or texture stages, as long as its
shader DDIs work. It still receives the DDIs the converter does not intercept, for example SetViewport, SetZRange and
SetMaterial when the converter is off.

---

## 5. Lock/Unlock, dynamic resources and the managed pool

**Lock** (`_DdLockLH@4` @10065500 → `pfnLock` slot 0x8c with `D3DDDIARG_LOCK`, verified):

| D3D9 input | D3DDDI_LOCKFLAGS |
|---|---|
| `D3DLOCK_READONLY` (0x10) | `ReadOnly` (Discard is not considered then) |
| `D3DLOCK_DISCARD` (0x2000) | `Discard` |
| `D3DLOCK_NOOVERWRITE` (0x1000) | `NoOverwrite` |
| `D3DLOCK_DONOTWAIT` (0x4000) | `DoNotWait` |
| buffer offset/size | `RangeValid` |
| rect | `AreaValid` |
| box | `BoxValid` |
| resource with the `MightDrawFromLocked` usage | `MightDrawFromLocked` |
| runtime pool 1 | `NotifyOnly` |

- Outputs read back: `pSurfData` and `Pitch`, plus `SlicePitch` for volumes.
- A failure gives "UMD failed Lock in DdLockLH". `D3DERR_DEVICELOST` there marks the device lost.
- `pfnLockAsync` is used only when all three async slots exist and the resource is flagged for it.

**Unlock** (`_DdUnlockLH@4` @10065c90 → `pfnUnlock` slot 0x90):
- It sends `NotifyOnly` for pool-1 resources.
- After a device loss it retries once.

**Discard and NoOverwrite.** The UMD must rename on `Discard`, returning fresh memory without waiting, and must not
wait on `NoOverwrite`. That is how a Source-engine dynamic VB/IB ring is used. **Inferred:** this is the DDI contract;
the runtime only passes the flags through. `_LockCB@8` @100c18f0 is the UMD's route to `D3DKMTLock` and handles the
shared-primary lock notification.

**Managed pool**
- `_QueryLHDDICaps` clears `D3DCAPS2_CANMANAGERESOURCE`, so the runtime's `CResourceManager` (`Init` @100583db) owns
  every D3DPOOL_MANAGED resource.
- The driver sees a VIDEOMEMORY resource plus a SYSTEMMEM shadow created with runtime-owned `pSysMem`.
- Dirty regions are uploaded with:
  - textures: `CMipMap::UpdateDirtyPortion` → `CD3DBase::TexBlt` @10062936 → `CD3DDDIDX10::TexBlt` @10065770 →
    `pfnTexBlt1` (0x1ec, DriverVersion ≥ 0x4001) or `pfnTexBlt` (0x48), with
    `D3DDDIARG_TEXBLT{hDst, hSrc, CubeMapFace, DstPoint, SrcRect}`;
  - buffers: `CVertexBuffer::UpdateDirtyPortion` and `CIndexBuffer::UpdateDirtyPortion` → `CD3DBase::BufBlt`
    @1011bc09 → `pfnBufBlt1` (0x1e8) or `pfnBufBlt` (0x44);
  - on PreLoad: `CBaseTexture::PreLoadImpl` and `CBuffer::PreLoadImpl`.
- `UpdateTexture` and `UpdateSurface` use the same TexBlt.
- Priority and LOD go through `pfnSetPriority` (`CD3DDDIDX10::SetPriority` @1013fc50).
- `CBaseDevice::CanTexBlt` and `CBaseDevice::CanBufBlt` (@100f8f55, @100f739a) are always true for a WDDM HAL
  (DDI type ≥ 4 and ≥ 6), so these DDIs are mandatory.

---

## 6. Silent fallbacks and failures for a "correct but minimal" UMD

1. **There is no fallback to 9on12 once a name is present.** `D3D9RequestsD3D9on12` is consulted once per adapter
   in `CEnum::CEnum`, and only STATUS_INVALID_PARAMETER selects 9on12. If the DLL load, `OpenAdapter`,
   `pfnCreateDevice`, `ValidateUMDeviceFuncs`, a mandatory `pfnGetCaps`, the caps validation (§2.3) or the format
   validation (§2.4) fails:
   - `Direct3DCreate9` still succeeds and lists the adapter;
   - HAL `GetDeviceCaps` or `CreateDevice` fail ("The specified adapter does not support a D3D9 hardware device",
     `CBaseDevice::Init` @1004f36a; `D3DERR_NOTAVAILABLE` from `_InternalDirectDrawCreate` @100576b2).

   The kernel-side requirement is in `src/km/avgpu.inf`: the DX9 entry must name a DLL that exports `OpenAdapter`.
2. **The UMD runs inside Direct3DCreate9 and runs several times.** The first `pfnCreateDevice` happens during
   enumeration. More adapter/device pairs come from:
   - `ValidateCreateDevice`;
   - the app's `CreateDevice`;
   - `CheckDeviceMultiSampleType` with quality levels;
   - Reset.

   Global state shared between instances, or any assumption of one device per process, is wrong.
3. **The `ValidateUMDeviceFuncs` pairing.** Filling both `pfnCreateResource` and `pfnCreateResource2` (or both Blt
   and Blt1, or Present/Flush together with Present1/Flush1 on the WDDM1.3 path) fails `pfnCreateDevice` when
   `DriverVersion > 0x4001`.
4. **FORMATOP mistakes disable the whole driver.** These are easy to get wrong: listing A8R8G8B8 with 3DACCELERATION,
   or advertising a multisample type for a format without the NONMASKABLE bit.
5. **Missing kernel facts.** GETSEGMENTSIZE, ADAPTERTYPE, WDDM_1_2_CAPS and WDDM_1_3_CAPS must succeed, and the
   adapter must have a monitor (§1.5). Otherwise CreateDevice fails before any caps query.
6. **A NULL core slot crashes** the first time the API uses it (§3.3). Fill every slot from `pfnSetRenderState`
   through `pfnOpenResource`, even with stubs that return S_OK. In particular `pfnStateSet`, `pfnGetInfo`,
   `pfnSetConvolutionKernelMono`, `pfnComposeRects`, `pfnDepthFill`, the light and material slots, and the patch
   draws.
7. **State blocks need a real `pfnStateSet`** (§4.4). A no-op that returns a NULL handle silently drops state.
8. **Present needs `pfnPresentCb`** with the UMD's own `hContext` (DriverVersion ≥ 2). A UMD that submits rendering
   but never calls PresentCb shows a black window and reports no error.
9. **The windowed primary may be opened through `pfnOpenResource`** on the kernel's shared GDI primary (§4.2). Return
   `E_INVALIDARG` if it cannot be honoured. That case is tolerated, while other failures fail CreateDevice.

## Not established
- What `D3DKMTGetSharedPrimaryHandle` returns for avgpu's primary under DWM, and so which §4.2 branch a windowed
  device takes.
- The exact trigger for the intermediate `pfnBlt` in `DdBltLH`.
- The kernel half of display-mode enumeration.
- Whether `SetTextureStageState` is fully consumed by the PS converter (§4.5).
- `d3d9on12_x86.dll` was imported but not analysed. Nothing above depends on it.
