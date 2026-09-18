# Research: user-mode submission in the guest (049)

## 1. What a doorbell is attached to: a hardware queue, not a context

`D3DKMT_CREATE_DOORBELL` (d3dkmthk.h) takes `hHwQueue` and `hRingBuffer`, not a
context: user-mode submission is the hardware-scheduling path, where the process
owns a ring buffer allocation and a queue on it, and the doorbell is how it
tells the device the ring's tail moved. The driver already has the queue half -
`HwQueue`, `HwQueueTable` and `HwQueueSubmissions` (feature 019) - and what 049
adds is the page the process stores to and what happens when it does.

The structure also asks for three CPU addresses back: the doorbell itself, a
**status page**, and the location where user mode writes the fence value it has
queued to. So a doorbell is three mappings, not one, and the status page is how
a process learns it was disconnected without a kernel call (US2).

## 2. What the device already does with a doorbell store

Feature 030 built the device side: a `Doorbell` object per context, whose
connection registers the context's wake event as a KVM ioeventfd on the
doorbell page, so a store completes in the host kernel and wakes the worker
directly. Nothing in the guest reaches it yet - feature 043's transport submits
through `D3DKMTSubmitCommand`, a kernel transition per submission.

So the wire is already there on the far side. 049 is the near side: the DDIs
dxgkrnl calls to hand a process a page of it, and the transport that stores to
it.

## 3. Why DisconnectDoorbell is the hard part

dxgkrnl disconnects doorbells for suspend, reset, migration and TDR, and the
KMD must make later stores harmless *before it returns* (docs/kb/05-scheduling.md).
The mechanism Windows drivers use is remapping the process's doorbell page to a
dummy page: the store then lands somewhere that nothing reads.

That is the whole of US2, and it is what makes a fallback necessary rather than
optional: a process that stored to a disconnected doorbell has submitted
nothing, learns so from the status page, and must resubmit through
`D3DKMTSubmitCommand`. A transport that assumed a store always arrived would
lose work exactly when the guest was suspended or reset.

## 4. What the guest side must not do on the hot path (FR-004)

No lock, no allocation, no system call in the steady state. That rules out
asking the kernel anything per submission - including "am I still connected".
The status page is what makes that possible: the process reads a value the KMD
wrote, in its own address space.

## 5. The benchmark is a ratio, not a number (US3, SC-001)

A per-submit cost in microseconds says nothing across machines. What the
benchmark reports is both paths' cost on the same boot, and the ratchet is
their ratio - the same shape feature 048's conformance ratchet has, and for the
same reason: the absolute number is a property of the host, the ratio is a
property of the driver.

## 6. The status page is a protocol, not a flag

`D3DDDI_DOORBELLSTATUS` (d3dukmdt.h:2063) has four values, and each is an
instruction to the process:

| Value | What user mode does |
|---|---|
| `CONNECTED` | write the ring, store to the doorbell, done - no kernel call |
| `CONNECTED_NOTIFY_KMD` | store, then call `D3DKMTNotifyWorkSubmission` as well |
| `DISCONNECTED_RETRY` | do not store; submit through `D3DKMTSubmitCommand` and keep trying to reconnect |
| `DISCONNECTED_ABORT` | the doorbell is gone; destroy it and make another |

So US2's "detects the disconnect from the doorbell's status and resubmits" is
not a heuristic: it is reading one enum out of a page the KMD writes, which
costs no kernel transition and is what FR-004 needs.

`DXGKARG_CONNECTDOORBELL` (d3dkmddi.h:10308) is where the KMD answers it, along
with the kernel CPU address of the doorbell page; `DXGKARG_DISCONNECTDOORBELL`
names only the doorbell, and the KMD must have made later stores harmless by
the time it returns.

## 7. What `CONNECTED_NOTIFY_KMD` buys a paravirtual driver

Nothing, here. It exists for hardware that needs the KMD to do something after
a store - power state, a hardware register write. This device's doorbell is an
ioeventfd: the store *is* the notification. So this driver answers `CONNECTED`
and implements `DxgkDdiNotifyWorkSubmission` as the fallback path it is,
never as the steady state.

## 8. The doorbell is already rung - by the driver

