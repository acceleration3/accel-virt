# Research: rvkumd.dll - the user-mode driver and DXGI

Every decision below was checked against the tree on 2026-09-20 (branch
`055-umd-foundation`, from `d8e68546`) and against the sources named here.
Nothing in this document is recalled: each fact names the header, the binary or
the page it was read from, and every fetched artefact is pinned by SHA-256 the
way `tools/fetch_windows_kit.sh` and `tools/re/binaries.txt` pin theirs.

## 0. The evidence, pinned

| Artefact | Provenance | SHA-256 |
|---|---|---|
| `d3d10umddi.h` | `Microsoft.Windows.WDK.x64` 10.0.26100.6584, `c/Include/10.0.26100.0/um/` | `61899403d94840fab282dbb7da6faf234e2954bbdb47e3455f0f0572eb4e723a` |
| `d3d12umddi.h` | same package | `d338e00bbd079cfed42434b5bd8cb9c658238cfe22747fb7d3b7cff3a3cabc28` |
| `d3dumddi.h` | same package | `099fad9e7e4faff6f7699ecd331edf4ff783396b2714280e219593bee3fb422c` |
| `dxgiddi.h` | `Microsoft.Windows.SDK.CPP` 10.0.26100.9169, `c/Include/10.0.26100.0/um/` | `97ccf125b8ff36f444acf543e0dc5e7cd1924deb5be9a156d19d45057cc6b66c` |
| `d3dkmthk.h`, `d3dukmdt.h` | same SDK package, `shared/` - already fetched today | (in the kit) |
| `d3d12core.dll` 10.0.26100.x | Microsoft symbol server, already pinned in `tools/re/binaries.txt` | `70710de807f188a85a3f5ad3a729a41bfa7a92b7621ceeaf38059f5640399267` |
| `d3d11.dll` 10.0.26100.9278 | Microsoft symbol server, `msfetch.py get d3d11.dll --winver 11-24H2` | `ae2777002f5a0ff543b5b9bec5124746559fd01d6c7c0c216d3194632f084502` |

The two WDK `um/` headers and `dxgiddi.h` are **not in the kit subset today**:
`tools/fetch_windows_kit.sh` takes the WDK's `km/` and `shared/` whole and only
a named list of SDK `um/` headers. T001 adds them (the WDK package's `um/`
directory is 120 MB; three named headers are 1.6 MB of it).

`d3d11.dll` joins `tools/re/binaries.txt`: it is where the D3D10/11 runtime's
side of this contract is written down, as `opengl32.dll` is for feature 050.

## 1. What a D3D UMD DLL must export, and who calls it

**Decision.** `rvkumd.dll` exports exactly two undecorated names for this
feature, and a third when feature 064 arrives:

| Export | Called by | Signature |
|---|---|---|
| `OpenAdapter10_2` | `d3d11.dll` (the D3D10/10.1/11 runtime) | `HRESULT APIENTRY (D3D10DDIARG_OPENADAPTER *)` |
| `OpenAdapter12` | `D3D12Core.dll` | `HRESULT APIENTRY (D3D12DDIARG_OPENADAPTER *)` |
| `OpenAdapter9` | `d3d9.dll` - **feature 064** | `HRESULT APIENTRY (D3DDDIARG_OPENADAPTER *)` |

**Rationale.**
- `strings d3d11.dll` (pinned above) holds the exact literals `OpenAdapter10`,
  `OpenAdapter10_2` and `OpenAdapter_D3D11On12`, and the diagnostics
  `Failed to retrieve OpenAdapter10` / `Failed to retrieve OpenAdapter10_2`.
  So the runtime resolves both by name and says which one it could not find.
- `strings d3d12core.dll` holds `OpenAdapter12`, `LoadUMD`,
  `QAI (UMDRIVERNAME)` and `Driver does not have OpenAdapter entrypoint`.
  There is no `OpenAdapter12` string anywhere in `d3d12umddi.h`: the header
  declares only `PFND3D12DDI_OPENADAPTER` and `D3D12DDIARG_OPENADAPTER`
  (`d3d12umddi.h:2686-2694`), so the binary is the only record of the name.
