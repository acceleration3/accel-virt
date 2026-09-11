# DDI call sequences in dxgkrnl.sys

Evidence for the L2 DDI conformance harness (`tests/harness/fake_dxgkrnl.cpp`,
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

`FakeDxgadapter` (`tests/harness/fake_dxgadapter.cpp`) reproduces this table
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
`STATUS_GRAPHICS_CHILD_DESCRIPTOR_NOT_SUPPORTED` (0xC01E0401).

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
