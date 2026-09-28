# DDI call sequences in dxgkrnl.sys

Evidence for the L2 DDI conformance harness (`tests/harness/dxgkrnl/fake_dxgkrnl.cpp`,
see `docs/testing.md`). Every claim below is derived from Microsoft's
`dxgkrnl.sys` 10.0.26100.9444 (and `dxgmms2.sys` from the same package) as
imported into `re/ghidra-proj`, rebuilt by `tools/re/build_workspace.sh` from
`tools/re/binaries.txt`. The order dxgkrnl calls a miniport's `DxgkDdi*`
entry points, and the IRQL it uses, is what the harness must reproduce so the
guest driver is exercised the way Windows exercises it.

To re-derive any step, decompile the cited function, e.g.

```bash
export GHIDRA_INSTALL_DIR=/opt/ghidra JAVA_HOME=/usr/lib/jvm/java-26-openjdk
PY=~/.local/share/uv/tools/pyghidra-mcp/bin/python
$PY tools/re/q.py re/ghidra-proj wddm dxgkrnl.sys dec 'DpiFdoStartAdapter'
$PY tools/re/q.py re/ghidra-proj wddm dxgkrnl.sys xref DpiDxgkDdiStartDevice
```

Slot numbers are the `DRIVER_INITIALIZATION_DATA` field indices from
`docs/ref/driver_init_table.md`. Field names are from the WDK headers under
`third_party/windows-kit/include/`.

## How dxgkrnl dispatches a DDI (verified offset mapping)

dxgkrnl copies the miniport's filled-in DDI table into a per-adapter object
(the "miniport object", the pointer passed to every `DpiDxgkDdi*` thunk). In
that object the pointer for **slot N sits at byte offset `0x88 + 8*N`**. This
was read off the one-line thunks, each of which does nothing but an indirect
call through that offset:

| DDI | slot | miniport-object offset | thunk (name @ addr) |
|---|--:|---|---|
| `DxgkDdiAddDevice`            | 1  | `+0x90`  | `DpiDxgkDdiAddDevice @ 1403671bc` |
| `DxgkDdiStartDevice`          | 2  | `+0x98`  | `DpiDxgkDdiStartDevice @ 14043eacc` |
| `DxgkDdiStopDevice`           | 3  | `+0xa0`  | `DpiDxgkDdiStopDevice @ 14026a718` |
| `DxgkDdiRemoveDevice`         | 4  | `+0xa8`  | `DpiDxgkDdiRemoveDevice @ 140429eb8` |
| `DxgkDdiInterruptRoutine`     | 6  | `+0xb8`  | `DpiFdoMessageInterruptRoutine @ 14000bca0` |
| `DxgkDdiDpcRoutine`           | 7  | `+0xc0`  | `DpiFdoDpcForIsr @ 14000d3e0` |
| `DxgkDdiQueryChildRelations`  | 8  | `+0xc8`  | `DpiDxgkDdiQueryChildRelations @ 14019f968` |
| `DxgkDdiQueryChildStatus`     | 9  | `+0xd0`  | `DpiDxgkDdiQueryChildStatus @ 1403eb7f4` |
| `DxgkDdiQueryDeviceDescriptor`| 10 | `+0xd8`  | `DpiDxgkDdiQueryDeviceDescriptor @ 1403c4580` |

`DxgkDdiQueryAdapterInfo` (slot 17) is the exception: it is dispatched from the
`DXGADAPTER` object, not the miniport object. `DdiQueryAdapterInfo @ 1402f4e90`
indirect-calls through `DXGADAPTER+0x1c8` with the miniport context at
`DXGADAPTER+0x120`. `DxgkDdiControlInterrupt3` (slot 153) is read at
`DXGADAPTER+0x578` in the caps-validation code below.

---

## 1. Registration (already established; re-verified)

`DxgkInitialize` (displib) reaches `DpiInitialize @ 140082850` /
`DpiInitializeWin8` -> `DpiInitializeInternal @ 140082868` ->
`DpiInitializeEx @ 1402526d8`:

- `Version < 0x104E` -> `STATUS_REVISION_MISMATCH` (0xC0000059).
- The following slots must be non-NULL or the same status is returned:
  1 `DxgkDdiAddDevice`, 2 `DxgkDdiStartDevice`, 3 `DxgkDdiStopDevice`,
  4 `DxgkDdiRemoveDevice`, 5 `DxgkDdiDispatchIoRequest`,
  6 `DxgkDdiInterruptRoutine`, 7 `DxgkDdiDpcRoutine`,
  8 `DxgkDdiQueryChildRelations`, (9 `DxgkDdiQueryChildStatus` OR
  125 `DxgkDdiQueryConnectionChange`), 10 `DxgkDdiQueryDeviceDescriptor`,
  11 `DxgkDdiSetPowerState`, 13 `DxgkDdiResetDevice`, 14 `DxgkDdiUnload`.
  Slot 12 `DxgkDdiNotifyAcpiEvent` is not required.
- `DpiGetDriverDataSizeFromVersion @ 14019f340` never fails; it maps version to
  the number of table bytes copied; any version >= 0x11001 copies 0x608 bytes
  (the whole 193-field table).
- `IoAllocateDriverObjectExtension` is keyed by a constant; a second
  registration on the same driver object fails with
  `STATUS_OBJECT_NAME_COLLISION` and is logged.
- `DpiInitializeEx` installs `DpiDispatchPnp`, `DpiDispatchPower`,
  `DpiDispatchIoctl`, `DpiDispatchInternalIoctl`, `DpiDispatchSystemControl`,
  `DpiDispatchCreate`, `DpiDispatchCleanupAndClose`, `DpiAddDevice` (driver
  extension `AddDevice`) and `DpiDriverUnload @ 1402526c0`.

---

## 2. Adapter bring-up

All of PnP dispatch runs at `PASSIVE_LEVEL`: `IRP_MJ_PNP` is delivered by the
I/O manager at passive level, and the handlers below take fast mutexes
(`AcquireMiniportListMutex`), `ExAcquireResourceExclusiveLite` and create
system threads — none legal above passive.

### 2.1 AddDevice (`DpiAddDevice @ 140364d70`)

The driver-extension `AddDevice` builds the FDO and calls the miniport:

1. `DpiDxgkDdiAddDevice` (slot 1, `miniport+0x90`) at `DpiAddDevice+...` (call
   site line ~786). Then, whenever slot 60 is non-NULL (`miniport+0x268`),
   `DpiDxgkDdiLinkDevice @ 1400925cc` is called for every adapter, not only
   chained ones. A failure status fails the AddDevice; `NumberOfLinksInChain`
   above 0x100 fails it with `STATUS_QUOTA_EXCEEDED`; a non-zero count with
   `LeadLink` FALSE makes the adapter a secondary link. A NULL slot 60 skips
   all of this. So a registered LinkDevice must succeed with a chain of one led
   by the adapter (feature 006).
2. `DpiDxgkDdiExchangePreStartInfo @ 14026a338` (slot 126) is called here, at
   AddDevice time (call site ~1469) — before any StartDevice.
3. The FDO's PnP/Power dispatch and the DPC-for-ISR are wired up:
   `KeInitializeDpc(&fdo->Dpc, DpiFdoDpcForIsr, ...)`,
   `fdo->MajorFunction[IRP_MJ_PNP] = DpiFdoDispatchPnp`,
   `... [IRP_MJ_POWER] = DpiFdoDispatchPower`.

If AddDevice fails after the miniport's `DxgkDdiAddDevice` succeeded, dxgkrnl
calls `DpiDxgkDdiRemoveDevice` (slot 4) to unwind (call site ~1661).

### 2.2 `IRP_MN_START_DEVICE`

`DpiDispatchPnp` -> `DpiFdoDispatchPnp @ 140254a40`. The FDO holds a handler
table at `fdo+0x120` indexed by `MinorFunction`
(`handler = *(fdo + 0x120 + MinorFunction*8)`; see `DpiFdoDispatchPnp+...`).
`MinorFunction == 0` (`IRP_MN_START_DEVICE`) -> `DpiFdoHandleStartDevice`.
Table entries were installed in `DpiAddDevice`: index 0 StartDevice, 2
RemoveDevice, 4 StopDevice, 0x0d FilterResourceRequirements, 0x17
SurpriseRemoval.

`DpiFdoHandleStartDevice @ 140256a10`:

1. Forwards the IRP down to the lower (PDO/bus) stack synchronously
   (`IoForwardIrpSynchronously(fdo->LowerDevice, Irp)`); if that fails, start
   fails and no miniport DDI runs.
