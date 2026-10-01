# 05 — Scheduling, submission, fences, TDR

## The two submission models

### Packet scheduling (all WDDM versions)

VidSch picks one context per node, hands you one DMA buffer at a time, and
waits for its fence before giving you the next. The KMD sees:

```
DxgkDdiRender(hContext, pCommand)            ← PASSIVE, in submitter's process
    translate UMD buffer → hardware buffer
DxgkDdiPatch(...)                            ← only in physical-addressing mode
DxgkDdiSubmitCommand / SubmitCommandVirtual  ← DISPATCH_LEVEL, must not block
    …engine runs…
DxgkCbNotifyInterrupt(DXGK_INTERRUPT_DMA_COMPLETED)
DxgkCbQueueDpc()
DxgkDdiDpcRoutine()
```

`DXGKARG_SUBMITCOMMANDVIRTUAL` is small and fully public:

```c
typedef struct _DXGKARG_SUBMITCOMMANDVIRTUAL
{
    HANDLE                          hContext;
    D3DGPU_VIRTUAL_ADDRESS          DmaBufferVirtualAddress;
    UINT                            DmaBufferSize;
    VOID*                           pDmaBufferPrivateData;
    UINT                            DmaBufferPrivateDataSize;
    UINT                            DmaBufferUmdPrivateDataSize;
    UINT                            SubmissionFenceId;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID  VidPnSourceId;
    D3DDDI_FLIPINTERVAL_TYPE        FlipInterval;
    DXGK_SUBMITCOMMANDFLAGS         Flags;
    UINT                            EngineOrdinal;
    UINT                            NodeOrdinal;
} DXGKARG_SUBMITCOMMANDVIRTUAL;
```

`SubmissionFenceId` is a **monotonically increasing 32-bit id per node**. The
contract is: when work for fence id *N* has completed, report *N*; VidSch
infers that everything ≤ *N* is done. `DxgkDdiQueryCurrentFence` must return
the last completed id on demand.

### Hardware queues (WDDM 2.2+)

`DxgkDdiCreateHwQueue` / `DxgkDdiSubmitCommandToHwQueue`. The KMD owns a ring
the hardware drains; VidSch stops arbitrating per packet. Requires GPU VA and
`DXGK_DRIVERCAPS.SchedulingCaps.MultiEngineAware` style capabilities.

### User-mode submission (WDDM 3.1+)

`DXGKQAITYPE_USERMODESUBMISSION_CAPS`, plus the doorbell DDIs:
`DxgkDdiCreateDoorbell`, `ConnectDoorbell`, `DisconnectDoorbell`,
`DestroyDoorbell`, `DxgkDdiNotifyWorkSubmission`. A doorbell is a page of
device MMIO mapped into a user-mode process; the UMD writes its ring, stores to
the doorbell, and no kernel transition happens at all.

**This is the single most important WDDM 3.1/3.2 mechanism for a
paravirtualized driver.** A doorbell maps exactly onto a device doorbell
register backed by a KVM ioeventfd: the store completes in the host kernel and
wakes a device worker thread directly. It means the guest UMD can push serialized Vulkan
commands into a shared ring and poke the host directly, with the KMD involved
only in setup and in fault/recovery paths.

The catch: `DisconnectDoorbell` must be honoured. `dxgkrnl` disconnects
doorbells when a device is suspended, reset, or migrated, and the KMD must make
subsequent user-mode stores harmless — usually by remapping the doorbell page
to a dummy page.

#### What this driver answers (feature 049)

`DXGKQAITYPE_USERMODESUBMISSION_CAPS` reports a four-byte doorbell at offset 0
of the page and no secondary one, which is what `av_device.h` says a store is.
A doorbell is `DoorbellTable`'s `QueueDoorbell`, holding the hardware queue it
was made on and the page index that queue's context was bound to —
`AvContextTable` hands out a doorbell page and a host context together, so the
page a process stores to is the one the device's worker for that context waits
on. `ConnectDoorbell` answers the kernel address of that page inside the BAR
the miniport mapped, and `CONNECTED`: this device never asks for
`CONNECTED_NOTIFY_KMD`, because the store *is* the notification, and
`DxgkDdiNotifyWorkSubmission` does nothing but succeed for a process that was
told otherwise by some other driver.

