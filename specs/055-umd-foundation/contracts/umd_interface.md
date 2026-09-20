# Contract: how the Direct3D runtimes reach `rvkumd.dll`

Source of truth for the runtimes' side: `d3d11.dll` 10.0.26100.9278
(`ae2777002f5a0ff543b5b9bec5124746559fd01d6c7c0c216d3194632f084502`) and
`d3d12core.dll` (pinned in `tools/re/binaries.txt`), and the WDK headers
`d3d10umddi.h`, `d3d12umddi.h`, `d3dumddi.h` pinned in research.md §0. This
contract is what the INF and the DLL promise them.

**Read this if you are feature 056, 057 or 064.** Everything below is 055's and
is finished; the rows marked *056*, *057* or *064* are the only places those
features change behaviour, and they change it by filling a slot that today
answers a documented refusal - never by changing a signature, a handle
meaning, or the object model.

## Registration (INF)

In the software key of every rvkgpu adapter (`[Rvkgpu_SoftwareSettings]`):

| Value | Type | Data |
|---|---|---|
| `UserModeDriverName` | `REG_MULTI_SZ` (`0x00010000`) | `rvkumd.dll`, `rvkumd.dll`, `rvkumd.dll`, `rvkumd.dll` |

Four entries, indexed by `KMTUMDVERSION` (`d3dkmthk.h:1933`): `DX9`, `DX10`,
`DX11`, `DX12`. A **bare file name**, not a `%13%` path - dxgkrnl's
`PostProcessUMDFileName` turns it into the path the runtime loads, and that
translation is what makes a UMD work inside a VM container. The runtime asks
the kernel for it with `D3DKMTQueryAdapterInfo(KMTQAITYPE_UMDRIVERNAME = 1)`
into `D3DKMT_UMDFILENAMEINFO`; `d3d12core.dll` carries the literal
`QAI (UMDRIVERNAME)` beside `LoadUMD`.

No `UserModeDriverNameWow` (feature 061). `rvkumd.dll` is in `[Rvkgpu_Files]`,
copied to DIRID 13 with `rvkgpu.sys`, and covered by the package catalog.

## Exports

Exactly these, undecorated, from `rvkumd.def`; each is one line into the root.

| Export | Resolved by | Behaviour |
|---|---|---|
| `OpenAdapter10_2(D3D10DDIARG_OPENADAPTER *)` | `d3d11.dll` | opens the adapter for the D3D10/10.1/11 runtime; fills `hAdapter` and `pAdapterFuncs_2`; `S_OK`, or `E_INVALIDARG` for a null argument, `E_FAIL` for no rvkgpu adapter, `E_OUTOFMEMORY` for no room |
| `OpenAdapter12(D3D12DDIARG_OPENADAPTER *)` | `D3D12Core.dll` | the same for D3D12; fills `hAdapter` and `pAdapterFuncs` |
| `OpenAdapter9(D3DDDIARG_OPENADAPTER *)` | `d3d9.dll` | **feature 064**; not exported by 055, and `d3d9.dll` then finds no driver rather than a broken one |

`OpenAdapter10` is **not** exported: `OpenAdapter10_2` is its superset, the
runtime prefers it, and the docs require it of a D3D11 driver.
`OpenAdapter_D3D11On12` is not exported: taking that road would put Microsoft's
`d3d11on12.dll` between the runtime and this driver, which is 056's decision to
make and not the foundation's.

The DLL's entry does nothing at attach or detach: no thread, no wait, no
allocation, nothing the loader lock forbids. The root is constructed on the
first `OpenAdapter*` call and tears down nothing at process detach.

## `OpenAdapter10_2`

```c
typedef struct D3D10DDIARG_OPENADAPTER {
    D3D10DDI_HRTADAPTER            hRTAdapter;         // in
    D3D10DDI_HADAPTER              hAdapter;           // out
    UINT                           Interface;          // in
    UINT                           Version;            // in
    CONST D3DDDI_ADAPTERCALLBACKS* pAdapterCallbacks;  // in
    union { D3D10DDI_ADAPTERFUNCS   *pAdapterFuncs;
            D3D10_2DDI_ADAPTERFUNCS *pAdapterFuncs_2; };  // out
} D3D10DDIARG_OPENADAPTER;
```

| Field | 055's rule |
|---|---|
| `hRTAdapter` | recorded on the `UmdAdapter`; handed back to the runtime in every callback |
| `hAdapter` | filled with the `UmdAdapter`'s handle; the runtime passes it to all five table slots |
| `Interface` | must be one `DdiVersionList` serves, else `E_NOINTERFACE` and nothing is opened |
| `Version` | recorded; never a condition (no runtime-build workaround exists here yet) |
| `pAdapterCallbacks` | **the pointer is recorded, never the individual callbacks** - the runtime may move them whenever no thread is inside the driver. A null pointer is `E_INVALIDARG` |
| `pAdapterFuncs_2` | all five slots written, all non-null |