2. Runs the adapter-start work. By default (`DAT_140169e62` gate) it is run on
   a dedicated system thread: `PsCreateSystemThread(..., DpiFdoStartAdapterThread
   @ 14025c830, ctx)`; otherwise inline. Either way it reaches
   `DpiFdoStartAdapterThreadImpl @ 14025c93c` ->
   `DpiFdoStartNonLdaAdapter @ 14025d23c` ->
   (`DpiFdoCreateSysMmAdapter`, then) `DpiFdoStartAdapter @ 14025a7c8`.

### 2.3 The miniport DDI sequence inside `DpiFdoStartAdapter @ 14025a7c8`

This is the ordered sequence the harness must reproduce. For a normal render
FDO (device signature `0x74727044` subtype 2, not an indirect-display / MSBDD
anchored device) the calls are:

| # | DDI / action | slot | dxgkrnl call site | on failure |
|--:|---|--:|---|---|
| 1 | **Connect interrupt** | — | `DpiFdoConnectInterrupt @ 1404348ac` | start aborts with the connect status |
| 2 | Build `DXGKRNL_INTERFACE` callback table (0x240 bytes) | — | inline (`DpEvalAcpiMethod` … `DxgkDisconnectDoorbellCB`) | — |
| 3 | `DxgkDdiStartDevice` | 2 | `DpiDxgkDdiStartDevice @ 14043eacc` (`miniport+0x98`) | teardown (see 2.4) |
| 4 | `DxgkDdiQueryChildRelations` | 8 | `DpiFdoEnumChildDevices @ 140439d2c` -> `DpiDxgkDdiQueryChildRelations @ 14019f968` | start fails |
| 5 | `DxgkDdiQueryAdapterInfo` × several | 17 | `DxgkAddAdapter @ 140435870` -> `CreateAdapter @ 140435950` -> `DXGADAPTER::Initialize @ 1401a2818` -> `DdiQueryAdapterInfo @ 1402f4e90` | see per-type below |
| 6 | `DxgkDdiDisplayDetectControl` | 124 | `DpiDxgkDdiDisplayDetectControl @ 1403eb01c` (only if `adapter+0xe20 != 0`) | start fails |

Notes:

- **The interrupt is connected before `DxgkDdiStartDevice`** (step 1 precedes
  step 3 in `DpiFdoStartAdapter`). `DpiFdoConnectInterrupt` issues
  `IoConnectInterruptEx` with `Version = 3` (fully specified), message routine
  `DpiFdoMessageInterruptRoutine`, line routine `DpiFdoLineInterruptRoutine`,
  and the miniport context as `ServiceContext`. So a driver may receive an
  interrupt as soon as `DxgkDdiStartDevice` returns (and, strictly, its ISR is
  registered before it).
- `DxgkDdiEnumChildDevices` -> `QueryChildRelations` happens only when the
  adapter reports children (`adapter+0xa90 != 0`); it is called before
  `DxgkAddAdapter`.

### 2.4 QueryAdapterInfo types during start

`DXGADAPTER::Initialize @ 1401a2818` (reached from `DxgkAddAdapter`, i.e. after
`DxgkDdiStartDevice` has returned) issues `DdiQueryAdapterInfo @ 1402f4e90`
with `DXGKARG_QUERYADAPTERINFO.Type` at offset 0. Types and enum values from
`third_party/windows-kit/include/wdk-shared/d3dkmddi.h`:

| order | `Type` value | `DXGKQAITYPE_*` | condition | on failure |
|--:|--:|---|---|---|
| 1 | 1    | `DRIVERCAPS`          | always | **fatal** — Initialize returns, `DxgkAddAdapter` fails, start fails (`"Miniport failed DdiQueryAdapterInfo(DXGKQAITYPE_DRIVERCAPS)"`) |
| 2 | 15   | `PHYSICALADAPTERCAPS` | per physical adapter | logged at warning level (`bVar4`), start continues |
| 3 | 21   | `FRAMEBUFFERSAVESIZE` | cap bit `adapter+0x9bc & 0x800` | fatal (`"Failed to query frame buffer save area size"`) |
| 4 | 39   | `DIRTYBITTRACKINGCAPS`| WDDM ≥ 0x3200 and DDI ver > 0x11000 | logged, non-fatal |
| 5 | 47   | `64BITONLYCAPS`       | always | non-fatal; cap simply not set |
| 6 | 37   | `NATIVE_FENCE_CAPS`   | cap bit `adapter+0x9b8 & 0x800` (NativeGpuFence) | fatal (`"Failed to get DXGK_NATIVE_FENCE_CAPS"`) |

`FakeDxgadapter` (`tests/harness/dxgkrnl/fake_dxgadapter.cpp`) reproduces this table
and the checks below; `CapabilityRule` names each one. The `DRIVERCAPS` output
is `GetDriverCapsSizeFromDdiVersion(0x11007)` = 0x250 bytes, copied to
`DXGADAPTER+0x978`, so a field at `DXGADAPTER+X` is `DXGK_DRIVERCAPS+(X-0x978)`.
`PHYSICALADAPTERCAPS` is asked only when `WDDMVersion` >= 2.0 (and a failure
there keeps defaults); `FRAMEBUFFERSAVESIZE` only when
`MemoryManagementCaps.IoMmuSecureModeSupported` (bit 11) is set. A
`DXGKDDI_WDDMv1` answer with no capability passes every check.

The `DRIVERCAPS` reply is validated hard: e.g. a WDDMv2+ driver that does not
set `SchedulingCaps.MultiEngineAware` fails start; setting
`IndependentVidPnVSyncControl` without exposing `DxgkDdiControlInterrupt3`
(slot 153, read at `DXGADAPTER+0x578`) fails start; an unknown `WDDMVersion`
(`adapter+0xac8`) value fails start. Further checks, in order:
`MiscCaps.Reserved` bits (WDDM >= 2.4); `IndependentVidPnVSyncControl` without
`DxgkDdiControlInterrupt3`; `WDDMVersion` outside `DXGK_WDDMVERSION` (0x2105 and
0x2106 are not accepted) fails with `STATUS_REVISION_MISMATCH`; a version below
1.2 with any 1.2 capability (preemption caps, `SupportNonVGA`,
`SupportSmoothRotation`, `SupportPerEngineTDR`,
`PresentationCaps.SupportSoftwareDeviceBitmaps`, `PreemptionAware` or
`CancelCommandAware`, `SupportDirectFlip`, `SupportRuntimePowerManagement`,
`SupportSurpriseRemovalInHibernation`), below 1.3 with any 1.3 capability
(`FlipIndependent`, `CrossAdapterResource`, `HybridDiscrete`,
`MaxOverlayPlanes`), below 2.0 with `HybridIntegrated`; cross-adapter resource
tiers out of order; hybrid flags without `CrossAdapterResource`, both hybrid
flags, `HybridIntegrated` without outputs; `SupportPerEngineTDR`,
`SupportKernelModeCommandBuffer`, `SupportRuntimePowerManagement` and
`SupportMultiPlaneOverlay` without their DDIs (overlays also need planes, at
most 8 before 2.0 or 10 from 2.2, and `SupportDirectFlip`); colour transform
levels out of order or without `DxgkDdiSetTargetGamma` (WDDM >= 2.3); exactly
one of the two diagnostic DDIs (WDDM >= 2.4). Each of these returns
`STATUS_INVALID_PARAMETER` from start. After the cores are created, a full
adapter at 1.2 must set `SupportDirectFlip` and at 1.3 `FlipIndependent`.

Correction (feature 017): the below-1.2 and below-1.3 capability checks and the
full-adapter `SupportDirectFlip` / `FlipIndependent` checks run only when the
`ValidateWDDMCaps` registry value is set, which it is not by default;
`HybridIntegrated` below 2.0 is checked always. From 2.0, `CreateRenderCore`
also requires `FlipCaps.FlipOnVSyncMmIo` unless `MiscCaps.ComputeOnly`
("Driver reports WDDM version 2.0 or higher but does not support
FlipOnVSyncMmIo cap"), and an answered `PHYSICALADAPTERCAPS` makes
`MultiEngineAware` mandatory. `FakeDxgadapter::validate_wddm_caps()` turns the
gated checks on.

`DdiQueryAdapterInfo @ 1402f4e90` does **not** filter the `Type` — every
requested type is forwarded verbatim to slot 17 (relevant to §5) — and logs
`"Driver returned an invalid NTSTATUS code"` for any answer other than
`STATUS_SUCCESS`, `STATUS_NOT_IMPLEMENTED` (0xC0000002),
`STATUS_INVALID_PARAMETER` (0xC000000D) or `STATUS_NO_MEMORY` (0xC0000017).

### 2.4a The render core and VidMm's segments

After the capability checks, `DXGADAPTER::Initialize` calls
`ADAPTER_RENDER::CreateRenderCore @ 1401b13d8`. It reads the render DDIs from
dxgkrnl's copy of the table (`DXGADAPTER+0x1d0` .. `+0x300`, `+0x320`; slot 34
`DefaultDdiEscape` at `+0x250`, slot 47 `DefaultDdiStopCapture` at `+0x2b0` and
slot 49 `DefaultDdiCreateOverlay` at `+0x2c0` pin the mapping):

