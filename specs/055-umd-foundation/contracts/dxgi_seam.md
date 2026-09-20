# Contract: the DXGI DDI seam inside `CreateDevice`

Source of truth: `dxgiddi.h`
(`97ccf125b8ff36f444acf543e0dc5e7cd1924deb5be9a156d19d45057cc6b66c`),
`d3d10umddi.h` and Microsoft's *Supporting the DXGI DDI*.

**Read this if you are feature 056 or 058.** 055 owns the seam - which
revision, which callbacks, which device, and a non-null tested slot for every
entry. 056 and 058 own the bodies of the nine slots marked below and nothing
else in this file.

## There is no DXGI export and no DXGI entry point

`D3D10DDIARG_CREATEDEVICE` (`d3d10umddi.h:7118`) carries
`DXGI_DDI_BASE_ARGS DXGIBaseDDI` **by value**. The runtime puts its own
callbacks in `pDXGIBaseCallbacks` and a pointer to an empty functions struct in
one arm of the union; the driver fills that struct. For D3D12 the same table
arrives as `D3D12DDI_TABLE_TYPE_DXGI` (= 3) through `pfnFillDDITable`.

```c
typedef struct DXGI_DDI_BASE_ARGS {
    DXGI_DDI_BASE_CALLBACKS *pDXGIBaseCallbacks;   // in
    union {
        DXGI1_6_1_DDI_BASE_FUNCTIONS *pDXGIDDIBaseFunctions6_1;   // WDDM 2.2.2+
        DXGI1_5_DDI_BASE_FUNCTIONS   *pDXGIDDIBaseFunctions6;     // WDDM 2.1.1+
        DXGI1_4_DDI_BASE_FUNCTIONS   *pDXGIDDIBaseFunctions5;     // WDDM 2.0+
        DXGI1_3_DDI_BASE_FUNCTIONS   *pDXGIDDIBaseFunctions4;     // WDDM 1.3+
        DXGI1_2_DDI_BASE_FUNCTIONS   *pDXGIDDIBaseFunctions3;     // Win8+
        DXGI1_1_DDI_BASE_FUNCTIONS   *pDXGIDDIBaseFunctions2;
        DXGI_DDI_BASE_FUNCTIONS      *pDXGIDDIBaseFunctions;
    };
} DXGI_DDI_BASE_ARGS;
```

## Choosing the revision

`DxgiBaseTable` takes the revision from the **DDI interface version the runtime
asked for in `D3D10DDIARG_CREATEDEVICE::Interface`**, never from a guess and
never from the widest struct the header declares. At
`D3DWDDM3_2_DDI_INTERFACE_VERSION` (`0x000B002D`) that is
`DXGI1_6_1_DDI_BASE_FUNCTIONS`, 21 slots.

Refusals, each with a test:

| Case | Answer |
|---|---|
| `pDXGIBaseCallbacks` is null | `E_INVALIDARG`; the device is not created |
| the functions union is null | `E_INVALIDARG`; the device is not created |
| `Interface` names a revision this driver does not serve | `E_NOINTERFACE`, before anything is written |

Writing 21 slots into the struct a narrower revision names would write past the
runtime's memory, which is why the revision is decided before the first store
and why the decision is one object's, not a branch in the create.

**The callbacks pointer is recorded, never the individual callbacks.**
Microsoft: "The driver should record the pointer to the DXGI callback function
table ... because the Direct3D runtime can change the address of the callback
function whenever there is no thread inside the user-mode display driver."
`DXGI_DDI_BASE_CALLBACKS` holds `pfnPresentCb`,
`pfnPresentMultiplaneOverlayCb`, `pfnPresentMultiplaneOverlay1Cb`,
`pfnSubmitPresentBltToHwQueueCb` and `pfnSubmitPresentToHwQueueCb`.

## The 21 slots of `DXGI1_6_1_DDI_BASE_FUNCTIONS`

Every slot is non-null and every slot has a test. "Refusal" means the result a
driver that does not support the operation is documented to return, not a
crash and not a silent success.

