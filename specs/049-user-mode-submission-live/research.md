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