Feature 043's path does not end at `D3DKMTSubmitCommand`: the miniport's
`GuestContext` owns the context's ring and its doorbell index
(`src/core/rvk/guest_context.hpp:84`), and `RingSubmitter` stores to the
doorbell page when it publishes a tail. So every piece of the mechanism exists
and works today - the ring, the page, the ioeventfd, the worker that wakes -
and what makes a submission cost a kernel transition is only *who* does the
store: the driver, on the process's behalf, inside `SubmitCommandVirtual`.

That is what makes 049 a narrower feature than it looks. It does not build a
submission path; it hands the existing one's last two writes - the ring tail
and the doorbell store - to the process that already wrote the commands, and
makes the driver's copy the fallback.

Two consequences:

- The ring a process writes is the ring the device already reads, and
  `RingConsumer` already validates the tail against the head it owns
  (`src/device/ring/ring_consumer.cpp:83`, faulting with `FaultCode::ring_tail`).
  FR-005's "every value the device reads from a user-written ring is validated"
  is therefore a property to *keep*, and the hostile tests for it belong beside
  the ones 030 already has.
- `LoopbackDoorbells` (`tests/harness/`) already models a doorbell page whose
  store at offset 0 rings the mock host, so the full-stack rig can show a
  user-mode store reaching the worker without a new harness.

## 9. Native fences are not this feature's, and FR-003 has to say so

FR-003 asks completion waits to use native fences. That is thirteen more DDIs -
`CreateNativeFence`, `Open`, `Close`, `Destroy`, `UpdateMonitoredValues`,
`UpdateCurrentValuesFromCpu`, `SetNativeFenceLogBuffer`, `UpdateNativeFenceLogs`
and their caps (`d3dkmddi.h:3147-3409`, slots 162-165 and 186-189) - each with
GPU-visible fence memory, a log buffer the OS reads for scheduling, and a
sharing path across processes. It is a feature's worth of work beside the
doorbell's, and 024's own title carries both for the same reason: they arrived
in WDDM 3.1 together, not because they are one mechanism.

What 049 delivers is the submission half: a store that reaches the device with
no kernel transition. The wait stays what it is today -
`Kmt::wait_for_context`, which the device wakes through the fence queue's irqfd
(feature 044) - and the benchmark measures the submit cost, which is what SC-001
is about. A wait without a kernel transition is what the native fences buy, and
it belongs with them.

**This is a scope finding, recorded rather than quietly dropped**: FR-003 is
not met by this feature, and the spec says so beside the requirement.

## 10. Why this device needs no dummy page

The usual way a KMD makes stores harmless after `DisconnectDoorbell` is to
remap the process's doorbell page to a dummy page (kb 05). That is a mechanism
for hardware where a store *is* the work - a register write that starts a
command processor.

Here it is not. A store to this device's doorbell page signals an eventfd; the
worker wakes and reads the **ring's tail**, which is what says there is work
(`RingConsumer::observe`, `src/device/ring/ring_consumer.cpp:83`). A tail that
did not move consumes nothing.

So a store to a disconnected doorbell costs one spurious wake and does nothing,
by construction: the process that was told `DISCONNECTED_RETRY` submits through
`D3DKMTSubmitCommand`, which publishes the tail through the driver, and the
driver rings the doorbell itself. There is no state a rogue store can reach.

What the KMD does on disconnect is therefore what the DDI actually requires:
mark the doorbell disconnected so the status a process reads says so, and
refuse to answer a kernel address for it again until it reconnects. A dummy
page would be a mechanism with nothing to protect.

This is worth stating rather than assuming, because it is the property that
makes FR-005 hold: **every value the device reads from a user-written ring is
already validated** (feature 030), and the doorbell carries no value at all.

## 11. Whether a guest process may create a hardware queue at all

A doorbell is created on an `hHwQueue` (§1), and a process gets one from
`D3DKMTCreateHwQueue` - which dxgkrnl answers only when hardware-accelerated
GPU scheduling is on for the adapter. Nothing in the WDK's DDI headers reports
that: there is no `DXGKQAITYPE_HARDWARESCHEDULING*` in 10.0.26100's
`d3dkmddi.h`, and `DXGK_VIDSCHCAPS` carries no such bit. It is an opt-in the
driver makes in its INF's software settings and the OS confirms in its own
policy, and `src/km/rvkgpu.inf` makes no such entry today.