- Microsoft's own documentation agrees for the D3D11 half: "The Direct3D
  runtime next calls the user-mode display driver's **OpenAdapter10_2**
  function through the DLL's export table to open an instance of the graphics
  adapter. The **OpenAdapter10_2** function is the DLL's only exported
  function." and "You must implement **OpenAdapter10\_2** in your Direct3D
  version 11 drivers."
  (learn.microsoft.com, *Initializing Communication with the Direct3D Version
  11 DDI*).

**Alternatives rejected.** `OpenAdapter10` as well (the docs call `10_2` a
superset and the runtime prefers it; exporting both doubles the surface for no
caller this project has); `OpenAdapter_D3D11On12` (that is the D3D11-on-12
mapping layer's entry, which would put Microsoft's `d3d11on12.dll` between the
runtime and this driver - feature 056 decides whether to take that road, and it
is not the foundation).

## 2. How DXGI and the runtimes find the DLL

**Decision.** The INF writes, in the adapter's software key
(`[Rvkgpu_SoftwareSettings]`, where `VulkanDriverName` and `OpenGLDriverName`
already live), one `REG_MULTI_SZ` with **four** entries in Direct3D version
order:

```inf
HKR,, UserModeDriverName, 0x00010000, "rvkumd.dll", "rvkumd.dll", "rvkumd.dll", "rvkumd.dll"
```

No `UserModeDriverNameWow` (feature 061 owns the 32-bit arm, §11), and
`rvkumd.dll` joins `[Rvkgpu_Files]` and the catalog, copied to DIRID 13 with
`rvkgpu.sys` as `rvkvk.dll` and `rvkgl.dll` are.

**Rationale.**
- `d3dkmthk.h:2467` `KMTQAITYPE_UMDRIVERNAME = 1`, answered into
  `D3DKMT_UMDFILENAMEINFO { KMTUMDVERSION Version; WCHAR UmdFileName[MAX_PATH]; }`
  (`d3dkmthk.h:1944`). `KMTUMDVERSION` (`d3dkmthk.h:1933`) is
  `DX9 = 0, DX10, DX11, DX12, DX12_WSA32, DX12_WSA64`. The caller says *which*
  Direct3D it is and the kernel answers *which file*, so the value is a list
  indexed by that enum, not one name.
- `d3d12core.dll` carries the literal `QAI (UMDRIVERNAME)` beside `LoadUMD`:
  D3D12Core asks the kernel, it does not read the registry itself.
- Microsoft's *Enabling Support for the Direct3D Version 11 DDI* gives the
  three-entry form verbatim -
  `HKR,, UserModeDriverName, %REG_MULTI_SZ%, umd9.dll, umd10.dll, umd11.dll`,
  and `umd.dll, umd.dll, umd.dll` where one DLL serves all three - and the
  D3D12 page says a D3D12 driver "should append the user-mode driver file name
  to the `UserModeDriverName{Wow}` registry key(s)", making it four.
- The value is a **bare file name**, not a `%13%` path: dxgkrnl's
  `PostProcessUMDFileName` (already read for feature 050, docs/kb/07-usermode.md)
  turns it into the path the runtime loads, and it is that translation that
  makes a UMD work inside a VM container. `VulkanDriverName` is the opposite
  case and is an absolute path, because the Vulkan loader reads it raw.

**What is already true.** `src/km/rvkgpu.inf:47` already writes
`HKR,, UserModeDriverName, 0x00010000, "rvkumd.dll"` - a one-entry list, put
there in feature 006 because `DXGADAPTER::Initialize` refuses an adapter whose
software key has no `UserModeDriverName` at all. Nothing loads the DLL at
adapter start; only the string is read. 055 turns that placeholder into the
real four-entry list and ships the file it names.

**Alternatives rejected.** Four different DLLs (there is one translation core
here, and a second DLL would be a second copy of the transport in the same
process); a `%13%` absolute path (the container translation above would be
bypassed, and no shipping driver writes one).

## 3. What the D3D10/11 runtime does with the adapter it opened

**Decision.** `OpenAdapter10_2` fills `D3D10DDIARG_OPENADAPTER` and answers
`S_OK`; everything else follows from the table it returns.