## The D3D10/11 adapter table - `D3D10_2DDI_ADAPTERFUNCS`

| Slot | 055 answers | Whose |
|---|---|---|
| `pfnGetSupportedVersions` | the count-then-fill protocol over `DdiVersionList`: a null array answers the count; an array shorter than the count is `E_OUTOFMEMORY` with the count written back; an exact array is filled and `S_OK`. Today the list is one word, `D3DWDDM3_2_DDI_SUPPORTED` = `0x000B002D00010000` | 055; **056** adds a word only when it has implemented and tested that revision |
| `pfnGetCaps` | the caps whose data 055 owns (adapter identity and memory, below); `E_INVALIDARG` for every other `D3D10_2DDICAPS_TYPE`, with a test naming each type refused | 055 for identity; **056** for `D3D11DDICAPS_THREADING`, `_SHADER`, `_3DPIPELINESUPPORT`, the `D3D11_OPTIONS*` family and the format caps |
| `pfnCalcPrivateDeviceSize` | `sizeof(DeviceCookie)`, independent of `Interface`, `Version` and `Flags` | 055, and it does not change: the cookie is a magic and a pointer whatever the device becomes |
| `pfnCreateDevice` | builds the `UmdDevice`, records the callbacks, fills the DXGI seam, then answers `E_NOTIMPL` because no device-functions table is filled | **056** fills `pWDDM2_6DeviceFuncs` and answers `S_OK`; everything before that line is 055's and must not be re-done |
| `pfnCloseAdapter` | destroys the adapter; `E_FAIL` if a device made through it is still live | 055 |

## `OpenAdapter12` and the D3D12 adapter table

```c
typedef struct D3D12DDIARG_OPENADAPTER {
    D3D12DDI_HRTADAPTER            hRTAdapter;         // in
    D3D12DDI_HADAPTER              hAdapter;           // out
    CONST D3DDDI_ADAPTERCALLBACKS* pAdapterCallbacks;  // in
    D3D12DDI_ADAPTERFUNCS*         pAdapterFuncs;      // out
} D3D12DDIARG_OPENADAPTER;
```

No `Interface` and no `Version`: D3D12 carries the version per table instead.

| Slot | 055 answers | Whose |
|---|---|---|
| `pfnGetSupportedVersions` | as the D3D11 one, over the D3D12 list | 055; **057** adds words |
| `pfnGetCaps` | the identity caps; `E_INVALIDARG` otherwise | 055 / **057** |
| `pfnCalcPrivateDeviceSize` | `sizeof(DeviceCookie)` | 055 |
| `pfnCreateDevice` | as the D3D11 one, then `E_NOTIMPL` | **057** |
| `pfnDestroyDevice` | destroys the device the cookie names; a cookie this driver did not write is ignored, not dereferenced | 055 |
| `pfnCloseAdapter` | as the D3D11 one | 055 |
| `pfnGetOptionalDDITables` | zero tables: `*puEntries = 0`, `S_OK` | **057** names the optional tables it fills |
| `pfnFillDDITable` | `E_NOTIMPL` for every `D3D12DDI_TABLE_TYPE`, with a test naming each of the four a device needs (`DEVICE_CORE`, `COMMAND_LIST_3D`, `COMMAND_QUEUE_3D`, `DXGI`) | **057** |

**What D3D12Core actually does, read from the binary** (research.md §12, and
057 should read that section before filling anything):

- it zeroes all eight table slots before the call, so a slot the driver does
  not write reads back as null;
- after a successful `OpenAdapter12` it checks that the out `hAdapter` is
  non-zero **and** that slots 2 through 7 - `pfnCloseAdapter`,
  `pfnGetSupportedVersions`, `pfnGetCaps`, `pfnGetOptionalDDITables`,
  `pfnFillDDITable`, `pfnDestroyDevice` - are all non-null. If not, it calls
  `pfnCloseAdapter` to hand the adapter back and answers `0x887A0004`
  (`DXGI_ERROR_UNSUPPORTED`). That is why all eight are filled from the first
  commit, and why `pfnCloseAdapter` must be safe on an adapter no device was
  made on;
- the first thing it asks the driver afterwards is the 3D pipeline level:
  `pfnGetCaps` with `D3D12DDICAPS_TYPE_0081_3DPIPELINESUPPORT1` (1074, 8
  bytes), falling back to `D3D12DDICAPS_TYPE_3DPIPELINESUPPORT` (1007, 4
  bytes). Refusing both is survivable - the runtime journals
  `"Driver returned invalid pipeline caps"` and forces generic level 1 - so it
  is the first cap **057** owes, not 055.

