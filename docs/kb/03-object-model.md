# 03 — Object model

Everything in WDDM hangs off a handle table owned by `dxgkrnl`. The KMD gets an
opaque `HANDLE` per object plus a chance to attach its own context pointer.
Names below in `CAPS` are `dxgkrnl`'s internal C++ classes, recovered from
`dxgkrnl.pdb`'s public symbols.

```
DXGPROCESS                     one per process that touched D3D
 └─ DXGADAPTER  (ADAPTER_RENDER / ADAPTER_DISPLAY)   one per GPU
     ├─ DXGDEVICE                                    one per D3D device
     │   ├─ DXGCONTEXT ──── node/engine affinity
     │   │    └─ DXGHWQUEUE          (WDDM 2.2+, hardware scheduling)
     │   ├─ DXGALLOCATION            (may be shared / NT-shared)
     │   └─ DXGSYNCOBJECT            fence, monitored fence, native fence
     ├─ DXGPAGINGQUEUE               (WDDM 2.0+) per-device paging work
     └─ DXGVIRTUALGPU                (PARAV or GPUP) if virtualized
```

## Adapter

Created when PnP starts the device. `DxgkDdiAddDevice` gives you the PDO and
you return a `MiniportDeviceContext`; `DxgkDdiStartDevice` is where you get
`DXGKRNL_INTERFACE`, report your interrupt/DMA resources, and return
`NumberOfVideoPresentSources` and `NumberOfChildren`.

For a paravirtual adapter with no real display connectors, `NumberOfChildren`
is still non-zero: you report synthetic children of type
`TypeVideoOutput` so the VidPn machinery has targets to bind to.

`dxgkrnl` splits the adapter into `ADAPTER_RENDER` and display roles
internally — the virtualization DDIs (`DdiCreateVirtualGpu`,
`DdiSetVirtualGpuResources`, `DdiPrepareLiveMigration`) all hang off
`ADAPTER_RENDER`.

## Device

`DxgkDdiCreateDevice` / `DxgkDdiDestroyDevice`. A device is the scope for
allocations and contexts and the unit of TDR blame — when a device faults,
`dxgkrnl` marks *the device* as in error (`DXGKVMB_COMMAND_MARKDEVICEASERROR`
exists for exactly this in the paravirtual path) and every context under it
dies.

## Context and node/engine

`DxgkDdiCreateContext` takes a `NodeOrdinal` and `EngineAffinity`. The
KMD declares its engine topology in `DXGK_DRIVERCAPS.GpuEngineTopology`:

- **Node** — a scheduling queue. Roughly "an engine class": 3D, compute, copy,
  video decode, video encode.
- **Engine** — a physical instance within a node.

VidSch schedules per node. Contexts on different nodes run concurrently and
have no ordering relationship unless you make one with fences.

`DxgkDdiCreateHwContext` / `DxgkDdiCreateHwQueue` (WDDM 2.2+) are the
hardware-scheduling variants: instead of VidSch picking a packet at a time, the
KMD gets a queue object that the hardware itself drains.

**Design note:** a paravirtual driver should expose the minimum node count that
matches what the host can actually run in parallel. Exposing five nodes when
the host serializes everything onto one Vulkan queue just produces false
parallelism that VidSch will schedule into and then stall on.

## Allocation

`DxgkDdiCreateAllocation` is called with an array of
`DXGK_ALLOCATIONINFO`, one per allocation in a resource. For each you fill in:

- `pPrivateDriverData` — your own per-allocation record. Opaque to Windows.
- `Size`, `Alignment`
- `SupportedReadSegmentSet` / `SupportedWriteSegmentSet` — bitmask of which of
  your segments VidMm may place it in
- `PreferredSegment`
- `Flags` — `CpuVisible`, `PermanentSysMem`, `Cached`, `Protected`, …
- `HintedBank`, `MaximumRenamingListLength`

`DxgkDdiDescribeAllocation` is how VidMm asks back for width/height/format when
it needs to move the bytes itself.

`DxgkDdiGetStandardAllocationDriverData` is how the runtime creates
*standard* allocations — shared primaries, staging surfaces, GDI surfaces —
without the UMD being involved. `basicrender.sys` implements this
(`WarpKMGetStandardAllocationDriverData`) and it is required for DWM to work.

**NT-shared allocations** matter for a remoting design: `DXGKVMB_COMMAND_
CREATENTSHAREDOBJECT` / `OPENRESOURCE` / `SHAREOBJECTWITHHOST` show that
GPU-PV has explicit protocol for cross-process shared surfaces, because DWM
composites the desktop from surfaces other processes rendered. You will need
the equivalent: a host-side object identity that survives being opened by a
second guest process.

## Paging queue

WDDM 2.0 made residency asynchronous. `D3DKMTCreatePagingQueue` gives a
device its own queue; `MakeResident` / `Evict` enqueue work there and complete
via a monitored fence. The KMD sees the result as `DxgkDdiBuildPagingBuffer`
calls with `DXGK_OPERATION_*` operations, then a normal submit on the paging
node.

## Sync objects

`D3DDDI_SYNCHRONIZATIONOBJECT_TYPE` — the complete list from
`d3dukmdt.h`:

| Type | Semantics |
|---|---|
| `D3DDDI_SYNCHRONIZATION_MUTEX` | Legacy keyed mutex. |
| `D3DDDI_SEMAPHORE` | Legacy counting semaphore. |
| `D3DDDI_FENCE` | Legacy fence. |
| `D3DDDI_CPU_NOTIFICATION` | Signals a KEVENT; how the CPU waits. |
| `D3DDDI_MONITORED_FENCE` | The modern primitive: a 64-bit monotonically increasing value in a shared page, with waiters registered against thresholds. |
| `D3DDDI_PERIODIC_MONITORED_FENCE` | Monitored fence driven by vsync. |
| `D3DDDI_NATIVE_FENCE` | **WDDM 3.1+.** Hardware-signalled fence usable directly from user mode without a kernel transition. |

Monitored fences are the ones that map cleanly onto a remoting design: the
value lives in a page the host can write and the guest can read, and "signal"
is a store plus a wake. That is the same shape as a Vulkan timeline semaphore,
which is exactly what you want when the host executes on Vulkan and can back
each fence with a `VK_SEMAPHORE_TYPE_TIMELINE` semaphore.