```c
typedef struct D3D10DDIARG_OPENADAPTER {
    D3D10DDI_HRTADAPTER            hRTAdapter;         // in
    D3D10DDI_HADAPTER              hAdapter;           // out
    UINT                           Interface;          // in
    UINT                           Version;            // in
    CONST D3DDDI_ADAPTERCALLBACKS* pAdapterCallbacks;  // in
    union { D3D10DDI_ADAPTERFUNCS   *pAdapterFuncs;
            D3D10_2DDI_ADAPTERFUNCS *pAdapterFuncs_2; };  // out
} D3D10DDIARG_OPENADAPTER;                             // d3d10umddi.h:7637
```

`D3D10_2DDI_ADAPTERFUNCS` (`d3d10umddi.h:7554`) has **five** slots:
`pfnCalcPrivateDeviceSize`, `pfnCreateDevice`, `pfnCloseAdapter`,
`pfnGetSupportedVersions`, `pfnGetCaps`.

`D3DDDI_ADAPTERCALLBACKS` (`d3dumddi.h`) has three:
`pfnQueryAdapterInfoCb`, `pfnGetMultisampleMethodListCb`, and from WDDM 2.4
`pfnQueryAdapterInfoCb2`. The documented use is "The user-mode display driver
should call the **pfnQueryAdapterInfoCb** adapter callback function to query
for the graphics hardware capabilities from the display miniport driver" - the
private-data channel the miniport's `DxgkDdiQueryAdapterInfo` already answers.

Versioning is explicit and not inferred from a failed open: "Instead of the
Direct3D API relying upon failure of the driver's `OpenAdapter10_2` function to
indicate no support for a particular version, the driver must explicitly list
the DDI versions it supports. The Direct3D runtime calls the user-mode display
driver's **GetSupportedVersions** function". The word for one version is
`(interface << 32) | (build << 16)`; at WDDM 3.2 the D3D11 interface is
`D3DWDDM3_2_DDI_MINOR_VERSION = 45` with `D3DWDDM3_2_DDI_BUILD_VERSION = 1`
(`d3d10umddi.h:7969-7974`), so
`D3DWDDM3_2_DDI_INTERFACE_VERSION = (11 << 16) | 45 = 0x000B002D` and
`D3DWDDM3_2_DDI_SUPPORTED = 0x000B002D00010000`.

`pfnCalcPrivateDeviceSize` then answers how many bytes of the **runtime's**
memory the driver wants for its device, and `pfnCreateDevice` is handed that
block as `hDrvDevice`. The driver never allocates its own device: that is the
single most load-bearing shape difference between this DDI and the ICDs.

**What 055 answers and what it declines.** `pfnGetSupportedVersions` and
`pfnCalcPrivateDeviceSize` and `pfnCloseAdapter` are 055's in full.
`pfnGetCaps` answers the caps whose data 055 owns (adapter identity and memory,
§6) and `E_INVALIDARG` - the documented not-supported result for a
`D3D10_2DDICAPS_TYPE` a driver does not serve - for every other type, each with
a test naming the type. `pfnCreateDevice` builds the device object model and
the DXGI seam (§5) and then, until feature 056 fills
`D3DWDDM2_6DDI_DEVICEFUNCS`, answers `E_NOTIMPL`: a device that cannot draw is
not offered as one. That refusal is a tested behaviour, not a gap
(constitution VI and VIII).

## 4. What D3D12Core does

**Decision.** `OpenAdapter12` fills `D3D12DDIARG_OPENADAPTER`
(`d3d12umddi.h:2686`) - the same shape minus `Interface` and `Version`, which
D3D12 carries per table instead - and answers `S_OK`.

`D3D12DDI_ADAPTERFUNCS` (`d3d12umddi.h:2674`) has **eight** slots:
`pfnCalcPrivateDeviceSize`, `pfnCreateDevice`, `pfnCloseAdapter`,
`pfnGetSupportedVersions`, `pfnGetCaps`, `pfnGetOptionalDDITables`,
`pfnFillDDITable`, `pfnDestroyDevice`.

The two that have no D3D11 counterpart are what makes D3D12 a different animal:

```c
typedef HRESULT (APIENTRY *PFND3D12DDI_GETOPTIONALDDITTABLES)(
    D3D12DDI_HADAPTER, UINT32 *puEntries, D3D12DDI_TABLE_REQUEST *);
typedef HRESULT (APIENTRY *PFND3D12DDI_FILLDDITTABLE)(
    D3D12DDI_HADAPTER, D3D12DDI_TABLE_TYPE, VOID *, SIZE_T, UINT, D3D12DDI_HRTTABLE);
```

`D3D12DDI_TABLE_TYPE` (`d3d12umddi.h:2487`) has 27 values;
`DEVICE_CORE = 0`, `COMMAND_LIST_3D = 1`, `COMMAND_QUEUE_3D = 2`, `DXGI = 3`
are the four a device needs. So D3D12 does not hand out one device-functions
struct: it asks the driver to fill a named table of a named size, repeatedly.
The newest revision in this kit is build 110 on interface `R8`
(`D3D12DDI_SUPPORTED_0110`, `d3d12umddi.h:13656`).

**055's share.** The adapter half: `OpenAdapter12`, `pfnGetSupportedVersions`,
`pfnCalcPrivateDeviceSize`, `pfnCloseAdapter`, `pfnGetCaps` over the same
identity data, `pfnGetOptionalDDITables` answering zero tables, and
`pfnFillDDITable` answering `E_NOTIMPL` for every type until 057 fills them.
`pfnCreateDevice` answers `E_NOTIMPL` for the same reason `pfnCreateDevice`
does on the D3D11 side. The device and command-list tables are feature 057's.

**Which calls D3D12Core makes at device creation** is FR-002's question, and it
is answered in §12 from `d3d12core.dll` symbolized in a Ghidra workspace. Two
notes for anyone repeating it: `AV_PYGHIDRA` must name a Python with
`pyghidra` installed, and the wheel Ghidra ships
(`Ghidra/Features/PyGhidra/pypkg/dist/pyghidra-3.1.0-py3-none-any.whl`) is
where to get one; and Ghidra refuses a project path with a dot-prefixed
element, so `tools/re/build_workspace.sh`'s `re/ghidra-proj` cannot be used
from a worktree under `.claude/worktrees/` - the project goes elsewhere and the
script takes the directory as an argument. Importing the two D3D binaries
alone, rather than every row of `tools/re/binaries.txt`, is minutes rather than
an hour.

## 5. The DXGI seam belongs to CreateDevice, not to the adapter

**Decision.** There is no separate DXGI entry point and no separate DLL export.
`D3D10DDIARG_CREATEDEVICE` (`d3d10umddi.h:7118`) carries a
`DXGI_DDI_BASE_ARGS DXGIBaseDDI` by value; the runtime puts its callbacks in
`pDXGIBaseCallbacks` and a pointer to an empty functions struct in one arm of
the union, and the driver fills it. For D3D12 the same table arrives as
`D3D12DDI_TABLE_TYPE_DXGI` through `pfnFillDDITable`.

Which arm of the union is live is chosen by the DDI interface version the
runtime asked for. At WDDM 3.2 that is `DXGI1_6_1_DDI_BASE_FUNCTIONS`
(`dxgiddi.h:798`), 21 slots:

`pfnPresent`, `pfnGetGammaCaps`, `pfnSetDisplayMode`, `pfnSetResourcePriority`,
`pfnQueryResourceResidency`, `pfnRotateResourceIdentities`, `pfnBlt`,
`pfnResolveSharedResource`, `pfnBlt1`, `pfnOfferResources1`,
`pfnReclaimResources`, `pfnGetMultiplaneOverlayCaps`,
`pfnGetMultiplaneOverlayGroupCaps`, `pfnReserved1`,
`pfnPresentMultiplaneOverlay`, `pfnReserved2`, `pfnPresent1`,
`pfnCheckPresentDurationSupport`, `pfnTrimResidencySet`,
`pfnCheckMultiplaneOverlayColorSpaceSupport`, `pfnPresentMultiplaneOverlay1`,
`pfnReclaimResources1`.