Still unread, and left in T036: the D3D11 runtime's own sequence, and which
`pfnFillDDITable` types D3D12Core asks for.

## The device object model - do not re-invent this

The runtime owns the device's memory. `pfnCalcPrivateDeviceSize` says how many
bytes it should allocate; `pfnCreateDevice` receives that block as
`hDrvDevice.pDrvPrivate` and it is the handle every later device call names.

055's answer, and the one 056, 057 and 064 use unchanged:

- the block holds a **`DeviceCookie`**: `uint64_t magic_` (`RVKD` =
  `0x444B5652`) and `UmdDevice *device_`. Nothing else. No object is
  constructed there - `tools/check_design.py` allows in-place construction only
  in `ObjectStorage` and `ObjectPool`, and the runtime's block is neither.
- the **`UmdDevice`** lives in the root's `ObjectPool<UmdDevice>`.
- **`DeviceTable::device_of(void *drv_private)`** is the only way back. It
  checks the magic *and* that the pointer is one the table handed out, before
  reading any field. A block another component scribbled on, a block from a
  destroyed device, or a pointer never handed out all answer null, and the
  caller answers the DDI's documented refusal.

Every table thunk is one line: resolve the handle, call the object, return.
No logic in a thunk - the same rule as `src/core/ddi/entry_points.cpp`.

## Adapter identity, and what it does and does not decide

`AdapterIdentity` answers, from the same two D3DKMT queries the Vulkan ICD
makes on the same adapter:

| Value | Source |
|---|---|
| LUID | `Kmt::adapter_luid` (`D3DKMT_ENUMADAPTERS2::AdapterLuid`) |
| vendor id, device id, subsystem id | `PciIdentity` from `AdapterSearch` |
| description | the driver's own name for the adapter |
| dedicated video memory | `Kmt::query_video_memory(local)` |
| shared system memory | `Kmt::query_video_memory(non_local)` |

A figure the kernel refuses is reported as zero and logged once; the open does
not fail for it.

**What the UMD does not decide:** whether DXGI calls the adapter hardware or
software. `D3DKMT_ADAPTERTYPE` (`d3dkmthk.h:2088`) - `RenderSupported`,
`DisplaySupported`, `SoftwareDevice`, `Paravirtualized` - is dxgkrnl's answer
for `KMTQAITYPE_ADAPTERTYPE`, derived from what the miniport is. `rvkgpu` is a
PnP display device with a VidPn, so `SoftwareDevice` is already clear and
nothing here can set it. What the UMD decides is whether the adapter is listed
as a D3D adapter **at all**, and at what level: DXGI lists an adapter whose UMD
loads and whose `GetSupportedVersions` names a version the runtime knows. That
is why the adapter today has DWM on WARP (feature 046's live measurement, in
`docs/kb/07-usermode.md`), and why it will not once 056 answers `S_OK` to a
create.

DXCore's `DXCORE_ADAPTER_ATTRIBUTE_D3D11_GRAPHICS` and `_D3D12_GRAPHICS`
follow from the same versions. 055 claims neither, because 055 creates no
device (constitution VI, FR-004).

## Device removal

One rule for all four APIs. The transport's context fault (feature 044) is the
cause; the D3D spelling is:

| Cause | Answered |
|---|---|
| the adapter was reset or the host faulted a context this device did not own | `DXGI_ERROR_DEVICE_REMOVED` (`0x887A0005`) |
| this device's own context was faulted | `DXGI_ERROR_DEVICE_RESET` (`0x887A0007`) |

The **first** cause is the one kept and the one `GetDeviceRemovedReason` names;
later ones are counted and dropped. A removed device answers its removal code
from every entry that returns an `HRESULT`, asks the transport nothing, and
still gives back locally whatever the kernel holds for it when it is destroyed.

Most D3D11 device functions return `void`, so an error is *reported*, not
returned: through `pfnSetErrorCb` in the core-layer callbacks that
`D3D10DDIARG_CREATEDEVICE` supplies. 055 records that callback on the device;
**056** is the first to call it.

## What 056 must not assume

- **There is not one transport link per process shared with the ICDs yet.**
  FR-007 needs feature 053's device-global object identity. Today `UmdRoot`
  holds its own `UmdConnection` with its own rvk context, and an allocation
  made through it is not nameable through the Vulkan ICD's. The connection is a
  constructor argument, so the join is a different object passed in, not a
  rewrite - but 056 must not write code that assumes a Vulkan handle and a D3D
  handle name the same host object.
- **No feature level is reported.** Adding one is adding a version word to
  `DdiVersionList` *after* the thing it promises is implemented and tested.
- **The private-data records are not written.** 055 owns only the envelope
  (`contracts/private_data.md`); the allocation and resource descriptions nest
  inside it and are 056's and 057's, each with its own hostile table and fuzz
  target.