| # | Slot | 055 answers | Whose |
|--:|---|---|---|
| 1 | `pfnPresent` | `E_NOTIMPL` | **056** (through feature 046's path), then **058** |
| 2 | `pfnGetGammaCaps` | `S_OK` with zero control points and no scale-and-offset support: this adapter has no gamma ramp | 055 |
| 3 | `pfnSetDisplayMode` | `E_NOTIMPL` | **056** |
| 4 | `pfnSetResourcePriority` | `S_OK`, ignored: priority is advisory and this driver has no eviction order of its own | 055 |
| 5 | `pfnQueryResourceResidency` | `E_NOTIMPL` until a resource exists | **056** |
| 6 | `pfnRotateResourceIdentities` | `E_NOTIMPL` | **056** |
| 7 | `pfnBlt` | `E_NOTIMPL` | **056** |
| 8 | `pfnResolveSharedResource` | `E_NOTIMPL` | **056** |
| 9 | `pfnBlt1` | `E_NOTIMPL` | **056** |
| 10 | `pfnOfferResources1` | `E_NOTIMPL`: offer and reclaim need a resource, and this driver never discards one | **056** decides whether to support it at all |
| 11 | `pfnReclaimResources` | `E_NOTIMPL`, same reason | **056** |
| 12 | `pfnGetMultiplaneOverlayCaps` | `S_OK` with zero planes and no caps: no overlay support | 055; **058** if host composition gains planes |
| 13 | `pfnGetMultiplaneOverlayGroupCaps` | `S_OK` with zero groups | 055 / **058** |
| 14 | `pfnReserved1` | `E_NOTIMPL` - reserved by the header, never called; the slot exists so the struct is fully written | 055 |
| 15 | `pfnPresentMultiplaneOverlay` | `E_NOTIMPL` (no overlay support is reported) | **058** |
| 16 | `pfnReserved2` | `E_NOTIMPL` | 055 |
| 17 | `pfnPresent1` | `E_NOTIMPL` | **056**, then **058** |
| 18 | `pfnCheckPresentDurationSupport` | `S_OK` with no supported duration: this adapter does not do custom present duration | 055 |
| 19 | `pfnTrimResidencySet` | `S_OK`, nothing trimmed: residency is the KMD's here | 055 |
| 20 | `pfnCheckMultiplaneOverlayColorSpaceSupport` | `S_OK` with support 0 | 055 |
| 21 | `pfnPresentMultiplaneOverlay1` | `E_NOTIMPL` | **058** |

A slot that answers `S_OK` with an all-zero answer is not a shortcut: each one
is a capability this adapter genuinely does not have, and each has a test that
asserts the zeroes reached the runtime's buffer and nothing past it was
written. A slot that answers `E_NOTIMPL` is a capability that will exist; its
test asserts the refusal and that the device was not removed by it.

## `DXGI_STATUS_NO_REDIRECTION` - the one-value decision

*Supporting the DXGI DDI*: a driver whose hardware is not shared with a
Direct3D 9 implementation "must return the `DXGI_STATUS_NO_REDIRECTION` value
instead of the `S_OK` value from its `CreateDevice(D3D10)` function". That
tells DXGI not to use the shared-resource presentation path with DWM, and to
call `PresentDXGI` to move rendered data instead.

**This driver never answers it.** It will have a D3D9 UMD (feature 064) and it
wants the redirection path: that is how DWM composites, and it is what feature
058 replaces with flips of host images. So when 056 makes `CreateDevice`
succeed, it succeeds with `S_OK`.

## What a present will cost, and why 055 does not build it

Recorded here so 056 does not rediscover it. From `docs/kb/07-usermode.md`
(feature 046's live measurement on the real guest): with no UMD present, DWM
composites on WARP and `DxgkDdiPresent` is called twice in a whole boot, both
times with no allocation list - so nothing in the miniport's present path has
ever carried a frame. The moment `CreateDevice` succeeds, that changes: DWM
creates a real device on this adapter, DXGI's redirection path starts naming
allocations, and `PresentBlit` (`src/core/present/`) begins to move rows.
056's present is therefore the first thing in this project that makes the
miniport's present path live, and its first test should be that
`adapter/0 present_rows` in a state dump stops being zero.