Microsoft is explicit about the callbacks: "The driver should record the
pointer to the DXGI callback function table ... rather than record the
individual pointer to the DXGI callback function because the Direct3D runtime
can change the address of the callback function whenever there is no thread
inside the user-mode display driver." So the device holds
`DXGI_DDI_BASE_CALLBACKS *`, never a copied `pfnPresentCb`.

**055's share.** Every one of the 21 slots is non-null and every one is tested
(FR-003). The ones that need no resource - `pfnGetGammaCaps`,
`pfnSetResourcePriority`, `pfnQueryResourceResidency`,
`pfnGetMultiplaneOverlayCaps`, `pfnGetMultiplaneOverlayGroupCaps`,
`pfnCheckPresentDurationSupport`, `pfnCheckMultiplaneOverlayColorSpaceSupport`,
`pfnTrimResidencySet`, the two reserved slots and the offer/reclaim pair -
answer the documented result for a driver that does not support them.
`pfnPresent`, `pfnPresent1`, `pfnBlt`, `pfnBlt1`, `pfnRotateResourceIdentities`,
`pfnSetDisplayMode`, `pfnResolveSharedResource` and the two multiplane presents
name allocations, so their bodies are feature 056's and 058's; 055 gives each a
slot that answers `E_NOTIMPL` and a test that it does, and the seam - which
revision, which callbacks, which device - is 055's and is finished here.

**One consequence worth writing down.** *Supporting the DXGI DDI* says a driver
whose hardware is not shared with a D3D9 implementation "must return the
`DXGI_STATUS_NO_REDIRECTION` value instead of the `S_OK` value from its
`CreateDevice(D3D10)` function", which tells DXGI not to use the shared-resource
presentation path with DWM and to call `PresentDXGI` instead. This driver will
have a D3D9 UMD (feature 064) and wants the redirection path (it is how DWM
composites, and what feature 058 replaces), so `CreateDevice` answers `S_OK`
and never `DXGI_STATUS_NO_REDIRECTION`. Recorded here because it is a one-value
decision with a large consequence and nothing else in the tree says it.

## 6. Adapter identity, and what "DXGI sees a real GPU" actually depends on

**Finding, and a scope correction to the spec's US2.** Whether DXGI lists an
adapter as hardware is **not** the UMD's to say. `D3DKMT_ADAPTERTYPE`
(`d3dkmthk.h:2088`) - `RenderSupported`, `DisplaySupported`, `SoftwareDevice`,
`Paravirtualized`, `ComputeOnly`, `Detachable` and the rest - is answered by
dxgkrnl for `KMTQAITYPE_ADAPTERTYPE` (15) and
`KMTQAITYPE_ADAPTERTYPE_RENDER` (57) out of what the miniport is, not out of
anything the UMD returns. `rvkgpu` is a PnP display device with a VidPn, so
`SoftwareDevice` is already clear; nothing in 055 can set or clear it.

What the UMD does decide is whether the adapter is listed **at all** as a D3D
adapter, and at which feature level: DXGI enumerates an adapter whose UMD
loads and whose `GetSupportedVersions` names a version the runtime knows.
Feature 046's live measurement is the negative proof already in the tree
(docs/kb/07-usermode.md): "The display class key has no `UserModeDriverName`:
there is no D3D UMD, so DWM composes with WARP".

**Decision.** `AdapterIdentity` answers, from the capset and the KMD:
the adapter LUID (`Kmt::adapter_luid`, feature 045 T021), the PCI vendor and
device ids (`AdapterSearch`/`PciIdentity`, already in the tree), the
description, and dedicated/shared memory from
`Kmt::query_video_memory` (`D3DKMTQueryVideoMemoryInfo`, also 045 T021) - the
same two calls the Vulkan ICD already makes for the same adapter, so SC-002's
"the same LUID, ids and memory Vulkan reports" is true by construction rather
than by agreement.

DXCore's attribute queries are answered out of the same object: it asks for
`DXCORE_ADAPTER_ATTRIBUTE_D3D11_GRAPHICS` / `_D3D12_GRAPHICS`, which follow
from the versions `GetSupportedVersions` names. Until 056 and 057 name one,
neither is claimed - constitution VI, and FR-004 says so.

