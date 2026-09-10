# 10 — Case study: `basicrender.sys`, a WDDM 3.2 KMD with no hardware

`basicrender.sys` 10.0.26100.8972 is the **WARP kernel-mode driver** — the
miniport behind Microsoft's software rasterizer. 73 KB, 138 functions, fully
symbolized from `BasicRender.pdb`, and compiled against WDDM 3.2 headers
(the `DXGKDDI_INTERFACE_VERSION_WDDM3_2` constant `0x11007` appears in it).

It is the closest existing thing to what we are building: a **render-capable
WDDM miniport whose "GPU" is software.** Every problem a paravirtual driver
has — no real interrupt, no real engine, no real memory, and a TDR watchdog
that does not care — WARP has already solved, in a form Microsoft ships.

## Its DDI surface

The `WarpKM*` prefix marks the DDI entry points. Complete list as recovered:

```
Lifecycle    WarpKMAddDevice  WarpKMStartDevice  WarpKMStopDevice
             WarpKMRemoveDevice  WarpKMUnload  WarpKMQueryInterface
             WarpKMQueryChildRelations  WarpKMEscape
Interrupt    WarpKMInterruptRoutine (3 bytes — a stub!)  WarpKMDpcRoutine
Caps         WarpKMQueryAdapterInfo  WarpKMGetNodeMetadata
Device       WarpKMCreateDevice  WarpKMDestroyDevice
Process      WarpKMCreateProcess  WarpKMDestroyProcess
             WarpKMCreateProtectedSession
Allocation   WarpKMCreateAllocation  WarpKMDestroyAllocation
             WarpKMDescribeAllocation  WarpKMOpenAllocation
             WarpKMCloseAllocation  WarpKMGetStandardAllocationDriverData
Context      WarpKMCreateContext  WarpKMDestroyContext
Submission   WarpKMRender  WarpKMPatch  WarpKMSubmitCommand
             WarpKMSubmitCommandVirtual  WarpKMValidateSubmitCommand
             WarpKMBuildPagingBuffer
Sync/TDR     WarpKMQueryCurrentFence  WarpKMPreemptCommand
             WarpKMCancelCommand  WarpKMResetFromTimeout
             WarpKMResetEngine  WarpKMQueryEngineStatus
             WarpKMQueryDependentEngineGroup
Present      WarpKMPresent
Misc         WarpKMCalibrateGpuClock
```

Note `WarpKMInterruptRoutine` is **three bytes** — it returns FALSE. There is
no hardware interrupt. Completions are injected instead (below).

## The "GPU" is a worker thread

The internal classes are `WARPKMADAPTER`, `WARPKMGPUNODE`, `WARPKMCONTEXT`,
`WARPKMDMABUFINFO`, `WARPKMALLOCATION`, and command objects
`WARPGPUCMDPRESENT` / `WARPGPUCMDSYNC` each with a `Run()` and a `Discard()`.

```
StartGPU()  →  PsCreateSystemThread(WarpGPUWorkerThread)
                    └─ RunGPU()          the whole GPU, as a loop
```

`RunGPU`'s callees give away its structure exactly:

```
KeWaitForMultipleObjects      wait for work, or for a timeout tick
KeAcquireSpinLockRaiseToDpc
DequeueDmaBuffer              take the head of a node's list
KeReleaseSpinLock
Run                           execute the command object
ExInterlockedInsertHeadList   push back if it could not run yet
CheckDmaBufTimeout            the TDR watchdog, self-administered
Discard                       drop it on cancel
KeSetEvent
```

That is the entire execution engine: a thread, per-node linked lists, a
spinlock, and an event.

## Submission, decompiled

`WarpKMSubmitCommandVirtual` (Ghidra output, lightly annotated — struct offsets
resolved against `DXGKARG_SUBMITCOMMANDVIRTUAL` from `d3dkmddi.h`):

```c
int WarpKMSubmitCommandVirtual(WARPKMADAPTER *adapter,
                               DXGKARG_SUBMITCOMMANDVIRTUAL *arg)
{
    WARPKMCONTEXT *ctx = arg->hContext;
    if (!ctx || (arg->Flags & 8))                 /* 8 = a reject flag  */
        return STATUS_INVALID_PARAMETER;          /* 0xC000000D          */

    if ((arg->Flags & 3) == 0 && (ctx->state & 0xB) == 0) {
        UINT umdSize = arg->DmaBufferUmdPrivateDataSize;
        if (umdSize != 0x118 && umdSize != 0x130)   /* two UMD record shapes */
            return STATUS_INVALID_PARAMETER;

        BYTE *priv = arg->pDmaBufferPrivateData;
        WARPADDRESS *addr  = (WARPADDRESS *)(priv + 0x108);
        UINT        *count = (UINT *)       (priv + 0x110);

        if (umdSize == 0x130) {
            KeAttachProcess(ctx->device->process);   /* touch UM memory */
            status = ctx->SubmitVirtual((WARPGPUUMCMDSYNC *)(priv + 0x118),
                                        addr, count);
            KeDetachProcess();
        } else {
            status = ctx->SubmitVirtual(*(UINT *)(priv + 0x100), addr, count);
        }
        if (status < 0) { ctx->OnSubmitFailed(); return status; }
        /* … rebuild a local copy of arg with the resolved address … */
    }
    adapter->QueueDmaBuffer(arg);
    return status;
}
```