**No dummy page is needed here**, and that is a property of the device rather
than a shortcut. A store to this doorbell signals an eventfd; the worker wakes
and reads the *ring's tail*, which is what says there is work, and a tail that
did not move consumes nothing. Disconnecting therefore means marking the
doorbell disconnected — so the status page a process reads says so — and
refusing to answer a kernel address for it until it reconnects.

What moves with the submission is harder than the doorbell itself. Once a
process publishes its own ring tail and rings its own page:

- The KMD no longer knows what was submitted. `HwQueueProgressFenceLastQueued`
  `ValueCPUVirtualAddress` (`d3dkmthk.h`) is handed to the **process**, not to
  the driver, so a KMD cannot check a completion against what it published —
  and must not fail the context for one it never saw.
- The ring's header is the tail both sides share. A batch the process falls
  back to `SubmitCommandVirtual` with begins past what the driver last
  published, so the driver takes the submission's own beginning, after checking
  that the header ends exactly where that submission does.
- A wait has nothing of the driver's to wait for. What it waits on is
  `HwQueueProgressFenceCPUVirtualAddress` — the read-only mapping dxgkrnl gives
  both the process and the KMD — which this driver writes the device's
  completed fence into for any context that submits for itself.

**Native fences** are `CreateNativeFence`, `DestroyNativeFence`,
`UpdateMonitoredValues` and `UpdateCurrentValuesFromCpu` (slots 162-165), which
this driver implements over `NativeFenceTable`, plus `DXGKQAITYPE_NATIVE_FENCE_CAPS`.
Sharing and the log buffer (`Open`, `Close`, `SetNativeFenceLogBuffer`,
`UpdateNativeFenceLogs`, slots 186-189) are not implemented and nothing reports
them.

#### What the OS demands before any of it runs (feature 049, research.md §14-§20)

Reporting one of these capabilities is an *ask*, not a statement, and
`DXGADAPTER::Initialize` fails the whole adapter start - `problem=43` in the
guest - for an adapter that asks wrongly:

- a node that reports `UserModeSubmission` must also report
  `ContextSchedulingSupported`, implement all five doorbell DDIs, and the
  adapter must set `DXGK_VIDSCHCAPS.NativeGpuFence`;
- a node that reports `ContextSchedulingSupported` needs the OS's own hardware
  scheduling policy enabled, and either IoMmu support or an earlier node with
  GPU VA - hence two nodes here, the hardware-scheduled one second;
- `NativeGpuFence` may be set only where the OS enabled
  `DXGK_FEATURE_NATIVE_FENCE`, which the OS decides from its staged state, the
  driver's answer through `DXGKDDI_FEATURE_INTERFACE` (asked for by GUID at
  `DxgkDdiQueryInterface`, *before* StartDevice), the overlap of their version
  ranges, and `…\Features\<id>\Enabled` in the registry.

And on Windows 11 26100 none of it can be reached: `CheckFeatureOSSupport`
answers feature 37 with a hard `false` before consulting any of that, so the
native fence feature is never enabled, the capability may never be set, and no
node may report user-mode submission. The driver asks for both features at
StartDevice and reports what it was granted, which on this build is hardware
scheduling alone.

## Fences

| Primitive | Where the value lives | Who signals |
|---|---|---|
| Submission fence (`SubmissionFenceId`) | KMD-internal | KMD, via `DxgkCbNotifyInterrupt` |
| Monitored fence | A page VidMm allocates, mapped to CPU and GPU | GPU, or KMD via `DXGK_OPERATION_SIGNAL_MONITORED_FENCE` |
| Native fence (WDDM 3.1+) | GPU-writable memory + a log buffer | GPU directly, no kernel |

**Monitored fences** are the workhorse. Semantics: a 64-bit value that only
increases; waiters register a threshold; when the value reaches it they wake.
This is a Vulkan timeline semaphore with the names changed.

**Native fences** (WDDM 3.1, extended in 3.2) go further: user mode both signals
and waits without a kernel transition, and the KMD gets
`DxgkDdiSetNativeFenceLogBuffer` / `DxgkDdiUpdateNativeFenceLogs` so the OS can
still observe the ordering for debugging and for scheduling decisions.
WDDM 3.2 adds `DxgkDdiOpenNativeFence` / `CloseNativeFence` for sharing them
across processes. Report support via `DXGKQAITYPE_NATIVE_FENCE_CAPS`.