## 7. Where the code lives, and how it is tested on Linux

**Decision.**
- `src/icd/d3d/` - the UMD's decisions, portable C++23, beside
  `src/icd/vulkan/` and `src/icd/opengl/`, over the pieces all three share in
  `src/icd/common/` (`ThreadLock`, `LogSink`, `LockedTransport`, `SlicedWait`,
  `ExtensionPolicy` is Khronos-only and is not used here).
- `src/icd/d3d/ddi/` - **mirror records** of the WDK structures the DDI passes,
  one per header, named for the WDK type
  (`D3d10ddiargOpenAdapterRecord`, ...), public fields keeping the WDK's names.
- `src/icd/d3d/umd_entry_points.{hpp,cpp}` - the function-table thunks, each
  one line into the object behind the handle. This is the same ABI-boundary
  exception `src/core/ddi/entry_points.cpp` takes, for the same reason and with
  the same proof obligation.
- `src/um/d3d/` - the Windows shim: the two DLL exports, the library that owns
  the root's storage, `rvkumd.def`, and `umd_record_layout.cpp`, which proves
  every mirror record against the real WDK header at compile time exactly as
  `src/km/ddi_record_layout.cpp` does for the kernel DDIs.

**Why the thunks are portable and not in `src/um`.** On x64 Windows `APIENTRY`
(`__stdcall`) is the only calling convention there is, so a table of portable
function pointers *is* the ABI. The tests then drive the same table the
runtime would, from Linux, which is the whole of constitution II's promise and
is how feature 050 proved `rvkgl.dll` before a guest ever booted. It stops
being true for x86, where `__stdcall` is real and exports are decorated - which
is one more reason the 32-bit arm is feature 061's (§11).

**The record rule needs widening (PATCH amendment).** `tools/header_scan.py`
exempts `*Record` structs from the private-members rule only under
`src/core/ddi/` (`RECORD_DIRECTORY`, line 51). The UMD DDI records are the same
kind of thing - memory the runtime owns, WDK names kept so they grep to the
header, offsets proven in a shim translation unit - so `RECORD_DIRECTORY`
becomes a tuple and gains `src/icd/d3d/ddi/`. CLAUDE.md's "Adaptations" section
and the constitution's Principle III wording are amended to say records, not
just kernel records.

**The fake runtime.** `FakeD3dRuntime` in `tests/harness/icd/` plays the role
`FakeOpengl32` plays for the OpenGL ICD: it loads the "DLL" (calls the root's
open), resolves the adapter functions, and calls them in the order the real
runtime does. It is refusable at every step, records what it was given, and -
once FR-002's recorded sequence exists - replays that sequence exactly.

## 8. One process, one link - and what 053 owes it

**Decision for 055.** The UMD opens its own connection: `AdapterSearch` finds
the rvkgpu adapter by PCI identity, `GuestConnection` makes the D3DKMT device
and paging queue, and a `KmtTransport` of `SubApi::vulkan` gives one rvk
context - the same three steps `icd::Connection` and `glicd::GlConnection`
take, with one difference: it is opened once per process at the first
`OpenAdapter*` rather than once per instance, because the D3D DDI has no
instance and `OpenAdapter10_2`/`OpenAdapter12` in the same process must reach
the same host objects.