**This is not guessed at here, and it does not block the guest half.** The
transport asks for a queue and a doorbell and uses them if it gets them; a
refusal is the same path as a disconnect, which US2 requires anyway. So Phase 2
is written the same way whichever answer the OS gives, and the live guest run
is what answers it - at which point the INF entry, if that is what is missing,
is a one-line change with a measurement behind it.

What must not happen is the driver reporting user-mode submission and a process
finding no way to use it. It cannot: the capability says what a doorbell *is*,
and dxgkrnl only asks for one after it has decided a process may have a
hardware queue.

## 12. What the driver gives up when the process submits for itself

Found while building Phase 3, and it is the part of §8 that was not obvious:
handing the last two writes to the process takes two judgements away from the
driver, and a third one has to be built.

- **What was submitted.** `FenceProgress` refuses a fence page that completes
  more than the driver published (`unsubmitted`, `fence_progress.cpp`). On the
  doorbell path the driver published nothing, so every completion would fail
  the context. Windows has the same shape and answers it the same way: the
  location user mode writes its queued fence into is
  `HwQueueProgressFenceLastQueuedValueCPUVirtualAddress` (`d3dkmthk.h:898`),
  which `D3DKMT_CREATE_DOORBELL` hands the **process**, and no field of
  `DXGKARG_CREATEHWQUEUE` or `DXGKARG_CONNECTDOORBELL` hands it to the KMD. So
  the KMD cannot make that judgement, and this driver stops making it for a
  context whose doorbell is connected - keeping the ones it still can: nothing
  moves backwards, and a fault stays.
- **Where the tail is.** `RingSubmitter` publishes the tail and remembers it. A
  process that publishes its own leaves the driver's copy behind, and the batch
  it falls back to `SubmitCommandVirtual` with begins past it. Once a doorbell
  has been connected on a ring, the header is the tail both sides share: the
  driver takes the submission's own beginning, after checking that the header
  ends exactly where that submission does. That check is what keeps a user
  written tail from carrying anything (FR-005).
- **What a wait waits for.** `Kmt::wait_for_context` was going to stay as it is
  (§9), and it cannot: with no submission through the driver there is no DMA
  packet to complete, so a wait would time out. What the wait is for is already
  in the DDI - `HwQueueProgressFenceCPUVirtualAddress`, the read-only mapping
  dxgkrnl gives the process and the KMD - and this driver writes the device's
  completed fence there for a context that submits for itself. dxgkrnl waits
  until that reaches what user mode queued, which is what the full-stack rig
  now does. Native fences remain the thing that removes the kernel transition
  from the wait; this is what makes the wait correct without them.

  On a machine the process still waits the way it always has -
  `GdiKmt::wait_for_context` signals a CPU-notification object from the GPU,
  broadcast on the context - and whether dxgkrnl orders that signal behind work
  a process queued through its doorbell is a question only the guest run
  answers, exactly like §11's. Both are answered on the same boot, and neither
  changes the shape of what is written here.

## 13. What the first live run answered (feature 049, T019)

The guest ran the benchmark and reported `note submit.doorbell none`: this
machine's Windows handed the adapter no hardware queue, so the doorbell path
never ran and only the kernel one was measured - 9337 ns a submission.

Reverse engineering says why, and it is not the INF entry §11 guessed at:

- `D3DKMTCreateHwQueue` takes an `hHwContext`, and `DxgkCreateHwQueueInternal`
  refuses any context without `D3DDDI_CREATECONTEXTFLAGS.HwQueueSupported`
  (0x10, `d3dukmdt.h:766`). This process never asked for one: `GdiKmt::
  create_context_virtual` passes no flags.
- A context that *does* ask is then a hardware-scheduled context, and
  `DxgkSubmitCommandInternal` refuses it outright - "DXGCONTEXT 0x%p is
  hardware scheduled and should not be passed to SubmitCommandCb". So the
  fallback US2 needs cannot be `D3DKMTSubmitCommand` on such a context: it is
  `D3DKMTSubmitCommandToHwQueue`, which the driver already has a DDI for
  (feature 019) and has never carried a batch through.