Interrupt reporting uses `DXGK_INTERRUPT_TYPE`: `DMA_COMPLETED`,
`DMA_PREEMPTED`, `DMA_FAULTED`, `CRTC_VSYNC`, `CRTC_VSYNC_WITH_MULTIPLANE_
OVERLAY*`, `GPU_ENGINE_MONITORED_FENCE_SIGNALED`, `GPU_ENGINE_TIMEOUT`, plus
the display flip ones. Fill a `DXGKARGCB_NOTIFY_INTERRUPT_DATA` and call
`DxgkCbNotifyInterrupt`, then `DxgkCbQueueDpc`.

Both calls must happen with the interrupt spinlock held. If you are not in a
real ISR — which a paravirtual driver is not — use
`DxgkCbSynchronizeExecution` to run a small routine at the adapter's IRQL that
does the notify + queue DPC pair. `basicrender.sys` does exactly this in
`WARPKMADAPTER::WarpSynchronizeNotifyInterrupt`, which is two indirect calls:
`DxgkCbNotifyInterrupt` then `DxgkCbQueueDpc`.

### `DMA_FAULTED` bugchecks a GPU MMU adapter (read from dxgmms2, feature 044)

A live Windows 11 guest bugchecked **0x119 VIDEO_SCHEDULER_INTERNAL_ERROR
(0xD, node 0, engine 0, fence 0x2E)** the moment the driver reported
`DXGK_INTERRUPT_DMA_FAULTED`. In the 26100 dxgmms2.sys,
`VidSchDdiNotifyInterruptWorker` @140031b80 reaches
`WdLogSingleEntry5(0, 0x119, 0xD, NodeOrdinal, EngineOrdinal, FaultedFenceId)`
for every interrupt of type 4 on an adapter whose contexts use GPU virtual
addressing; it is never processed. So this driver never reports it:

- a context the host faulted has everything it queued **completed** by the
  DPC, so the process waiting on it wakes and finds no reply for its fence,
  and every later `SubmitCommandVirtual` on it answers
  `STATUS_INVALID_PARAMETER`, which D3DKMTSubmitCommand returns to the process;
- `ResetEngine` answers `LastAbortedFenceId` and reports nothing more - VidSch
  already knows what it reset;
- work queued for a device that changed across D3 stays queued, so the engine
  stops moving and timeout detection resets it; a submission on that device is
  refused.

## Preemption and TDR

The KMD must implement, and mean, all of:

| DDI | Contract |
|---|---|
| `DxgkDdiPreemptCommand` | Stop the currently running packet as soon as you can. Report `DMA_PREEMPTED` with the fence id you actually got to. |
| `DxgkDdiCancelCommand` | Remove a not-yet-started packet from the queue. |
| `DxgkDdiQueryEngineStatus` | Is the engine progressing? Used by the TDR watchdog - and read what that answer *does*, below. |
| `DxgkDdiResetEngine` / `ResetHwEngine` | Reset one engine, keep the rest of the GPU alive. |
| `DxgkDdiResetFromTimeout` | Full adapter reset. Called at PASSIVE. |
| `DxgkDdiRestartFromTimeout` | Resume after a reset. |
| `DxgkDdiQueryDependentEngineGroup` | Which engines must be reset together. |

### What "responsive" costs (read from dxgmms2, feature 040)

`VidSchiCheckNodeTimeout` (dxgmms2.sys @1400c9b50) is the periodic check:

1. Elapsed time on the node is a difference of checkpoints times
   `KeQueryTimeIncrement()`, compared against `g_TdrConfig + 4` (`TdrDelay`)
   times 10,000,000 - seconds in 100 ns units.
2. Past that, **and only for a device that allows long-running packets**
   (`VidSchiRunningDeviceAllowsLongRunningPackets` @14004505c: every queued
   packet accounted for, none in flight), it calls `DdiQueryEngineStatus`
   (dxgkrnl.sys @140027dc0).
3. **A successful call whose answer has bit 0 set makes the check return zero -
   no timeout this round.** Otherwise it captures the running process, marks
   the node timed out and goes on to `VidSchiReportHwHang`.

