# 08 — What WDDM 3.0, 3.1 and 3.2 added

`DXGKDDI_INTERFACE_VERSION` values, from `d3dukmdt.h`:

```
WDDM2_9  0xE003     Windows 11 21H2
WDDM3_0  0xF003     Windows 11 22H2
WDDM3_1  0x10004    Windows 11 23H2
WDDM3_2  0x11007    Windows 11 24H2 (build 26100)   ← our target
```

The `DRIVER_INITIALIZATION_DATA` table is 188 slots at WDDM 3.2. The last 34
slots are everything 2.9 and later added, and the theme is unmistakable:
**user-mode submission, and virtualization.**

## WDDM 2.9 — flip queue logging

`DxgkDdiSetFlipQueueLogBuffer`, `UpdateFlipQueueLog`, `CancelQueuedFlips`,
`SetInterruptTargetPresentId`. Hardware flip queues, with a log buffer so the
OS can see what the hardware queued without an interrupt per flip. Same pattern
that native fence logs later use.

## WDDM 3.0 — backing stores and CPU events

`DxgkDdiSetAllocationBackingStore`, `CreateCpuEvent`, `DestroyCpuEvent`,
`CancelFlips`. `CreateCpuEvent` lets the KMD hand the OS an event object the
*hardware* can signal — the beginning of the "signal the CPU without an
interrupt" direction.

## WDDM 3.1 — doorbells and native fences

This is the big one for us. Nine new slots:

| Slot | Purpose |
|---|---|
| `DxgkDdiCreateNativeFence` | Create a fence the GPU signals directly in memory. |
| `DxgkDdiDestroyNativeFence` | |
| `DxgkDdiUpdateMonitoredValues` | Update the thresholds the hardware watches. |
| `DxgkDdiUpdateCurrentValuesFromCpu` | CPU-side signal into a native fence. |
| `DxgkDdiCreateDoorbell` | Create an MMIO doorbell page. |
| `DxgkDdiConnectDoorbell` | Map it into a user-mode process. |
| `DxgkDdiDisconnectDoorbell` | Unmap it — must be honoured on suspend/reset. |
| `DxgkDdiDestroyDoorbell` | |
| `DxgkDdiNotifyWorkSubmission` | The KMD is told work was submitted from user mode. |

Together these are **user-mode submission**: a process writes commands to a
ring it owns, signals a fence in memory, and stores to a doorbell. The kernel
sees none of it on the fast path.

For a paravirtualized driver this is the mechanism that makes remoting
performant rather than merely correct. A doorbell is one MMIO store; on a
virtio device with ioeventfd, that store is exactly the wake-up the host is already
waiting on.

## WDDM 3.2 — virtualization, live migration, native fence sharing

Twenty-one new slots. Grouped:

### Live migration and dirty tracking

```
DxgkDdiCreateMemoryBasis            DxgkDdiDestroyMemoryBasis
DxgkDdiStartDirtyTracking           DxgkDdiStopDirtyTracking
DxgkDdiQueryDirtyBitData
DxgkDdiPrepareLiveMigration         DxgkDdiEndLiveMigration
DxgkDdiSaveImmutableMigrationData   DxgkDdiRestoreImmutableMigrationData
DxgkDdiSaveMutableMigrationData     DxgkDdiRestoreMutableMigrationData
```

Capability query: `DXGKQAITYPE_DIRTYBITTRACKINGCAPS`,
`DXGKQAITYPE_DIRTYBITTRACKINGSEGMENTCAPS`.

The split between *immutable* and *mutable* migration data is the interesting
part: immutable state is saved once when migration is prepared (device
topology, allocation inventory), mutable state is saved repeatedly as the VM
converges, and dirty-bit tracking tells the hypervisor which video memory pages
changed between rounds. This is a full pre-copy live migration protocol
expressed as DDIs.

### SR-IOV / GPU partitioning

```
DxgkDdiWriteVirtualizedInterrupt      inject an MSI-X into a guest
DxgkDdiSetVirtualGpuResources2        assign resources to a partition
DxgkDdiSetVirtualFunctionPauseState   pause a VF (for migration)
```

Plus the WDDM 3.0-era set already on `ADAPTER_RENDER`:
`DdiCreateVirtualGpu`, `DdiDestroyVirtualGpu`, `DdiGetVirtualGpuInfo`,
`DdiGetVirtualGpuProfile`, `DdiQueryVirtualFunctionLuid`,
`DdiReadVirtualFunctionConfig(Block)`, `DdiWriteVirtualFunctionConfig(Block)`,
`DdiResetVirtualFunction`, `DdiSetVirtualFunctionPowerState`,
`DdiGetGpuPartitionInfo`, `DdiSetGpuPartitionCount`, `DdiSetVirtualGpuVmBus`,
`DdiVirtualGpuDriverEscape`. Capability query: `DXGKQAITYPE_GPUPCAPS`.

### Native fence sharing and logs

```
DxgkDdiOpenNativeFence          DxgkDdiCloseNativeFence
DxgkDdiSetNativeFenceLogBuffer  DxgkDdiUpdateNativeFenceLogs
```

Native fences become shareable across processes, and the OS gets a log buffer
view of signal ordering. Capability query: `DXGKQAITYPE_NATIVE_FENCE_CAPS`.

### Misc

```
DxgkDdiCollectDbgInfo2                richer TDR dumps
DxgkDdiNotifyContextPriorityChange    priority QoS
DxgkDdiResetDisplayEngine             reset display without resetting the GPU
DxgkDdiBuildTestCommandBuffer         OS-generated test workload
DxgkDdiFeatureSampleAddValue/SubtractValue
```

## What this means for the plan

WDDM 3.2 is, more than any prior revision, a **virtualization release**. Two of
its three feature clusters exist so that a GPU can be shared between VMs and
migrated between hosts. That is good news: the model now has first-class
vocabulary for the things a paravirtual driver needs to express.

It is also a trap. `DXGKQAITYPE_GPUPCAPS` and the whole VF/partition family
describe **SR-IOV mediated passthrough** — one physical GPU carved into
hardware functions. That is not what we are building. A paravirtual driver
running on virtio should report *no* GPU-P capability and implement none of
those DDIs. The migration and dirty-tracking family is optional too; leave the
slots NULL until live migration is actually a requirement.

The parts of 3.1/3.2 you should implement:

- **Doorbells** — the performance story.
- **Native fences** — the sync story, and a direct analogue of Vulkan timeline
  semaphores.
- **`ResetDisplayEngine`**, **`CollectDbgInfo2`** — cheap, and they make TDR
  debugging survivable.