- dxgkrnl will not let the OS enable hardware scheduling for a driver that
  reports nothing: "Driver reported ContextScheduling support for a node, but
  the OS didn't allow HwSch to be enabled". This driver's
  `DxgkDdiGetNodeMetadata` answers `Flags = 0`, which reports neither
  `ContextSchedulingSupported` nor `UserModeSubmission`
  (`DXGK_NODEMETADATA_FLAGS`, `d3dkmdt.h:1975`).

So SC-001 and SC-002 - the benchmark's ratio and the suite with doorbells on -
need the hardware-queue half of WDDM that 019 left as DDIs: a context that may
hold a queue, a submission through that queue, and a wait on the queue's
progress fence, which is the monitored fence `D3DKMT_CREATEHWQUEUE` answers and
the location this driver already writes what the device finished into (§12).

That is what T020-T024 build. Nothing here is a new capability the driver does
not implement: it is the same ring, the same doorbell and the same fences,
reached the way a hardware-scheduled adapter is reached.

## 14. What the OS requires before it will let a doorbell exist

The second live run answered the rest of §13, and the answer is in dxgkrnl's
own strings. A node that reports `UserModeSubmission` is checked against three
things, and failing them does not refuse the doorbell - it refuses the
**adapter**, which came back as `problem=43` and
`StartAdapter_AddAdapterFailed` with STATUS_INVALID_PARAMETER:

- "Driver reported UserModeSubmission support for a node which doesn't support
  ContextScheduling."
- "Driver reports UserModeSubmission support, but does not support all relevant
  Doorbell DDIs."
- "Driver reported UserModeSubmission support, but **NativeGpuFence cap is not
  set**."