**What FR-007 asks and 053 must give.** "one link per process shared with the
Vulkan, OpenGL and OpenCL drivers" is not reachable from 055 alone. Each ICD is
its own DLL with its own root and its own `Connection`; a link shared across
four DLLs needs an object identity that is the *device's*, not one context's -
which is precisely what feature 053 builds ("device object identity must be
global, not per open", 053 spec, kb 11 risk 6). Until 053 merges, this driver
has its own context in a process that may also hold the Vulkan ICD's, and an
allocation made through one is not nameable through the other. Tasks T0xx in
Phase 7 carry the join; none of them is started here, and nothing 055 builds
assumes the single link (the connection is injected, so the sharing change is a
constructor argument, not a rewrite).

## 9. Device loss

**Decision.** `DeviceRemoval` is one object per device holding the reason and
whether it has been reported. The transport already answers a context fault
(`src/guest/transport/context_fault.*`, feature 044) and the ICDs already turn
it into `VK_ERROR_DEVICE_LOST`; the D3D spelling is
`DXGI_ERROR_DEVICE_REMOVED` (`0x887A0005`) with the first cause kept so
`GetDeviceRemovedReason` can name it, and `DXGI_ERROR_DEVICE_RESET`
(`0x887A0007`) when the fault was this device's own doing rather than the
adapter's. A removed device answers the removal code from every entry that
returns an `HRESULT` and asks the transport nothing, as `IcdRoot::lost` does.

The runtime is told through the core-layer callback `pfnSetErrorCb`, which
`D3D10DDIARG_CREATEDEVICE` supplies in `p11UMCallbacks` - the driver has no
return value on most D3D11 device functions, so an error is reported, not
returned. Recording that callback is 055's; using it on a draw is 056's.

## 10. Hostile input: the private driver data the KMD must distrust

**Decision.** The UMD's allocation, resource and context private data is a
`src/proto` record with the shape the transport records already have
(magic, u16 version, u16 kind, then fields, then a reserved tail required
zero), decoded by one class with a hostile-input table and a libFuzzer target,
and the KMD refuses anything it does not recognise with
`STATUS_INVALID_PARAMETER` and one log line naming the offending value.

**Why it is not built in 055.** The record's *fields* are the description of a
resource, and what a resource is, is feature 056's (textures, buffers, their
formats and tiling) and 057's. Writing the record now would be writing a format
for objects that do not exist. What 055 does build is the **envelope**: a
`UmdPrivateDataRecord` header - magic `RVKU`, version, kind, byte length - that
every later record nests inside, with its own hostile table and fuzz target, so
the KMD's refusal path and its log line are proven before the first field
exists. FR-006 and SC-003 are met for the envelope in 055 and re-met per record
in 056 and 057. This is recorded as a deliberate narrowing of FR-006.

## 11. Scope moved

- **The 32-bit arm (`UserModeDriverNameWow`, the WoW64 build, host-visible
  memory below 4 GiB)** is feature 061's, as it already is for the Vulkan and
  OpenGL ICDs. There is no x86 toolchain arm in this build at all. What 061
  will need from 055 is written down in plan.md: the mirror records must be
  size-checked on x86 too (pointer-sized fields change every offset), the
  function-table thunks become genuinely `__stdcall` and the exports genuinely
  decorated, and `check_guest_image.py` needs an i386 machine mode.
- **Direct3D 9 (`OpenAdapter9`, `D3DDDIARG_OPENADAPTER`, the D3D9 device
  funcs)** is feature 064's, on the adapter/device/DXGI foundation this feature
  lays. The fourth entry of the `UserModeDriverName` list names `rvkumd.dll`
  from 055 onwards so that 064 is a DLL change and not an INF change.
- **Rendering** - device functions, resources, shaders, draws, and with them
  the feature levels `GetSupportedVersions` and DXCore report - is 056 and 057.
  055 reports no feature level and no D3D11/D3D12 DDI version it has not
  tested, which today means `GetSupportedVersions` answers the WDDM 3.2 word
  and `CreateDevice` answers `E_NOTIMPL` (§3).
- **Presenting frames** is 056 (through 046's path) and 058 (flips of host
  images). 055 builds the seam and the refusals.
- **The recorded D3D12Core call sequence (FR-002)** waits on a Ghidra
  workspace; the task is written and unstarted.

## 12. What D3D12Core actually does with the driver it loaded (FR-002)

Read from `d3d12core.dll` (pinned in `tools/re/binaries.txt`) with its public
PDB, in a Ghidra workspace built by `tools/re/import.py`. Addresses are that
binary's.

**`NDXGI::CUMDAdapter::OpenAdapter<0>` @180064008** is the whole of the open,
and it is stricter than the header suggests:

1. It zeroes **0x40 bytes** at `this+0x110` before the call - eight pointers,
   which is exactly `D3D12DDI_ADAPTERFUNCS`. A driver that writes fewer slots
   leaves zeroes, which is what the check below reads.