- With none of CreateDevice, DestroyAllocation, DescribeAllocation,
  OpenAllocation, CloseAllocation, GetStandardAllocationDriverData,
  SubmitCommand, PreemptCommand, BuildPagingBuffer, ResetFromTimeout,
  RestartFromTimeout, QueryCurrentFence, DestroyDevice, Present,
  CreateAllocation, Patch, Render, RenderKm, the swizzling-range, StopCapture
  and overlay DDIs registered, there is no render core.
- Otherwise every one of CreateDevice, DestroyAllocation, DescribeAllocation,
  OpenAllocation, CloseAllocation, GetStandardAllocationDriverData,
  SubmitCommand, PreemptCommand, BuildPagingBuffer, ResetFromTimeout,
  RestartFromTimeout, QueryCurrentFence (not needed from WDDM 2.0),
  ControlInterrupt (not needed with `MiscCaps.ComputeOnly`), DestroyDevice,
  CollectDbgInfo or CollectDbgInfo2, and Present must be registered - and for a
  WDDM 1.x `WDDMVersion` also CreateAllocation, Patch and Render - or start fails
  with `STATUS_REVISION_MISMATCH`.
- `NumberOfSwizzlingRanges` needs both swizzling-range DDIs;
  `PreemptionAware` or `CancelCommandAware` without `MultiEngineAware` fails
  with `STATUS_INVALID_PARAMETER`, as does `MultiEngineAware` below WDDM 2.0
  with no asymmetric node.

`FakeDxgadapter::create_render_core` applies this; the scenario tests name the
first missing slot, so the render DDI set's progress is visible feature by
feature (SubmitCommand at 008).

A render adapter's VidMm then creates its segments in
`CreatePhysicalAdapterSegments @ 14009de94` (dxgmms2): `DXGKQAITYPE_QUERYSEGMENT4`
with a null descriptor array for `NbSegment` (0 fails with
`STATUS_UNSUCCESSFUL`, "Driver must provide at least one segment"; above 32
too), then again with `SegmentDescriptorStride` 0x68 and the array.
`ValidateSegmentDescriptors @ 14009ec8c` then requires page-aligned sizes,
flags below bit 22 without `Agp`, `PreservedDuringHibernate` or
`PartiallyPreservedDuringHibernate` only (and not both) with
`PreservedDuringStandby`, no `Aperture` or `CpuVisible` on a segment with a CPU
host aperture, at most one UEFI frame-buffer range in at most one segment, and
`PagingBufferSegmentId <= NbSegment`. `FakeVidMm` applies it.

dxdiag's and DXGI's memory sizes come from `VIDMM_GLOBAL::GetTotalSegmentSize
@ 14010636c`. Each segment counts the smaller of its size and `CommitLimit`:
one with neither `Aperture` (0x1) nor `ReservedSysMem` (0x1000) is dedicated -
video memory, or system memory with `PopulatedFromSystemMemory` (0x40) - and
the aperture and reserved totals give the shared figure (the larger, capped).
The implicit system memory segment is `ReservedSysMem` (0x1180 and up,
`InitializePhysicalAdapterSegments @ 140116728`), so it is never dedicated.
The adapter reports the host GPU's local memory as a flagless segment 2 after
the aperture, which VidMm makes a `VIDMM_MEMORY_SEGMENT` without calling the
driver; the implicit segment is then id 3. With a capset that reports no local
memory the aperture is the only segment. `FakeVidMm` applies the
classification.

### 2.4b The GPU MMU (feature 017)

For a physical adapter whose `PHYSICALADAPTERCAPS.Flags` sets `GpuMmuSupported`
(bit 1), `ADAPTER_RENDER::Initialize @ 1401b3d4c` asks `DXGKQAITYPE_GPUMMUCAPS`
(13; input the physical adapter index, 4 bytes; output 0x18) and, per level,
`DXGKQAITYPE_PAGETABLELEVELDESC` (14; input `{WORD level, WORD adapter}`;
output 0x14). A failed query, a virtual address width outside 13..63 or a
level count outside 2..6 fails start with `STATUS_DEVICE_CONFIGURATION_ERROR`
(0xC0000182); a level's page table size must be a power of two. Every node's
`DXGK_NODEMETADATA` must then set `GpuMmuSupported` and no reserved flag (0xFFE0).
VidMm keys on `MemoryManagementCaps.VirtualAddressingSupported` (0x20) and
dxgkrnl on `GpuMmuSupported` (0x40); both are set.

VidMm's `InitializeGpuVaState @ 1400972cc` (dxgmms2) runs once segments exist
and adds an implicit system memory segment, id `NbSegment + 1`. It checks the
leaf table size, that every `PageTableSegmentId` and `PagingProcessPageTableSegmentId`
names a segment (the implicit one included), the leaf index bits, the update
mode (at most 2), and that directory levels agree for a level count other than
2; any failure fails start with `STATUS_NO_MEMORY`. `FakeGpuMmu` applies both
halves. `SetRootPageTable`, `GetRootPageTableSize` and `SubmitCommandVirtual`
are then called without a NULL check; `CreateProcess` is checked. The paging
operations `TRANSFER`, `FILL` and `DISCARD_CONTENT` are WDDMv1 only
(`d3dkmddi.h`), and `BuildPagingBuffer` may answer only `STATUS_SUCCESS`,
`STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER` or `STATUS_GRAPHICS_ALLOCATION_BUSY`.
`basicrender.sys` uses the IoMmu model instead and implements none of the root
page table DDIs.

### 2.5 QueryChildStatus / QueryDeviceDescriptor

`DxgkDdiQueryChildStatus` (slot 9) is not called from the FDO start path
directly; it is driven by child (PDO) enumeration:
`DpiPdoIsChildConnected @ 14030d344` -> `DpiDxgkDdiQueryChildStatus @ 1403eb7f4`
(`miniport+0xd0`). `DxgkDdiQueryDeviceDescriptor` (slot 10,
`DpiDxgkDdiQueryDeviceDescriptor @ 1403c4580`, `miniport+0xd8`) is likewise the
child-descriptor path. These run after start, when the PDOs raised by
`QueryChildRelations` are enumerated by the PnP manager (`IRP_MN_QUERY_DEVICE_RELATIONS`).

What a miniport answers, from `basicdisplay.sys` (the same image set): its
`QueryChildRelations` fills `TypeVideoOutput`, `D3DKMDT_VOT_OTHER` (or
`D3DKMDT_VOT_INTERNAL` for an integrated panel), `D3DKMDT_MOA_NONE`, no SDTV,
`HpdAwarenessInterruptible`, `ChildUid` = index, for `ChildRelationsSize / 28
- 1` children; `QueryChildStatus` answers `StatusConnection` with
`HotPlug.Connected`, `StatusRotation` with `STATUS_INVALID_PARAMETER`, anything
else `STATUS_NOT_SUPPORTED`; `QueryDeviceDescriptor` without an EDID answers
`STATUS_GRAPHICS_CHILD_DESCRIPTOR_NOT_SUPPORTED` (0xC01E0401). This driver
answers a synthesized EDID 1.3 base block instead (feature 011, §4c): the window
`DescriptorOffset`/`DescriptorLength` is copied up to byte 128, and an offset at
or past 128 answers `STATUS_MONITOR_NO_MORE_DESCRIPTOR_DATA` (0xC01D0008).

### 2.6 Failure and cleanup within start

Once `DxgkDdiStartDevice` has succeeded, a later failure in `DpiFdoStartAdapter`
unwinds by calling, in order (call sites ~983–1005):

1. `DpiFdoRemoveChildDescriptors`
2. `DpiFdoStopDeviceAndReleasePostDisplayOwnership @ 14025d9d8` (slot 75,
   `DpiDxgkDdiStopDeviceAndReleasePostDisplayOwnership @ 14026a7dc`) for a
   POST/BDD device, otherwise `DpiDxgkDdiStopDevice` (slot 3).
3. `DpiFdoDisconnectInterrupt @ 1402546cc` (only if the interrupt was
   connected).