The first two this driver satisfies. The third it does not: native fences are
`DxgkDdiCreateNativeFence`, `DestroyNativeFence`, `UpdateMonitoredValues` and
`UpdateCurrentValuesFromCpu` (slots 162-165, feature 024's other half) with
`DXGKQAITYPE_NATIVE_FENCE_CAPS` reporting them, and none of it exists here.

So §9's deferral does not hold: **user-mode submission cannot be turned on
without native fences**. They are not the thing that makes the *wait* cheaper -
they are the thing the OS insists on before it will hand a process a doorbell
at all, and the roadmap's own title for 024 carries both for that reason.

What the node reports meanwhile is what it can back: context scheduling and the
in-flight depth, which is what lets the OS hand out a hardware queue. Reporting
user-mode submission before the fences exist is a capability nobody can use,
and constitution VI forbids it - which is exactly what the live run proved by
failing the adapter start.

## 15. Why reporting a scheduling capability is an ask, not a statement

The run after §14 still refused the adapter, with `ContextScheduling` alone,
and `DXGADAPTER::Initialize` (dxgkrnl, the function that logs those messages)
says why: a node's flags are validated against the **OS's own policy**, not
only against the driver.

```
if ((metadata.Flags & ContextSchedulingSupported) != 0) {
    if (!IoMmuSupported && !adapter->gpu_va)     -> "doesn't support GPU VA"
    if ((os_policy & 0x20000) == 0)              -> "the OS didn't allow HwSch to be enabled"
}
if ((metadata.Flags & UserModeSubmission) != 0) {
    if (wddm < 3.1)                              -> refused
    if (!ContextSchedulingSupported)             -> refused
    if ((os_policy & 0x8000000) == 0)            -> "the OS didn't allow UserModeSubmission"
    if ((caps & 0x800) == 0)                     -> "NativeGpuFence cap is not set"
}
```

Each of those refusals fails **StartAdapter**, not the feature: the adapter
ends at `problem=43` and nothing renders at all. So a capability here is not
something a driver states; it is something it may state only once the OS has
agreed, and the agreement is asked for through `DxgkCbQueryFeatureSupport`
(`DXGKARGCB_QUERYFEATURESUPPORT`, WDDM 2.9) with `DXGK_FEATURE_HWSCH` or
`DXGK_FEATURE_USER_MODE_SUBMISSION` and the driver's own support state.

That is what this driver now does at StartDevice, and the node reports context
scheduling only where the answer was yes. It is the same rule constitution VI
already asks for - never report a capability that is not implemented - with the
OS added to what "implemented" has to mean.

## 16. What a single-node GpuMmu adapter cannot report

The OS *does* allow hardware scheduling on this machine - the driver asked and
was told yes (`rvkgpu: hardware scheduling status 0x00000001`) - and the
adapter still failed to start. The remaining check, decompiled from
`DXGADAPTER::Initialize`, is the one before it:

```
if (metadata.Flags & ContextSchedulingSupported) {
    if (metadata.IoMmuSupported == 0 && physical_adapter->any_node_gpu_mmu == 0)
        -> "Driver reported ContextScheduling support for a node, but it doesn't support GPU VA"
}
...
if (metadata.GpuMmuSupported) { ...; physical_adapter->any_node_gpu_mmu = 1; }
```

`any_node_gpu_mmu` is cached **inside the same loop, after** the context
scheduling check, so a node's own GpuMmu support cannot satisfy it: only an
earlier node's can. This adapter has one node, and addresses through a GPU MMU
rather than an IOMMU, so there is no node before it and nothing to satisfy the
check with. The refusal fails StartAdapter, which is `problem=43` and no
display at all.

So hardware scheduling here needs one of two things this driver does not have:
`IoMmuSupported` on the node - an IOMMU path (`DxgkCbMapMdlToIoMmu`) it has no
implementation for - or a second node reporting GpuMmu before it.

Both are features of their own, and neither belongs to a doorbell. What 049
leaves, then, is the whole submission path built, tested at every layer and
driven end to end through the real driver and the real device core on Linux -
with the guest half switched off by two OS rules, each recorded here, each a
one-line change away once the thing it asks for exists:

- the node's flags, `hardware_scheduling_ && io_mmu_supported` (§16), and
- user-mode submission, which needs native fences besides (§14).

## 17-§19. What the OS actually requires, decompiled

Three more runs, three more rules, each read out of `dxgkrnl.sys` rather than
guessed:

**§17 - a capability may not be reported before the OS enables the feature.**
"Driver reports NativeGpuFence cap when NativeFence feature is disabled,
returning failure": reporting `DXGK_VIDSCHCAPS.NativeGpuFence` while
`DXGK_FEATURE_NATIVE_FENCE` is off fails the adapter start, exactly as the node
flags do. So the driver asks for that feature too, and reports the capability -
and answers `DXGKQAITYPE_NATIVE_FENCE_CAPS` at all - only where the answer was
yes.

**§18 - the OS asks the driver, through an interface the driver must answer.**
`DpiIsFeatureEnabled` enables a staged feature only when, among other things,
`QueryDriverFeatureSupport` says the driver supports it *and* the driver's
version range overlaps the OS's. That query arrives through
`DXGKDDI_FEATURE_INTERFACE`, asked for by GUID at `DxgkDdiQueryInterface` - a
DDI this driver used to refuse outright. Two things it taught immediately:

- A narrow version range disables the feature. The OS enables it at the highest
  version in both ranges, so a driver that says "version 1" and an OS staging
  version 2 agree on nothing. This driver implements each mechanism as the WDDM
  3.2 headers define it and has no revisions, so it reports every version.
- The query arrives *before* StartDevice. Answering
  `SupportedOnCurrentConfig` from what the driver had mapped so far answered no
  for everything - and turned off hardware scheduling, which the OS had been
  enabling by default before the driver answered at all.

**§19 - and then the feature has to be turned on.** With all of that right, the
live run still said: hardware scheduling 1, native fences 0, user-mode
submission 0. `IsFeatureEnabledUncached` needs one more thing - the feature's
configured state - and `ConfigureFeature` reads it from the registry:

```
<adapter's key>\Features\<DXGK_FEATURE_ID in decimal>
    Enabled, MinVersion, MaxVersion, AllowExperimental   (REG_DWORD)
```

So the INF turns on exactly the two this driver implements: 4
(`DXGK_FEATURE_USER_MODE_SUBMISSION`) and 37 (`DXGK_FEATURE_NATIVE_FENCE`).
That is the "one-line INF change with a measurement behind it" §11 guessed at,
and it took the whole chain above to find out which line.

## 20. Why the doorbell cannot open on this build of Windows

The run after §19 turned both features on the only way the OS offers - the
INF's `Features\4` and `Features\37` keys, and the same keys under
`HKLM\SYSTEM\CurrentControlSet\Control\GraphicsDrivers` for the global
database - and got:

```
rvkgpu: native fences status 0x00000000
rvkgpu: hardware scheduling status 0x00000001
rvkgpu: user-mode submission status 0x00000001
```

The same key worked for feature 4 and did nothing for feature 37, and
`dxgkrnl.sys` says why. `IsFeatureEnabledUncached` enables a feature only when
three things agree - its dependencies, the driver's answer through the feature
interface, and `CheckFeatureOSSupport`:

```c
local_55 = CheckFeatureOSSupport(descriptor_slot, descriptor, feature_id);
...
if ((dependencies_ok != '\0') && (local_55 != '\0') && (driver_and_version_ok != '\0'))
    flags |= enabled;
```

and `CheckFeatureOSSupport` answers for two feature ids without consulting
anything at all:

```c
if ((param_3 == 0x20) || (param_3 == 0x25)) {
    return false;
}
```

`0x25` is 37, `DXGK_DRIVER_FEATURE_NATIVE_FENCE`; `0x20` is 32,
`DXGK_DRIVER_FEATURE_PAGE_BASED_MEMORY_MANAGER`. Both are staged off in this
build of the OS itself, and no registry value, driver answer or version range
reaches that decision. (The ids around them confirm the mapping: 41
`SYNC_PRESENT_RENDER_HWQ_ONLY` returns true, and 45, 46 and 49 each call the
matching `Feature_*_IsEnabled` helper.) The one path that bypasses it is
`IsFeatureEnabledHost`, taken only for an adapter-specific feature in a VM that
`DxgkIsVmConnectedToHost` recognises - the GPU-PV partition talking to its host,
which this device is not.

That closes the chain the earlier sections opened, in the other direction:

- `DXGADAPTER::Initialize` refuses an adapter whose `DXGK_VIDSCHCAPS` sets
  `NativeGpuFence` while feature 37 is disabled (§17), which it always is here;
- it refuses a node that reports `UserModeSubmission` without that capability
  (§14);
- so on this build the node may not report user-mode submission, and a process
  is never handed a doorbell page.

So the driver asks the OS for both features at StartDevice and reports the node
flag only where both were granted. Everything behind the flag stays built and
tested - the five doorbell DDIs, the four native fence ones, the hardware queue
path, the user-mode ring publish and `GuestDoorbell::recover` - and the guest
suite exercises the fallback it was written to have: `submit.doorbell none`,
kernel submission, 45/45. The doorbell opens on the first Windows build whose
`CheckFeatureOSSupport` does not refuse 37, with no driver change.

## 21. The one door left, and why it is not one

`dxgkrnl` has a global, `g_NativeFenceDebugTest`, that the node validator reads
right before the checks §14 listed:

```c
if ((node_flags & UserModeSubmission) == 0) {
    if ((g_NativeFenceDebugTest != 0) && ((node_flags & ContextScheduling) != 0)) {
        node_flags |= UserModeSubmission;            /* the OS opts the node in */
        adapter->ums_caps_flags |= 1;
        adapter->doorbell_size = 0x200;
        adapter->secondary_doorbell_size = 0x200;
    }
}
```

Set, it turns user-mode submission on for any context-scheduled node and skips
the NativeGpuFence check entirely - which is exactly the wall §20 ends at. It
has no writer anywhere in the image: no registry parameter table names it, and
nothing in `DriverEntry` assigns it. It is a variable for a kernel debugger to
poke, so seeing this driver's doorbells live on 26100 would mean attaching one
(or writing guest memory through the QEMU gdbstub) and patching a byte in
`dxgkrnl`'s data section every boot. That is a demonstration, not a gate, and
nothing in the driver would differ; so it is written down here and not done.