2. `GetProcAddress(module, "OpenAdapter12")`. Absent: it journals
   `"Driver does not have OpenAdapter entrypoint"` and answers `0x887A0002`
   (`DXGI_ERROR_NOT_FOUND`).
3. It calls it with a stack structure of four fields in the order
   `D3D12DDIARG_OPENADAPTER` declares: `hRTAdapter` = the `CUMDAdapter`,
   `hAdapter` = 0 (out), `pAdapterCallbacks` = `this+0xf8`,
   `pAdapterFuncs` = `this+0x110`.
4. A negative `HRESULT` is journaled as `"OpenAdapter12"` and returned as is.
5. **Then it validates what the driver wrote.** The out `hAdapter` must be
   non-zero, and the table slots at `+0x120`, `+0x128`, `+0x130`, `+0x138`,
   `+0x140` and `+0x148` must all be non-null - that is
   `pfnCloseAdapter`, `pfnGetSupportedVersions`, `pfnGetCaps`,
   `pfnGetOptionalDDITables`, `pfnFillDDITable` and `pfnDestroyDevice`, slots 2
   through 7. Slots 0 and 1 (`pfnCalcPrivateDeviceSize`, `pfnCreateDevice`) are
   not checked here.
6. If the check fails and `pfnCloseAdapter` is non-null and `hAdapter` is
   non-zero, it calls `pfnCloseAdapter` to give the driver its adapter back -
   then journals `"OpenAdapter12"` and answers `0x887A0004`
   (`DXGI_ERROR_UNSUPPORTED`).
7. On success `hAdapter` is kept at `this+0x150`, and every later table call
   passes it.

So a partially-filled table is not tolerated, and a refusal after a successful
open is cleaned up through `pfnCloseAdapter` - which is why 055 fills all eight
slots from the first commit and why `pfnCloseAdapter` must be safe to call on
an adapter no device was made on.

**`CCreateDeviceCache::LoadUMD` @18007a230** is what happens next, and the
first thing it asks the driver is the 3D pipeline level:

- guarded on a DDI version, it calls `pfnGetCaps` with
  `{ Type = 0x432, pInfo = NULL, pData = <4-byte in, 4-byte out>, DataSize = 8 }`
  - `D3D12DDICAPS_TYPE_0081_3DPIPELINESUPPORT1` (1074), whose
  `D3D12DDI_3DPIPELINESUPPORT1_DATA_0081` takes the runtime's highest supported
  level in and answers the driver's;
- if that answers a failure, it falls back to `pfnGetCaps` with
  `{ Type = 0x3ef, pInfo = NULL, pData = <4 bytes>, DataSize = 4 }` -
  `D3D12DDICAPS_TYPE_3DPIPELINESUPPORT` (1007) - and a failure there is
  journaled as `"Get3DPipelineLevel"`;
- the answer must be 1, 2, or 10 through 14
  (`D3D12DDI_3DPIPELINELEVEL_1_0_GENERIC = 1` upward). Anything else is
  journaled as `"Driver returned invalid pipeline caps"` with `0x887A0020`
  (`DXGI_ERROR_DRIVER_INTERNAL_ERROR`) and
  forced to 1.

**Consequence for 055 and 057.** `pfnGetCaps` answering `E_INVALIDARG` to both
pipeline-level types is a *supported* outcome - the runtime falls back and then
forces level 1 with a journal entry - so 055's blanket refusal does not crash
D3D12Core. But it means the first cap 057 must implement is the pipeline level,
and that until it does, D3D12 on this adapter is generic level 1. That is the
honest reporting constitution VI asks for, not a gap to paper over. The test
for `pfnGetCaps` names both types explicitly.

`d3d12core.dll` also carries `QAI (UMDRIVERNAME)`, `Removing device due to bad
UMD error.`, `UMD reported device removed.`, `UMD reported application bug.`
and `UMD reported error while device is removed.` - the four ways a UMD's
reported error is classified, which is what §9's `DeviceRemoval` has to be
right about once 056 starts calling `pfnSetErrorCb`.

**Still unread**, and left in T036: the D3D11 runtime's own sequence, and which
`pfnFillDDITable` types D3D12Core asks for before it gives up. Both are in the
same workspace and are a query away; neither changes anything 055 builds.