It may also call `DxgkDdiCollectDiagnosticInfo` (`miniport+0x540`) to capture a
driver blob before creating a live dump. `DxgkDdiRemoveDevice` is **not** called
here — RemoveDevice is a separate later PnP IRP. So a driver that fails
`StartDevice`, or fails a mandatory `QueryAdapterInfo`, must be able to take a
`StopDevice` + interrupt-disconnect immediately afterward.

---

## 3. Teardown

### 3.1 `IRP_MN_STOP_DEVICE`

`DpiFdoDispatchPnp` (`MinorFunction == 4`) -> `DpiFdoHandleStopDevice @ 140257350`:

1. `DpiFdoStopMiracastSession`, then under the miniport-list mutex and the
   adapter resource, `DxgkStartPnPTransition`.
2. `DpiFdoStopAdapter @ 14025d310`, which:
   - optionally waits on adapter events (`KeWaitForSingleObject(adapter+0x10a8)`)
     — i.e. it drains before stopping;
   - `DpiDxgkDdiDisplayDetectControl` (slot 124, conditional);
   - `DpiFdoDestroyRelatedObjects`, `DpiRemoveAdapter`;
   - `DpiDxgkDdiStopDevice @ 14026a718` (slot 3, `miniport+0xa0`) — or, for a
     POST device, `DpiFdoStopDeviceAndReleasePostDisplayOwnership` (slot 75);
   - `DpiFdoDisconnectInterrupt @ 1402546cc` (only if `adapter+0x570 != 0`,
     i.e. an interrupt was connected).
3. `DpiFdoCloseDeviceFileObject`, then forwards the IRP down the stack.

Order to note: **StopDevice precedes interrupt disconnect** (mirror of start,
where connect precedes StartDevice).

### 3.2 `IRP_MN_REMOVE_DEVICE`

`DpiFdoHandleRemoveDevice @ 1402565c0`:

1. If the device is still started (state check), `DpiFdoHandleStopDevice` runs
   first (so a Remove without a preceding Stop still gets a Stop).
2. `IoReleaseRemoveLockAndWaitEx`, `DpiFdoRemoveChildDescriptors`.
3. Forwards the IRP down (`IofCallDriver(lower, Irp)`).
4. `DpiDxgkDdiRemoveDevice @ 140429eb8` (slot 4, `miniport+0xa8`).
5. `SysMmDestroyAdapter`, `IoDetachDevice(lower)`, `IoDeleteDevice(fdo)`.

So `DxgkDdiRemoveDevice` is called **after** the IRP has been passed down and is
the last miniport DDI of the device's life.

### 3.3 `IRP_MN_SURPRISE_REMOVAL`

`DpiFdoHandleSurpriseRemoval @ 140257700`:

1. Marks `adapter+0x48a = 1`, and if the device is no longer physically present
   (`DpiFdoIsDevicePresent`) calls
   `DpiDxgkDdiNotifySurpriseRemoval @ 14026a4f8` (slot 82) with
   `DXGK_SURPRISE_REMOVAL_TYPE = 1`.
2. If the device is still in a started state, `DpiFdoHandleStopDevice`
   (§3.1) is invoked, then the IRP is forwarded down.

`DxgkDdiRemoveDevice` follows later on the separate `IRP_MN_REMOVE_DEVICE`.

### 3.4 Driver unload

`DpiDriverUnload @ 1402526c0` -> `DpiCleanup @ 140252398` (global-state
teardown). Per-adapter DDIs (`StopDevice`, `RemoveDevice`) have already run via
the PnP IRPs above by the time the driver object is unloaded.

---

## 4. Interrupt and DPC

### 4.1 ISR — `DxgkDdiInterruptRoutine` (slot 6)

The interrupt is connected in `DpiFdoConnectInterrupt @ 1404348ac`
(`IoConnectInterruptEx`, fully specified). Two thunks are registered:

- `DpiFdoMessageInterruptRoutine @ 14000bca0` (MSI/MSI-X message routine)
- `DpiFdoLineInterruptRoutine @ 140039ae0` (line-based fallback)

Both do the same thing: indirect-call the miniport's slot 6 via
`*(miniport+0xb8)(miniportContext, MessageNumber)`. They run at the device's
`DIRQL` (whatever `IoConnectInterruptEx` assigned — above `DISPATCH_LEVEL`);
the profiler bookkeeping inside them is guarded by `KeGetCurrentIrql() < 2`
precisely because the ISR is normally above passive/APC. The interrupt IRQL is
computed in `DpiFdoConnectInterrupt` (and `DpiFdoGetInterruptIrql @ 14019f108`
/ `DpiLdaGetInterruptIrqlForChain` for chains).

### 4.2 DPC — `DxgkDdiDpcRoutine` (slot 7)

`DpiFdoDpcForIsr @ 14000d3e0` is the `KDPC` routine registered by
`KeInitializeDpc` in `DpiAddDevice`. It runs at `DISPATCH_LEVEL` (it asserts
`KeGetCurrentIrql() == DISPATCH_LEVEL` and takes an in-stack queued spinlock at
DPC level via `KeAcquireInStackQueuedSpinLockAtDpcLevel`). It reads the
miniport's slot-7 pointer at `*(miniport+0xc0)`:

- If the driver registered dxgkrnl's own `DxgNotifyDpcCB` as its DpcRoutine,
  dxgkrnl runs its internal notify path
  (`VIDSCH_EXPORT::VidSchDdiNotifyDpc` for a render adapter, or
  `ADAPTER_DISPLAY::ProcessDodNotifyDpc` for a display-only adapter), plus
  `DpiProcessMiracastNotifyDpc`.
- Otherwise it calls the miniport's `DxgkDdiDpcRoutine` directly:
  `(*(miniport+0xc0))(miniportContext)`.

So a paravirtual miniport's `DxgkDdiInterruptRoutine` is entered at DIRQL from
`DpiFdoMessageInterruptRoutine`/`DpiFdoLineInterruptRoutine`, and its
`DxgkDdiDpcRoutine` is entered at `DISPATCH_LEVEL` from `DpiFdoDpcForIsr`.

---

## 4a. Submission for a driver that is not MultiEngineAware (feature 009)

VidSch calls `DxgkDdiRender` (slot 53) at PASSIVE in the submitting process,
`DxgkDdiPatch` (slot 25) and `DxgkDdiBuildPagingBuffer` (slot 28) at PASSIVE
(`d3dkmddi.h` annotations), and `DxgkDdiSubmitCommand` (slot 26) and
`DxgkDdiPreemptCommand` (slot 27) at DISPATCH, with the device handle rather
than the context when `SchedulingCaps.MultiEngineAware` is clear.

The driver completes a packet with `DXGK_INTERRUPT_DMA_COMPLETED` carrying its
fence. Outside the ISR that notification must run inside
`DxgkCbSynchronizeExecution`, and the DPC then calls `DxgkCbNotifyDpc`.
`basicrender.sys` does exactly this from its worker thread
(`WARPKMADAPTER::WarpSynchronizeNotifyInterrupt`). This driver completes from
the DPC its SubmitCommand queues, never inside SubmitCommand itself.
`FakeVidSch` drives the sequence and checks every notification.

For paging, VidMm's `TransferToSegment` is the path that issues FILL and
TRANSFER, and that path is for memory segments. An aperture segment's pages come
from `MapVideoApertureSegment` and `UnmapVideoApertureSegmentInternal`, and
discards from `DiscardAllocation` (all dxgmms2). So an aperture-only adapter
answers MAP_APERTURE_SEGMENT, UNMAP_APERTURE_SEGMENT and DISCARD_CONTENT.

## 4b. Timeout recovery (feature 010)

`VidSchiRecoverFromTDR @ 140093834` (dxgmms2) starts `TdrResetFromTimeoutAsync`,
which reaches `ADAPTER_RENDER::DdiResetFromTimeout @ 1401beff8` (slot 32) and
later `DdiRestartFromTimeout @ 140425574` (slot 33), both at PASSIVE under the
adapter's DDI sync lock. Both log any status other than `STATUS_SUCCESS` as an
invalid NTSTATUS. VidSch aborts the packets it had submitted, so the driver
drops its queue without reporting them; `basicrender.sys`'s reset flags its
worker and waits for it to stop. This driver also resets the virtio device on
reset and brings it back on restart. `FakeVidSch::reset_from_timeout` treats
every submitted fence as settled, and the scenario recovers twice.

## 4c. Display modes (feature 011)

All nine DDIs are PASSIVE. The driver reaches each VidPn through
`DxgkCbQueryVidPnInterface` (version 1) and its topology through
`pfnGetTopology`; every path info, mode set and mode info it acquires is
released before the DDI returns. The minimum `basicdisplay.sys` implements,
and this driver follows, for one source and one target:

| DDI | Answer |
|---|---|
| `IsSupportedVidPn` | a null VidPn, or at most one path joining source 0 to target 0, is supported; anything else is not (still `STATUS_SUCCESS`) |
| `EnumVidPnCofuncModality` | per path: a source mode set that is neither pinned nor the pivot is replaced (`pfnCreateNewSourceModeSet`, `pfnAddMode` per mode, `pfnAssignSourceModeSet`) by every mode; a target mode set likewise, narrowed to the pinned source mode's size when one is pinned; unset scaling and rotation (not the pivot) get identity-only support through `pfnUpdatePathSupportInfo` |
| `CommitVidPn` | `PathPoweredOff` changes nothing; no path from the affected source, or no pinned source mode, leaves it without a mode; a pinned mode not in the list or not X8R8G8B8 is `STATUS_GRAPHICS_INVALID_VIDEO_PRESENT_SOURCE_MODE` (0xC01E0310) |
| `UpdateActiveVidPnPresentPath` | identity scaling and rotation only, else `STATUS_GRAPHICS_PATH_CONTENT_GEOMETRY_TRANSFORMATION_NOT_SUPPORTED` (0xC01E0346) |
| `RecommendMonitorModes` | every mode as a driver-origin monitor source mode, 1280x800 preferred; `STATUS_GRAPHICS_MODE_ALREADY_IN_MODESET` (0xC01E0314) from `pfnAddMode` is not a failure |
| `RecommendFunctionalVidPn` | `STATUS_GRAPHICS_NO_RECOMMENDED_FUNCTIONAL_VIDPN` (0xC01E0323) |
| `RecommendVidPnTopology` | `STATUS_GRAPHICS_NO_RECOMMENDED_VIDPN_TOPOLOGY` (0xC01E031A) |
| `ControlInterrupt` | `DXGK_INTERRUPT_CRTC_VSYNC` recorded; any other type `STATUS_NOT_SUPPORTED` |
| `GetScanLine` | from interrupt time at 60 Hz over the committed mode's total lines (one blank line per 24 active, 1080p's ratio); no committed mode is `STATUS_INVALID_DEVICE_STATE` |

Mode enumeration ends on `STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET`
(0x401E034C) and `STATUS_GRAPHICS_DATASET_IS_EMPTY` (0x401E034B), both
successes. A mode info `pfnAddMode` accepts belongs to dxgkrnl; one it refuses
is released by the driver. The modes are 640x480 up to 1920x1080, until the
device reports its displays (feature 027).

## 4d. Present, scanout and vsync (feature 012)

`DxgkDdiPresent` (PASSIVE, on a context) builds a DMA buffer the way
`basicrender.sys`'s `WARPKMCONTEXT::Present` does: one command (docs/protocol.md,
"Present command") with patch locations for allocation-list entries 1 (source)
and 2 (destination), `pDmaBuffer` and `pPatchLocationListOut` advanced past
what was written. Exactly one of `Blt`, `ColorFill`, `Flip` (with or without
`FlipWithNoWait`) is accepted; anything else is `STATUS_INVALID_PARAMETER`. A
buffer too small for the remaining sub-rectangles holds as many as fit and
answers `STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER` (0xC01E0001) with
`MultipassOffset` at the first one left; dxgkrnl patches, submits and calls
again. dxgkrnl then patches and submits it like any render (§4a).

`DxgkDdiSetVidPnSourceAddress` remembers the primary's address for source 0 (a
null allocation clears it; a segment other than 1 is refused); it is callable
up to DISPATCH because the address is read by the vsync tick under a spin
lock. dxgkrnl calls it above DISPATCH only for a driver reporting
`FlipOnVSyncMmIo`, which this one does not. `DxgkDdiSetVidPnSourceVisibility`
records visibility.

Vsync is synthetic. `DxgkDdiControlInterrupt(DXGK_INTERRUPT_CRTC_VSYNC, TRUE)`
starts a high-resolution platform timer (`ExAllocateTimer`, created at
StartDevice, deleted at StopDevice) at 60 Hz; each tick, at DISPATCH, reports
`CRTC_VSYNC` for target 0 with the scanout address and `PhysicalAdapterMask`
1 through `DxgkCbSynchronizeExecution`, then calls `DxgkCbNotifyDpc`.
dxgmms2's `VidSchiProcessIsrVSync` maps the target to its source and completes
flips whose address matches. The tick has its own notification data, so it
never races the DPC's completion reports. `FALSE` stops the timer.

With Present in the table, `DXGADAPTER::CreateRenderCore`'s required DDI set is
complete: the adapter becomes a render adapter.

## 4e. Pointer and the legacy surface (feature 013)

`DXGK_DRIVERCAPS` reports `PointerCaps` Monochrome | Color | MaskedColor with
`MaxPointerWidth` and `MaxPointerHeight` 64. `DxgkDdiSetPointerShape` (PASSIVE)
draws the shape into the cursor resource's 64x64 `B8G8R8A8` backing; the first
shape after a device start sends `RESOURCE_CREATE_2D` and
`RESOURCE_ATTACH_BACKING` for resource 1, every shape `TRANSFER_TO_HOST_2D` on
the control queue, and a visible cursor `UPDATE_CURSOR` on the cursor queue.
`DxgkDdiSetPointerPosition` sends `MOVE_CURSOR` while visible, `UPDATE_CURSOR`
with the resource when it becomes visible, and with resource 0 to hide it.
Nothing waits for the device: the DPC reclaims both queues and logs a response
other than `VIRTIO_GPU_RESP_OK_NODATA`. A power-up forgets the resource and the
shape, so the next shape creates it again.

For a null slot `ADAPTER_RENDER` installs `DefaultDdiAcquireSwizzlingRange`,
`DefaultDdiReleaseSwizzlingRange`, `DefaultDdiStopCapture`,
`DefaultDdiCreateOverlay`, `DefaultDdiUpdateOverlay`, `DefaultDdiFlipOverlay`
and `DefaultDdiDestroyOverlay`, each logging and answering `STATUS_NOT_SUPPORTED`
(0xC00000BB). This driver registers those slots and SetPalette with routines
that answer the same without reading their arguments; the overlay routines
receive a handle the driver never creates and touch nothing. The render core
is still created: with `NumberOfSwizzlingRanges` and `MaxOverlays` zero, nothing
calls them.

## 4f. WDDM 1.1 (feature 014)

`DxgkDdiQueryVidPnHWCapability` (PASSIVE) answers a `D3DKMDT_VIDPN_HW_CAPABILITY`
of 0 for source 0 and target 0 of a non-null functional VidPn: the driver
performs no rotation, scaling, cloning, colour conversion, linked-adapter or
remote output itself. `DescribePageTable`, `UpdatePageTable`,
`UpdatePageDirectory`, `MovePageDirectory`, `SubmitRender` and
`CreateAllocation2` are `PVOID` placeholders in `DRIVER_INITIALIZATION_DATA`
with no contract and stay NULL, as does `RenderKm`, which `DXGCONTEXT::Render`
calls only for a GDI context - created only for a driver reporting
`PresentationCaps.SupportKernelModeCommandBuffer`.

## 4g. WDDM 1.2 (feature 015)

Registered and tested, claimed by no capability until the adapter reports
WDDM 1.2 (with GPU VA, feature 017): a 1.2 claim below `DXGKDDI_WDDMv1_2` fails
`DXGADAPTER::Initialize`, and a full 1.2 adapter must also set
`SupportDirectFlip`.

- Per-engine recovery, as `basicrender.sys`: `QueryDependentEngineGroup` answers
  `1 << NodeOrdinal`; `QueryEngineStatus` sets `Responsive`; `ResetEngine`
  abandons the node's queue without a report, answers the fence that was
  executing (the last completed one when idle) in `LastAbortedFenceId`, and
  leaves the node accepting submissions; `CancelCommand` has nothing to free.
  Only node 0, engine 0.
- `NotifySurpriseRemoval` (called by `DpiFdoHandleSurpriseRemoval` when the
  device is gone, before StopDevice): the ISR claims nothing, the DPC harvests
  nothing, and stopping skips the transport reset a gone device cannot
  acknowledge; the next StartDevice finds the device present again.
- `StopDeviceAndReleasePostDisplayOwnership` (instead of StopDevice for a POST
  display owner) stops the device and answers a zero `DXGK_DISPLAY_INFORMATION`,
  which `DpiFdoStopDeviceAndReleasePostDisplayOwnership` accepts as no frame
  buffer handed to the basic display driver: the scan-out is guest pages behind a
  GPU address.
