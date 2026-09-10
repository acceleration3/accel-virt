# 09 — Case study: Microsoft's own paravirtualized GPU (GPU-PV / VAIL)

Everything here is reverse engineered from `dxgkrnl.sys` 10.0.26100.9444 and
`vkrnlintvsp.sys` 10.0.26100.8972, both with public PDBs. Names are
Microsoft's, recovered from mangled C++ public symbols.

## Two virtualization managers, one adapter

`dxgkrnl` contains two implementations of an abstract virtual GPU manager:

```
DXGVIRTUALGPUMANAGER            (base, vtable at ??_7DXGVIRTUALGPUMANAGER@@6B@)
 ├─ DXGVIRTUALGPUMANAGER_GPUP    SR-IOV / GPU partitioning
 └─ DXGVIRTUALGPUMANAGER_PARAV   paravirtualization  ← the interesting one
```

with matching per-GPU objects `DXGK_VIRTUAL_GPU_GPUP` and
`DXGK_VIRTUAL_GPU_PARAV`, and `DXGVIRTUALMACHINE` owning a list of them
(`AddVirtualGpu`, `AcquireVirtualGpuByLuid`, `ActivateVirtualGpu`).

`DXGVIRTUALGPUMANAGER_PARAV` exposes `InitializeVirtualGpuManager`,
`CreateVirtualGpu`, `DestroyVirtualGpu`, `SaveKsrState`.

## The transport: a symmetric VMBus pair

```
DXG_GUEST_VIRTUALGPU_VMBUS       inside the guest's dxgkrnl
DXG_HOST_VIRTUALGPU_VMBUS        inside the host's  dxgkrnl
```

Both implement the same channel lifecycle:
`CreateVmBusChannel`, `VmBusChannelOpened`, `VmBusChannelStarted`,
`VmBusChannelPostStarted`, `VmBusChannelProcessPacket`,
`VmBusChannelSuspend`, `VmBusChannelClosed`, `DestroyVmBusChannel`.

The host side additionally has `VmBusCommandTableVgpuToHost` — a dispatch
table from message id to handler — and `VmBusChannelProcessingComplete`.

## The wire protocol: 92 messages

`DXGKVMB_COMMAND_*`, extracted in full to
[`docs/ref/vmb_commands.txt`](../ref/vmb_commands.txt). Grouped by what they do:

**Adapter / process / device lifetime**
`OPENADAPTER_19H1`, `CLOSEADAPTER`, `CREATEPROCESS`, `DESTROYPROCESS`,
`CREATEDEVICE`, `DESTROYDEVICE`, `FLUSHADAPTER`, `FLUSHDEVICE`,
`MARKDEVICEASERROR`, `GETINTERNALADAPTERINFO_RETURN[1]`,
`DDIQUERYADAPTERINFO`, `QUERYADAPTERINFO`, `GETREGISTRYKEYS`,
`ISFEATUREENABLED[_GLOBAL]`, `LOGEVENT[_GLOBAL]`, `QUERYSTATISTICS`.

**Allocations and residency**
`CREATEALLOCATION`, `DESTROYALLOCATION`, `OPENRESOURCE`, `LOCK2`, `UNLOCK2`,
`MAKERESIDENT`, `EVICT`, `OFFERALLOCATIONS`, `RECLAIMALLOCATIONS`,
`QUERYALLOCATIONRESIDENCY`, `GETALLOCATIONPRIORITY`, `SETALLOCATIONPRIORITY`,
`UPDATEALLOCATIONPROPERTY`, `SETEXISTINGSYSMEMPAGES`,
`SETEXISTINGSYSMEMSTORE`, `SETIOSPACEREGION`, `INVALIDATECACHE`,
`QUERYVIDEOMEMORYINFO`, `CHANGEVIDEOMEMORYRESERVATION`,
`DDIGETSTANDARDALLOCATIONDRIVERDATA`.

**GPU virtual address space**
`RESERVEGPUVIRTUALADDRESS`, `MAPGPUVIRTUALADDRESS`,
`UPDATEGPUVIRTUALADDRESS`, `FREEGPUVIRTUALADDRESS`,
`CREATEPAGINGQUEUE[_RETURN]`, `DESTROYPAGINGQUEUE`.

**Contexts, queues, submission**
`CREATECONTEXTVIRTUAL`, `DESTROYCONTEXT`, `CREATEHWQUEUE`, `DESTROYHWQUEUE`,
`SUBMITCOMMAND`, `SUBMITCOMMANDTOHWQUEUE`, `SETQUEUEDLIMIT`,
`GETCONTEXTSCHEDULINGPRIORITY`, `SETCONTEXTSCHEDULINGPRIORITY`,
`GETDEVICESTATE_VB`.