So "responsive" is a deferral, granted once per check. A driver that answers it
unconditionally - as this one did until feature 040 - defers a hung host
forever: no TDR, no engine reset, no event 4101, a guest that simply stops.
Answer it from movement: idle, or progress that has changed since the last time
the question was asked.

**Both DDIs are called from that DPC**, not at PASSIVE: each wrapper reaches for
the current thread only when IRQL is below 2, and `DdiResetEngine`
(dxgkrnl.sys @14006e0a4) checks that the driver returns at the IRQL it entered -
a mismatch logs the display watchdog's own 0x113 class entry.

### What a power transition must not do (feature 040)

Windows suspends a GPU by calling `DxgkDdiSetPowerState` toward D3 and back to
D0, and keeps everything else: every allocation, the VidPn, every context. Two
things bite a remoting driver here, both found on a live guest:

- **Anything that interrupts on a timer must stop.** dxgkrnl keeps asking for
  vertical blanks across a suspend, and a blank reported to an adapter it has
  not powered up - which is where the timer fires on the way out of S3 - is a
  report from a device that is not there. Windows bugchecks for it.
- **The interrupt stays connected across the transition.** An ISR that only
  checks "does my device object exist" will run while the device is being built
  again, on queues that are not activated yet.

And one that is not the driver's: OVMF on q35 advertises S3 through ACPI and
cannot resume from it. The guest wakes and its own HAL bugchecks **0x5C
HAL_INITIALIZATION_FAILED (0x201, ..., STATUS_UNSUCCESSFUL)**, after the driver
has completed D0 cleanly. The suite disables S3 at the machine
(`-global ICH9-LPC.disable_s3=1`) rather than testing a resume the firmware
cannot do.
| `DxgkDdiCollectDbgInfo` / `CollectDbgInfo2` (3.2) | Dump state for the TDR report. |

Default TDR timeout is 2 seconds of no progress. **A remoting driver will trip
this the first time the host stalls**, so:

- `QueryEngineStatus` must reflect *host* progress, not just "the ring is not
  empty". Track a host-side heartbeat or last-completed timestamp.
- Preempt must be real. If the host cannot preempt, the KMD must at minimum
  stop feeding it and report preempted at the last completed fence.
- Reset must resynchronize the whole guest-side object graph with the host,
  because the host may have dropped state.

`basicrender.sys` has `WARPKMCONTEXT::CheckDmaBufTimeout` and
`NotifyDmaBufFault` for precisely this, and it is a software GPU with no
hardware to hang — the machinery is mandatory regardless.

## Priority and quality of service

`DxgkDdiSetContextSchedulingPriority`, `GetContextSchedulingPriority`,
and WDDM 3.2's `DxgkDdiNotifyContextPriorityChange`. Also
`DxgkDdiSetQueuedLimit`. In a remoting design these map onto host queue
priorities (`VK_EXT_global_priority`) or simply onto which host thread services
which guest ring.

## What ten minutes of fences on the real guest showed (feature 039, US3)

`AV_GUEST_IMAGE=... tools/guest/run_guest_suite.py --stress-minutes 10` on the
Windows 11 guest, the driver's fences submitted one at a time and each awaited,
with a vertical blank waited out every 64 fences so dxgkrnl turns that
interrupt source off and on around them:

| | |
|---|---|
| Fences submitted and awaited | ~991,600 (15,494 batches of 64) |
| Vertical blank waits, each an enable/disable of that source | 15,494 |
| Worst single fence wait | 13.06 ms |
| Fences lost, waits that never woke | none |
| Verifier violations, live kernel dumps | none |

**The wait that never wakes did not happen**, which is what the run is for: a
fence whose interrupt arrives while the guest has that source disabled is a
wait nothing ever ends, and 15,000 mask/unmask cycles did not produce one.

**13 ms is the tail, not the typical cost.** The fences here carry no work -
the executor is an echo - so the wait is the round trip: doorbell, worker,
fence page, MSI-X, and the guest's own scheduler getting back to the waiting
thread. What the number bounds is the worst case of that path on an idle
guest; a busy one will be worse, and 062's soak is where that gets measured.

**A defect this run found in the reporting, not the path.** The scenario read
its own count in the same argument list that ran the loop, so the report said
`ok stress.fences 0` beside a worst wait of 13 ms - an argument list may be
evaluated in either order, and this one was. The count is taken before it is
reported now, and the test asserts the number on the line rather than the
counter behind it.