- `GetChildContainerId` fills the ELD of child 0 from the synthetic EDID.
- `SystemDisplayEnable` answers `STATUS_NOT_SUPPORTED`, so `DpiEnterSystemDisplay`
  falls back to the POST adapter; `SystemDisplayWrite` is never reached.
- `SetPowerComponentFState` refuses every index (no power component is
  reported); `PowerRuntimeControlRequest` and
  `SetVidPnSourceAddressWithMultiPlaneOverlay` answer `STATUS_NOT_SUPPORTED`.

## 4h. WDDM 1.3 (feature 016)

- `GetNodeMetadata`, as `basicrender.sys`: the whole `DXGK_NODEMETADATA`
  (declared under `pack(1)`, 0x4a bytes) zeroed, `EngineType` 3D, name
  `3DNode00`. Node 0 of physical adapter 0 only.
- `CalibrateGpuClock` (up to DISPATCH), as `basicrender.sys`: the GPU clock is
  the CPU performance counter, both counters one `KeQueryPerformanceCounter`
  reading and `GpuFrequency` its frequency.
- `ControlInterrupt2` drives the same synthetic vsync as ControlInterrupt:
  `DXGK_VSYNC_ENABLE` starts it, both disable states stop it.
- `SetPowerPState` refuses every component; `CheckMultiPlaneOverlaySupport` and
  `FormatHistoryBuffer` stay NULL.

## 4i. WDDM 2.0 GPU virtual addressing (feature 017)

- `DRIVERCAPS`: `WDDMVersion` 2.0, `MultiEngineAware`, one asymmetric node,
  `VirtualAddressingSupported` and `GpuMmuSupported`, `FlipOnVSyncMmIo`,
  `SupportPerEngineTDR`. `PHYSICALADAPTERCAPS`: one node, paging node 0,
  `GpuMmuSupported`. `GPUMMUCAPS`: 48 bits, four levels, CPU-virtual updates,
  cache-coherent memory. Each `PAGETABLELEVELDESC`: 9 index bits, segment 0,
  4096 bytes. Node metadata sets `GpuMmuSupported`.
- With `FlipOnVSyncMmIo`, dxgkrnl calls `SetVidPnSourceAddress` synchronized
  with the interrupt, at device IRQL: the scanout address is stored with
  `InterlockedExchange64` and read the same way at vsync.
- `BuildPagingBuffer`: `UPDATE_PAGE_TABLE` writes the device's page table
  entries through VidMm's CPU mapping; `FLUSH_TLB`, `VIRTUAL_FILL`,
  `VIRTUAL_TRANSFER` and `COPY_PAGE_TABLE_ENTRIES` become paging commands
  (docs/protocol.md), a copy running multipass over its ranges;
  `NOTIFY_RESIDENCY` and the aperture operations need nothing.
- `CreateContext` accepts `VirtualAddressing`. `VidSchiEnsureRootPageTableUpdated
  @ 1400fcb40` (dxgmms2) calls `SetRootPageTable` before a submission when the
  process's root changed - except for contexts with flag 0x1000 of its own
  context object, which it skips - so a context without a root admits any DMA
  buffer address. `SubmitCommandVirtual` (PASSIVE) queues the fence and asks for
  the DPC; dxgkrnl accepts only `STATUS_SUCCESS` and `STATUS_INVALID_PARAMETER`
  from it, and only `STATUS_SUCCESS` from `CreateProcess` and `DestroyProcess`.
- `GetRootPageTableSize` answers 8 bytes per entry; `CreateProcess` keeps no
  driver process (as `basicrender.sys`); `SetStablePowerState` does nothing.
  `RenderGdi`, `MapCpuHostAperture`, `UnmapCpuHostAperture`,
  `CheckMultiPlaneOverlaySupport2`, `SetVidPnSourceAddressWithMultiPlaneOverlay2`,
  `PowerRuntimeSetDeviceHandle` and `SetVideoProtectedRegion` stay NULL.

## 4j. WDDM 2.1 (feature 018)

- `DXGADAPTER::Initialize @ 1401a2e21` calls `SetModeBehavior @ 1401a8d0c`
  before `DRIVERCAPS` for an adapter reporting a source and a child: Request
  `PrioritizeHDR` at DDI interface >= 0x7000 (from the DWM `HighColor` registry
  value between 0x6002 and 0x6FFF). The answer is only traced; a status other
  than `STATUS_NOT_SUPPORTED` is logged, one outside SUCCESS, NO_MEMORY and
  NOT_SUPPORTED logged invalid, and the wrapper asserts Satisfied and
  NotSatisfied share no bit. A NULL slot gets `DefaultDdiControlModeBehavior`
  (NOT_SUPPORTED). `ColorimetricControl` is requested at runtime only from driver
  version 2400. The driver satisfies nothing.
- `DXGMONITOR::_UpdateLinkInfo @ 14028e4a0` calls `UpdateMonitorLinkInfo` on
  monitor arrival and usage change with `Hidden` for a non-desktop usage and
  `HeadMounted` for usage 1 (NULL: a default answering NOT_SUPPORTED). It keeps
  capabilities 0x7F (plus `VariableRefresh` from driver version 2600; HDR10+ and
  Dolby Vision only for Xbox or software adapters), always clears
  `TargetIndependentPrimary`, masks other bits below driver version 2400 and
  fails with `STATUS_UNSUCCESSFUL` from it, and zeroes a `DitheringSupport` with
  a preference. A failure zeroes the link info and is swallowed below 2400. The
  driver answers the EDID's view: no stereo, no wide or high colour, no
  dithering.
- `WDDMVersion` 0x2100 is accepted and makes nothing new mandatory; user mode
  sees driver version 2100. `ReadConfig @ 1401a84bd` then enables the secondary
  independent flip when `FlipIndependent` is reported (it is not), and the MPO3
  flip path only with `SetVidPnSourceAddressWithMultiPlaneOverlay3` registered.
- `CheckMultiPlaneOverlaySupport3` is gated on `SupportMultiPlaneOverlay`,
  `PostMultiPlaneOverlayPresent` has no caller in this build, and
  `NtGdiDdDDIUpdateAllocationProperty` answers `STATUS_NOT_IMPLEMENTED` for a NULL
  `ValidateUpdateAllocationProperty`; all four stay NULL.

## 4k. WDDM 2.2 and hardware queues (feature 019)

- `WDDMVersion` 0x2200 makes nothing mandatory. `ADAPTER_DISPLAY::Initialize
  @ 1403abf44` asks `DXGKQAITYPE_DISPLAY_DRIVERCAPS_EXTENSION` (16) and, from
  2.2, assumes `VirtualModeSupport` when it fails; the driver answers 0.
- `DpiGetAdapterInfo @ 14024eb50` never copies `CreateHwContext`,
  `DestroyHwContext` or `SwitchToHwContextList`. The display slots of 2.2 are
  opt-in or NULL-checked: `CreatePeriodicFrameNotification` (VidSch falls back to
  a timer), `SetTimingsFromVidPn` with `DisplayDetectControl` and
  `QueryConnectionChange` (needs slot 105), `SetTargetGamma` (needs
  `GammaRampCaps`), `SetTargetContentType` and `SetTargetAnalogCopyProtection`
  (a NOT_SUPPORTED default), `SetTargetAdjustedColorimetry`,
  `ExchangePreStartInfo` (NULL is success), and the overlay caps queries.