Three things to take from this:

1. **`pDmaBufferPrivateData` is used as the queue node itself.** `QueueDmaBuffer`
   does `ExInterlockedInsertTailList(&adapter->node[arg->NodeOrdinal].queue,
   arg->pDmaBufferPrivateData, &adapter->lock)` — the `WARPKMDMABUFINFO` *is*
   the private data buffer VidMm already allocated for you. No extra
   allocation on the submission path, at DISPATCH_LEVEL, ever.

2. **`KeAttachProcess` for the user-mode sync record.** The 0x130-byte UMD
   private data variant carries a `WARPGPUUMCMDSYNC` that lives in the
   submitting process's address space. The KMD attaches to that process to read
   it. This is the pattern you need whenever the fast path must touch a
   user-mode ring.

3. **Submission never blocks.** `QueueDmaBuffer` ends in
   `ExInterlockedInsertTailList` + `KeSetEvent` and returns. That is the whole
   contract at DISPATCH_LEVEL.

## Completion without an interrupt

`WARPKMADAPTER::WarpSynchronizeNotifyInterrupt` is two indirect calls through
the `DXGKRNL_INTERFACE` stored in the adapter:

```c
BOOLEAN WarpSynchronizeNotifyInterrupt(void *adapter)
{
    /* +0x88 = DxgkCbNotifyInterrupt, +0x10 = DeviceHandle,
       +0x2230 = a prebuilt DXGKARGCB_NOTIFY_INTERRUPT_DATA */
    adapter->cb.DxgkCbNotifyInterrupt(adapter->hDevice, &adapter->notifyData);
    /* +0x38 = DxgkCbQueueDpc */
    return adapter->cb.DxgkCbQueueDpc(adapter->hDevice);
}
```

The worker thread calls this via `DxgkCbSynchronizeExecution`, which runs it at
the adapter's synchronization IRQL — the same guarantee a real ISR would have.
**This is the exact recipe for injecting completions from a non-interrupt
source**, and it is what a driver fed by a host ring must do.

Note the notify-data block is a fixed field at adapter+0x2230, reused every
time, not allocated per completion.

## TDR, self-administered

`WARPKMCONTEXT::CheckDmaBufTimeout` runs inside the worker loop and
`NotifyDmaBufFault` reports a fault. A software GPU still implements the whole
preempt/reset/query-status family, because `dxgkrnl`'s watchdog does not know
or care that there is no hardware. `WarpKMResetFromTimeout`,
`WarpKMResetEngine`, `WarpKMQueryEngineStatus`,
`WarpKMQueryDependentEngineGroup` are all present and real.

`WARPDMABUFEXECUTIONTIME::NotifyDmaBufCompletion` maintains a rolling average
of execution times over a 10-sample window — WARP measures its own throughput
so it can answer engine-status questions honestly. A remoting driver needs the
same, keyed off host progress.

## What to copy, and what not to

**Copy:** the structure. Worker thread per node, DMA private data as the queue
node, `DxgkCbSynchronizeExecution` for completions, self-administered timeout
checking, a stub interrupt routine.

**Do not copy:** the DMA buffer format, and the assumption that execution is
synchronous with the dequeue. WARP runs the command inline in `RunGPU`; a
remoting driver posts it to the host and must handle completion arriving on a
different thread, out of order relative to other nodes.

## Reproducing this analysis

```bash
export GHIDRA_INSTALL_DIR=/opt/ghidra JAVA_HOME=/usr/lib/jvm/java-26-openjdk
tools/re/build_workspace.sh        # fetches the pinned binaries, imports them
PY=~/.local/share/uv/tools/pyghidra-mcp/bin/python
$PY tools/re/q.py re/ghidra-proj wddm basicrender.sys funcs
$PY tools/re/q.py re/ghidra-proj wddm basicrender.sys dec WarpKMSubmitCommandVirtual
$PY tools/re/q.py re/ghidra-proj wddm basicrender.sys xref RunGPU
```