**Synchronization**
`CREATESYNCOBJECT[_RETURN]`, `DESTROYSYNCOBJECT`, `OPENSYNCOBJECT`,
`SIGNALSYNCOBJECT`, `ISSYNCOBJECTSIGNALED`,
`WAITFORSYNCOBJECTFROMCPU`, `WAITFORSYNCOBJECTFROMGPU`,
`SIGNALFENCE`, `SIGNALFENCENTSHAREDBYREF`, `SIGNALGUESTEVENT`,
`OPENKEYEDMUTEX`, `DESTROYKEYEDMUTEX`, `ACQUIREKEYEDMUTEXSYNC`,
`RELEASEKEYEDMUTEXSYNC`, `QUERYCLOCKCALIBRATION`.

**Cross-VM object sharing**
`CREATENTSHAREDOBJECT`, `DESTROYNTSHAREDOBJECT`, `SHAREOBJECTWITHHOST`,
`GETNEXTHARDLINK_RETURN`.

**Present and display**
`PRESENTHISTORYTOKEN`, `VAILPRESENTHISTORYTOKEN`, `BLT`,
`SETREDIRECTEDFLIPFENCEVALUE`, `UPDATEMONITORMAPPING`, `VSYNCREMOTINGCTRL`,
`CDDGDICOMMAND`, `CDDSYNCGPUACCESS`, `OPMREQUEST`.

**Driver store**
`GETDRIVERSTOREFILE[_RETURN]`, `READDRIVERSTOREFILE`.

**Plumbing**
`BASE`, `ESCAPE`, `COMPLETETRANSACTION`, `SENDWNFNOTIFICATION`.

## The single most important observation

**Look at what those messages are.** `CREATEDEVICE`, `CREATEALLOCATION`,
`MAPGPUVIRTUALADDRESS`, `MAKERESIDENT`, `SUBMITCOMMAND`,
`CREATESYNCOBJECT` — these are **D3DKMT operations**, not `DxgkDdi*`
callbacks.

GPU-PV does not virtualize the DDI. It virtualizes the *kernel API*. The guest
`dxgkrnl` is not a client of a paravirtual miniport; it is a shim that
forwards its own `D3DKMT*` surface over VMBus to the host's `dxgkrnl`, which
executes it against a real vendor KMD. There is no WDDM miniport in the guest
at all in the PARAV path — that is why GPU-PV requires the *host's* driver
files to be projected into the guest's DriverStore (`GETDRIVERSTOREFILE`,
`READDRIVERSTOREFILE` exist precisely to ship the host UMD into the guest).

Consequences:

- GPU-PV's cut line is **above** `dxgkrnl` and **below** the UMD. It needs the
  same vendor UMD on both sides, so it needs a Windows host with the same GPU
  vendor driver. It is structurally unavailable to a Linux/KVM host.
- Our design's cut line is **at Vulkan**, above everything. Different, and the
  right choice for a Linux host.
- But the *message inventory* is directly reusable as a checklist. Anything on
  that list of 92 is something the Windows graphics stack genuinely needs to be
  able to do remotely. If our design has no answer for `OFFERALLOCATIONS` or
  `ACQUIREKEYEDMUTEXSYNC`, that is a gap worth noticing early rather than at
  integration time.

## VAIL

`DXGVAILOBJECT`, `DXGVAILGUESTOBJECT`, `DXGVAIL`, plus
`CreateVmBusGuestSubscribers` / `CreateVmBusHostSubscribers` and
`DXGKVMB_COMMAND_VAILPRESENTHISTORYTOKEN`. VAIL ("Virtual Adapter Integration
Layer") is the *presentation* half: it is how a window rendered inside the
guest appears in the host's compositor, via present-history tokens forwarded
across the bus. This is the mechanism behind WSLg and Windows Sandbox's
seamless windows.

Relevant to us only as evidence: presentation was significant enough to get its
own object model and its own message. Do not treat present as an afterthought.

## `vkrnlintvsp.sys` — the cross-VM sync primitives

292 public symbols, a KMDF driver. It provides:

- `VkiCrossVmEventObjectType`, `VkiCrossVmMutexObjectType` — genuine NT object
  types for events and mutexes **shared across VM boundaries**.
- `VkiCalloutSetEvent`, `PulseEvent`, `ResetEvent`, `QueryEvent`,
  `AcquireMutex`, `ReleaseMutex` — a callout table (`VkiKernelCalloutTable`,
  registered via `VkiRegisterKernelExtension`) so the kernel can operate on
  them.
- `VkiRootAllocateSharedStateRegister`, `VkiRootFreeSharedStateRegister`,
  `VkiRootMapSharedPage` — shared pages and "state registers" between host and
  guest.
- `VkiCalloutWnfPush` / `WnfPull` / `WnfSetCallback` — WNF notifications across
  the boundary.
- `VkiRootMutexProxyThread`, `VkiRootAdjustSecurityDescriptorForVmwp` — a proxy
  thread so the host's VM worker process can participate.

This is the answer to "how does a host signal a guest fence cheaply". The value
lives in a shared page; the wake-up is a cross-VM event.

**Our analogue is simpler.** In the custom QEMU device, a fence value lives in
a guest page the device maps once and writes from its worker thread, and the
wake-up is an MSI-X interrupt injected through a KVM irqfd. The mechanism
Microsoft needed a whole kernel extension for is, for us, a store into memory
both sides already map plus an interrupt KVM already routes.