- Hardware scheduling: the driver calls `DxgkCbQueryFeatureSupport` for
  `DXGK_FEATURE_HWSCH` during StartDevice (`DpQueryFeatureSupport @ 14024def0`,
  honouring `HwSchMode`), and a node reports
  `DXGK_NODEMETADATA.Flags.ContextSchedulingSupported` only when enabled (read
  from DDI interface 0x9000 by `ADAPTER_RENDER::Initialize @ 1401b3d4c`; without
  GPU VA or the OS's permission start fails). Every context on such a node has
  `HwQueueSupported`, which `DXGCONTEXT::Initialize @ 1402dc810` refuses below
  driver version 2500, and never sees `SubmitCommandVirtual`.
- `DXGHWQUEUE::Initialize @ 14035be70` creates the progress fence, then calls
  `CreateHwQueue` on the context with the fence's handle and addresses; the
  driver answers `hHwQueue`. `DestroyCoreState @ 1403954f8` terminates the VidSch
  queue, calls `DestroyHwQueue`, then frees the fence; `DXGCONTEXT::DestroyContext`
  destroys queues before the context.
- `SubmitRenderToHwQueue @ 14001ae10` (DISPATCH) and `SubmitPagingToHwQueue
  @ 1400494c0` pass the fence id and CPU address; a failure other than
  NOT_IMPLEMENTED puts the device in error. Completion: write the id into the
  fence, report `MONITORED_FENCE_SIGNALED` (11) with the node and engine, notify
  the DPC; `VidSchiUnwaitNodeHwQueueProgress @ 140008b00` completes each packet at
  or below the fence.
- `ResetHwEngine` (no NULL check, `VidSchiResetHwEngine @ 140053e38`) carries
  the node and engine; the driver invalidates lost contexts with
  `DxgkCbInvalidateHwContext` inside the call, and `ResumeHwEngine` follows.
  `SetupPriorityBands` and `SetSchedulingLogBuffer` may answer NOT_IMPLEMENTED.
- The driver implements `CreateHwQueue`, `DestroyHwQueue`,
  `SubmitCommandToHwQueue` and `ResetHwEngine` (dropping in-flight work) without
  enabling hardware scheduling; enablement, invalidation and `ResumeHwEngine`
  arrive with driver version 2.5.

## 4l. WDDM 2.3 - 2.9 (feature 020)

- Swept every compare against WDDMVersion 0x22ff-0x29ff and driver versions
  2300-2900 in dxgkrnl and dxgmms2: for a driver with no colour transform, HDR,
  `MiscCaps` or `IndependentVidPnVSyncControl`, no step makes a DDI, capability
  or start query mandatory.
- 2.3: `HwQueuePacketCap` survives; disabling a primary, removing ownership and
  hiding a source go through `FlushPresentReferencesAndDisableOverlays` instead
  of the legacy synchronization (`IsLegacyDisplayStateSynchronization
  @ 14005829c`).
- 2.4: `MiscCaps` bits 9 and up must be 0; the diagnostic DDIs are a pair;
  `InitializeDriverWorkarounds @ 1401a4fc8` forces `FlipCaps.FlipImmediateMmIo`;
  per-adapter perf data and GPU version queries (26, 27) are asked, failures
  logged; the broadcast present sync object becomes a monitored fence.
- 2.5: HdrFP16 and HdrARGB10 scanout together fail start; HwQueueSupported
  contexts and more than 64 nodes are allowed; `SetTargetAdjustedColorimetry2`
  is tried first. 2.6: `ComputeOnly` honoured. 2.7: a registered
  `ControlInterrupt3` replaces `ControlInterrupt2` (`DdiControlInterrupt2
  @ 140025f44`); black-screen `CollectDiagnosticInfo`.
- Slots: `UpdateHwContextState` is never copied; protected sessions answer
  NOT_SUPPORTED when NULL (`DXGPROTECTEDSESSION::Initialize @ 14020b73c`);
  `SignalMonitoredFence` (no NULL check) and `PresentToHwQueue` serve only
  hardware-scheduled nodes; `ValidateSubmitCommand` only contexts and queues
  with private data (`DXGCONTEXT::SubmitCommand @ 1402bd5f0`);
  `SetTrackedWorkloadPowerLevel` needs `SupportTrackedWorkload`; the hot update
  pair needs the GPU partition interface; `CollectDiagnosticInfo` is NULL-checked
  and called after a failed AddDevice or StartDevice. `basicrender.sys` fills
  only 130, 131 and 146 of them. The driver reports 2.3 and leaves all twelve
  NULL.

## 4m. WDDM 2.4 and flips on a GPU MMU adapter (feature 021)

- `InitializeDriverWorkarounds @ 1401a4fc8` sets `FlipImmediateMmIo` for every
  2.4 adapter that is not IndirectKmd (`DXGADAPTER+0x1bc` bit 8, from the
  `\Driver\IndirectKmd` object name); the default flip mode becomes 3 and
  `VidSchiExecuteMmIoFlip @ 14000f440` adds `FlipImmediate` for an immediate
  present. At device IRQL like a vsync flip; VidSch completes it on return
  (`VidSchiCompleteFlipEntry`) and expects no report. STATUS_DEVICE_BUSY
  requeues it; INVALID_PARAMETER fails it.
- A primary's segment and address come from VidMm's allocation record
  (`VidSchiBindFlipPhysicalAddress @ 14003ba00`), written only by aperture and
  memory segments; for the implicit system memory segment both are 0
  (`VIDMM_SEGMENT::DriverId @ 1400397a8`). `VidSchiIsExpectedVSyncCookie
  @ 14003a0d4` completes a flip whose PrimaryAddress equals the vsync's exactly.
  The driver accepts segment 0 and reports the stored address, 0.
- For a context with GPU virtual addressing (every context here, CDD's
  included: `DXGDEVICE::CreateContext @ 1402dc274`),
  `DXGCONTEXT::SubmitPresentWithDmaBuffer @ 14031b804` calls `DxgkDdiPresent`
  with a `DXGK_PRESENTALLOCATIONINFO` array (source 1, destination 2, each with
  `AllocationVirtualAddress`) and no patch list, then submits virtually;
  `DxgkDdiPatch` is called only for other contexts. The driver writes the GPU
  virtual addresses into the command (address space 1).
- `ResumeHwEngine` (no NULL check) follows a successful `ResetHwEngine`. Slots
  132-142 stay NULL: NOT_IMPLEMENTED is accepted from the scheduling ones,
  `SuspendContext` NULL is synchronous, exclusive access is IoMmuSecureMode only,
  virtual machine data is GPU-PV host only, and the diagnostic pair left NULL
  gets dxgkrnl's defaults. Hardware scheduling additionally needs WDDM 2.5,
  `DxgkCbQueryFeatureSupport` (`DXGKRNL_INTERFACE+0x1e8`) at StartDevice,
  `SignalMonitoredFence`, `DxgkCbInvalidateHwContext` (`+0x148`) inside
  ResetHwEngine, and a user-mode driver submitting to hardware queues.

## 4n. WDDM 2.9 flip queue logs (feature 022)

- Slots 154-157 are copied from DDI interface 0xE002 (`DpiGetAdapterInfo
  @ 14024eb50`). Every flip queue path checks `VIDSCH_GLOBAL+0x3b`, set by
  `VidSchInitializeAdapter @ 1400a8a30` only when the driver requested
  `DXGK_FEATURE_HWFLIPQUEUE` and reports `MaxHwQueuedFlips` above 1.
  `SetFlipQueueLogBuffer` (NULL-checked, PASSIVE) gets a 0x400-byte log of 0x40
  entries; `UpdateFlipQueueLog`, `CancelQueuedFlips` (the fallback when
  `CancelFlips` answers NOT_IMPLEMENTED) and `SetInterruptTargetPresentId` run at
  device IRQL, are not NULL-checked and must answer SUCCESS;
  `VidSchiCommitFlipQueueLogUpdate @ 14003d168` bugchecks 0x119 on a bad index or
  a present id going backwards.
- From 2.6 the version needs DDI interface 0xA00C or later. 2.9 accepts
  `MapApertureCpuVisible` and honours `VirtualRefreshRateSupport`; nothing is
  mandatory. The driver reports 2.9 and leaves the four slots NULL.

## 4o. WDDM 3.0 (feature 023)

- Slots 158-160 are copied from DDI interface 0xF000, 161 from 0xF002.
  `SetAllocationBackingStore` (NULL-checked) serves only allocations with
  `Flags2.ShareBackingStoreWithKmd` (`VIDMM_GLOBAL::CommitGlobalBackingStore
  @ 1400fdf14`); `CreateCpuEvent` (NULL-checked; NOT_SUPPORTED fails a SignalByKmd
  CPU notification sync object) and `DestroyCpuEvent` (only for an event 159
  created) serve `DXGADAPTERSYNCOBJECT @ 14035e218`; `CancelFlips` (NULL-checked,
  device IRQL) is reached only with the hardware flip queue.
- At 3.0 `DXGDEVICE::CreateDriverAllocations @ 1402f2220` refuses `Flags2` above
  0x3f, and VidMm requires a CPU-visible allocation to include a segment with
  flags 0x1001 - which the implicit system memory segment (0x1190) has, inferred
  and to be confirmed in a VM. The driver reports 3.0, answers `Flags2` 0 and
  leaves the four slots NULL.

## 4p. WDDM 3.1: native fences and doorbells (feature 024)

- Slots 162-165 are copied from DDI interface 0x11000, 166-170 from 0x10002.
- Native GPU fences: `SchedulingCaps.NativeGpuFence` (0x800) requires
  `DXGK_FEATURE_NATIVE_FENCE` (37) granted by `DxgkCbQueryFeatureSupport` and
  `NATIVE_FENCE_CAPS` (37; 0x38 bytes: stride, MapToGpuSystemProcess, minimum and
  maximum address) answered, or start fails. `CreateNativeFence` (0x88 bytes,
  `DXGADAPTERSYNCOBJECT::InitializeAdapterObject @ 14035e218`) gets the VidSch
  object and 64 bytes of private data and answers the driver's handle; VidMm
  owns the value pages. `UpdateMonitoredValues` and `UpdateCurrentValuesFromCpu`
  (`@ 14004976c`, `@ 1400493b4`) run inside `DpSynchronizeExecution`, take arrays
  of handles, values and kernel CPU addresses and no adapter, and bugcheck 0x119
  on failure. `DXGK_INTERRUPT_NATIVE_FENCE_SIGNALED` (19) names up to 8 signalled
  fences or a hardware queue.
- Doorbells: a node with the user-mode submission flag needs driver version
  3100, `ContextSchedulingSupported`, `NativeGpuFence`,
  `DXGK_FEATURE_USER_MODE_SUBMISSION` (4), slots 166-170 and
  `USERMODESUBMISSION_CAPS` (38; doorbell size 1..0x4000). `ConnectDoorbell`
  answers a kernel address whose physical pages are contiguous
  (`DXGDOORBELL::Connect @ 1401f3180` builds the MDL from `MmGetPhysicalAddress`
  and rotates it into the process); `NotifyWorkSubmission` comes from user mode.
- 3.1 makes nothing mandatory for this driver. It reports 3.1 and leaves the nine
  slots NULL until hardware scheduling, which needs a user-mode driver, and the
  device's doorbell BAR (030) exist.

## 4q. WDDM 3.2 (feature 025)

- Slots 172-189 are copied from DDI interface 0x11000, 190 from 0x11002, 191 from
  0x11005 and 192 from 0x11006. At 3.2 `DpiFdoStartAdapter @ 14025b7b4` fails a
  display adapter without `UpdateMonitorLinkInfo` with STATUS_REVISION_MISMATCH;
  query 39 (dirty bit tracking caps) is asked, a failure logged, and a
  DirtyBitTracking answer makes 172-176 mandatory (`CreateRenderCore
  @ 1401b1a74`). The render core needs CollectDbgInfo or CollectDbgInfo2.
- 172-176 are reached only from GPU partitioning (`DXGK_VIRTUAL_GPU_GPUP`),
  177-185 only through the live migration interface, which requires all of them
  (`DxgkIsLiveMigrationInterfaceSupported @ 14006ced4`); 186-189 serve native
  fences; `NotifyContextPriorityChange` is NULL-checked and answers success;
  `ResetDisplayEngine` has no call site. The driver reports 3.2, declines query
  39 and leaves all twenty-one NULL.

## 5. Abusive-but-legal sequences (from `docs/testing.md` §L2)

Brief confirmation that each sequence the harness injects is reachable in the
shipping stack. Scheduler/paging DDIs live in `dxgmms2.sys` (same project).

- **QueryAdapterInfo for an unsupported type — confirmed possible.**
  `DdiQueryAdapterInfo @ 1402f4e90` forwards `Type` unchanged to slot 17 and
  only validates the *returned* NTSTATUS: it accepts success,
  `STATUS_NOT_IMPLEMENTED`, `STATUS_INVALID_PARAMETER` and `STATUS_NO_MEMORY`,
  and logs anything else - including `STATUS_NOT_SUPPORTED` - as invalid.
  Nothing filters the type, so the driver answers a type it does not implement
  with `STATUS_NOT_IMPLEMENTED` rather than touching either buffer.

- **StopDevice while packets are in flight — confirmed possible.**
  `DpiFdoStopAdapter @ 14025d310` performs a bounded wait
  (`KeWaitForSingleObject(adapter+0x10a8)`) but then calls
  `DpiDxgkDdiStopDevice` (slot 3) regardless of scheduler state; the wait is
  not a guarantee the rings are empty.

- **PreemptCommand for an already-completed fence — confirmed possible.**
  `DdiPreemptCommand @ 1400255f8` (dxgmms2) is emitted from the scheduler
  (`VidSchiSubmitPreemptionCommand @ 1400c9e5c`,
  `VidSchiCheckPreemptionPolicy @ 140025520`) without proving the target fence
  is still outstanding; a preempt can race a completion.

- **DestroyAllocation for an allocation still referenced by a submitted packet
  — confirmed possible.** `VidMmDestroyAllocation @ 14003b9d0` /
  `DestroyAllocation @ 1400aec48` (dxgmms2) are reachable from the user-mode
  `NtGdiDdDDIDestroyAllocation @ 1402d4920`; ordering against in-flight
  submissions is the driver's responsibility.

- **ResetFromTimeout then RestartFromTimeout, twice — confirmed possible.**
  TDR recovery runs `VidSchiRecoverFromTDR @ 140093834` ->
  `TdrResetFromTimeout @ 1401ebca0` (via `...WorkItem @ 1401ebdf0`) which
  drives `DdiResetFromTimeout @ 1401beff8` (slot 32) and, through the recovery
  `Reset` step, `DdiRestartFromTimeout @ 140425574` (slot 33). Repeated
  timeouts re-enter the same path, so a reset/restart pair can occur more than
  once.

- **Re-entrant SubmitCommand on two nodes — confirmed possible.**
  `VidSchSubmitCommand @ 1400f14e0` / `DdiSubmitCommand @ 140024de8` (slot 26)
  are per-node; the scheduler submits to different nodes independently, so two
  `DxgkDdiSubmitCommand` calls can be outstanding on distinct nodes.

- **Surprise removal during paging — confirmed possible.**
  `DpiFdoHandleSurpriseRemoval @ 140257700` issues
  `DxgkDdiNotifySurpriseRemoval` (slot 82) asynchronously with respect to VidMm
  paging work (`BuildPagingBuffer`/paging-process operations in dxgmms2); the
  removal handler does not first quiesce the paging engine.

---

## Not determined

- Where dxgkrnl issues `DXGKQAITYPE_WDDMDEVICECAPS`. `d3dkmddi.h` says it is
  "queried by the OS during device initialization after DxgkDdiAddDevice
  returns and before DxgkDdiStartDevice is called"; the call site was not
  traced, so the FDO does not issue it, and the driver answers it at any state.

- The exact set of `DXGKQAITYPE` values queried on the *child/PDO* enumeration
  and power paths (only the `DXGADAPTER::Initialize` set in §2.4 was traced;
  `DdiQueryAdapterInfo` also has callers in `InitializePowerManagement`,
  `ConfigureSysMm`, `DpiFdoInitializeGpuVirtualization` and
  `DpiFdoQueryAdapterInfoIntegratedDisplay*` that were not each decompiled).
- The precise `DdiResetFromTimeout` (slot 32) call site relative to
  `DdiRestartFromTimeout` inside the TDR `Reset` object — the orchestration was
  confirmed via xrefs (`TdrResetFromTimeout` -> `Reset` -> `DdiRestartFromTimeout`)
  but the interleaving of the two DDIs within `Reset` was not decompiled.
- Which paging operations VidMm issues to a GPU MMU adapter whose only
  segments are an aperture and the implicit system memory segment, and in which
  order; what flag 0x1000 of dxgmms2's context object means; and whether
  `DxgkPhysicalAdapterHandle` in `PHYSICALADAPTERCAPS` must be the
  `DeviceHandle` dxgkrnl passed at start (it is answered as that).
- What `DXGADAPTER+0x1bc` bit 9 (which with WDDM 2.1 enables the MPO3 flip
  path) means, and how callers of `DXGMONITOR::_UpdateLinkInfo` handle its
  failure at driver version 2400 and above.
- What `VirtualModeSupport` changes in mode enumeration; how dxgkrnl handles
  `DMA_COMPLETED` on a hardware-scheduled node; what `HwQueuePacketCap` and
  `MaxInFlightHwQueueBuffers` limit; the input `hHwQueue` of the paging queue;
  whether VidSch holds its scheduler lock across `SubmitPagingToHwQueue`.
- Which DDIs `FlushPresentReferencesAndDisableOverlays` issues; the contract of
  `SignalMonitoredFence`'s DMA buffer; what VidSch `+0xaf8`, `HwQueuePacketCap`
  and FDO+0x486 mean.
- Where the flip entry flag 0x100 that also selects `FlipImmediate` comes from;
  the offset part of an implicit-segment address; whether non-virtual contexts
  are allowed on a GPU MMU adapter.
- Whether dxgkrnl enables the hardware flip queue on an adapter without
  hardware scheduling; what flip queue field +0x50 means.
- Whether the driver or the OS writes native fence values through the CPU
  addresses of `UpdateMonitoredValues`; whether the native fence grant requires
  hardware scheduling; what device flag `+0x188 & 0x20` (which skips the doorbell
  DDIs) means.
- How 184 and 185 are dispatched; whether `ResetDisplayEngine` is reached through
  another table.
