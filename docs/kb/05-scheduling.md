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

## Preemption and TDR

The KMD must implement, and mean, all of:

| DDI | Contract |
|---|---|
| `DxgkDdiPreemptCommand` | Stop the currently running packet as soon as you can. Report `DMA_PREEMPTED` with the fence id you actually got to. |
| `DxgkDdiCancelCommand` | Remove a not-yet-started packet from the queue. |
| `DxgkDdiQueryEngineStatus` | Is the engine progressing? Used by the TDR watchdog. |
| `DxgkDdiResetEngine` / `ResetHwEngine` | Reset one engine, keep the rest of the GPU alive. |
| `DxgkDdiResetFromTimeout` | Full adapter reset. Called at PASSIVE. |
| `DxgkDdiRestartFromTimeout` | Resume after a reset. |
| `DxgkDdiQueryDependentEngineGroup` | Which engines must be reset together. |
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
