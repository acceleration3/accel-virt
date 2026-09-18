# Implementation plan: user-mode submission in the guest (049)

**Branch**: `049-user-mode-submission-live` · **Spec**: spec.md · **Research**: research.md

## Summary

Take the kernel out of the submission path. Feature 043's transport calls
`D3DKMTSubmitCommand` per submission; WDDM 3.1 lets a process write its own ring
and store to a page of device MMIO instead, and this device's doorbell page is a
KVM ioeventfd that wakes the host worker directly (feature 030 built that half).
So what 049 adds is the near side: the four doorbell DDIs the miniport left NULL
in 024, the capability that makes dxgkrnl ask for them, and a transport that
stores to a doorbell while it is connected and falls back to
`D3DKMTSubmitCommand` while it is not.

The fallback is not a nicety. dxgkrnl disconnects doorbells for suspend, reset,
migration and TDR, and the KMD must make later stores harmless before it
returns - so a transport that assumed a store always arrived would lose work
exactly when the guest was suspended or reset.

## Technical context

C++23 freestanding. The KMD half is `src/core/scheduling/` beside the hardware
queues of feature 019 (`HwQueue`, `HwQueueTable`, `HwQueueSubmissions`), with
one-line `DxgkDdi*` entry points in `src/core/ddi/` and `*Record` structures for
the four `DXGKARG_*` the DDIs take. The guest half is `src/guest/transport/`
over `Kmt`, which gains the three `D3DKMT*Doorbell` calls and
`D3DKMTNotifyWorkSubmission`. The device half exists (030). Tests: the DDI
conformance harness for the DDIs, `MockKmt` for the transport, the full-stack
rig for both halves together, and the guest for the benchmark.

## The protocol, in one paragraph

A process creates a hardware queue and a ring, asks for a doorbell on it
(`D3DKMTCreateDoorbell`, which hands back three mappings: the doorbell, a
status page, and the place to write the fence value it has queued), and
connects it. From then on it writes records into the ring, publishes the tail,
reads the status page, and - while that says `CONNECTED` - stores to the
doorbell and makes no kernel call at all. When the status says
`DISCONNECTED_RETRY` it submits the old way and tries to reconnect; when it says
`DISCONNECTED_ABORT` it destroys the doorbell and makes another.

## Classes this feature introduces

| Class | Kind | State it owns | Responsibility |
|---|---|---|---|
| `Doorbell` (KMD) | object | its queue, its page and its status | One process's doorbell, connected or not |
| `DoorbellTable` | system | every doorbell of the adapter | Handing out and finding doorbells |
| `DoorbellPage` | object | the device page and the dummy page | Which page a process's store lands in |
| `GuestDoorbell` | object | the three mappings and the queue | Submitting through a doorbell while it is connected |
| `SubmissionPath` | object | a doorbell and the kernel path behind it | Which way one submission goes |
| `SubmitBenchmark` | object | the two paths' measured costs | What a submission costs each way |

(Refined against the harness and the transport as each phase lands; a row that
turns out to have no state does not become a class.)

## Phasing

1. **The DDIs**: the four `*Record` structures with their offsets proven, the
   four one-line entry points, `Doorbell`/`DoorbellTable`/`DoorbellPage`, and
   `DXGKQAITYPE_USERMODESUBMISSION_CAPS`. Disconnect remaps to the dummy page
   before it returns.
2. **The transport**: `Kmt` gains the doorbell calls, `GuestDoorbell` and
   `SubmissionPath` choose a way per submission, and `MockKmt` scripts a
   disconnect mid-stream.
3. **Both halves together** in the full-stack rig: a store reaches the device
   worker, a disconnect makes the next store harmless, and no record is lost
   or executed twice across it.
4. **The benchmark and the ratchet** (US3): both paths measured on one boot,
   the ratio recorded.

## Constitution check

- **III**: every class above owns state; the DDI entry points are the one-line
  ABI boundary the constitution allows, and the records are the `*Record`
  exception (public fields, WDK names, offsets proven in
  `src/km/ddi_record_layout.cpp`).
- **IV**: every branch, invariants after every mutation, and a hostile table for
  everything a process writes - the ring tail, the fence value it claims to have
  queued, a store to a doorbell it disconnected (FR-005).
- **V**: the ring and the doorbell are written by user mode. Nothing read from
  them is trusted, and a violation faults that context and no other.
- **VI**: the capability is reported only once the DDIs are implemented and
  tested; until then dxgkrnl never asks.
- **IRQL**: `Connect`/`Disconnect` are PASSIVE (the header says so), and the
  DPC path that completes doorbell-submitted work is DISPATCH, so the tables it
  touches take no lock that can block.
