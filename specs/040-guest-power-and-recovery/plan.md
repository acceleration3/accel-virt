# Implementation Plan: Real-guest power, restart and recovery

**Branch**: `040-guest-power-and-recovery` | **Date**: 2026-09-13 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/040-guest-power-and-recovery/spec.md`

## Summary

Windows suspends, hibernates, disables, upgrades and times out every GPU it
has. The driver survives a clean boot today; this feature is what makes it
survive the rest.

Four things change:

1. **Power transitions put the device back the way they found it.** D3 tears
   the virtio device down and D0 builds it again - but the guest's object graph
   outlives both, so what resumes has to be reattached, not assumed.
   Allocations keep their aperture pages, the primary is scanned out again, and
   anything the device cannot restore is reported lost rather than used.
2. **`QueryEngineStatus` answers about the host, not about the ring.** Today it
   says "responsive" whatever is happening, which is the one answer a remoting
   driver must not give: dxgkrnl asks precisely when it suspects a hang, and a
   driver that always says yes turns an engine reset into an adapter reset.
3. **A stalled or faulted context is lost alone.** `ResetEngine` already aborts
   the node's queue; what is missing is the part that matters to a second
   process - its context keeps completing while the first one's is reported
   lost.
4. **The test executor can be told to stall or fault one context** from the
   in-guest runner, so L5 can produce a real TDR without a rebuild (FR-006).

## Technical Context

**Language/Version**: C++23 freestanding for `src/core` (the miniport) and
`src/device`; C17 for the QEMU glue; Python 3 for the runner.

**Primary Dependencies**: 038's live guest, 033's device-side fault and reset,
the echo executor in `tests/qemu/echo_executor.cpp`.

**Testing**: L1 for every new class over `MockPlatform`/`MockHost`; L2 for the
DDI sequences dxgkrnl produces around power and TDR (`FakeDxgkrnl`); L4 for a
faulting executor end to end; L5 for S3, S4, disable/enable, upgrade and an
injected stall.

**Target Platform**: Windows 11 guest on the patched QEMU, KVM.

**Performance Goals**: none of its own. SC-003's bound - recovery within 10 s of
a 5 s stall - is dxgkrnl's timeout plus our reset, and the reset itself must not
block the DPC.

**Constraints**: the TDR path runs at `Irql::dispatch` and inside dxgkrnl's own
timeout; nothing on it may allocate, block or take a mutex. A power transition
runs at PASSIVE but may not assume the device answers - a suspended host can be
gone by the time D0 arrives.

**Scale/Scope**: one adapter, one node, up to `DeviceTable::max_devices`
guest devices and their contexts.

## Constitution Check

| Principle | How this feature satisfies it |
|---|---|
| I. Specification precedes implementation | This plan and tasks.md precede the code; every task cites the story it serves. |
| II. The core is portable; the shim is not | Every new decision is in `src/core`; the guest's own suspend and resume come through `Platform`, and the runner drives Windows from outside. |
| III. Object-oriented, with state | Three classes below, each owning state and one responsibility. |
| IV. Test-first and exhaustive | Each class's L1 tests are written before it; the TDR and power sequences are L2 scenarios citing the dxgkrnl functions they mirror. |
| V. Everything from outside is hostile | A resumed device is not trusted: its configuration is read again, and a device whose identity changed across hibernation is refused rather than used (FR-005). |
| VI. Feature levels are phases | No capability is claimed by this feature; what changes is what the driver answers about itself. |
| VII. IRQL and locking are typed | `EngineProgress` is readable at DISPATCH and takes no lock a DPC could wait on; the power path is PASSIVE and says so. |
| VIII. No speculative structure | No abstraction for transitions that do not exist: S3 and S4 arrive as the same DDI calls, and the driver does not model them separately. |
| IX. Reproducible and observable | Every loss is logged once with the value that caused it (FR-007); the runner's scenarios are scripted and repeatable. |

No violations.

## Classes this feature introduces

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `EngineProgress` | object | the interrupt time and fence the node last moved at, and what it last told dxgkrnl | Say whether the engine is making progress |
| `PowerTransition` | object | what the adapter had running when power went away: whether the device was started, and which sources were scanning out | Put the adapter back the way power found it |
| `ContextLoss` | system | which contexts could not be carried across a transition or a reset, and whether their owners have been told | Report a context nobody can finish |

> **Superseded (044):** the driver never reports `DMA_FAULTED` - dxgmms2.sys `VidSchDdiNotifyInterruptWorker` @140031b80 bugchecks 0x119/0xD on `DXGK_INTERRUPT_DMA_FAULTED` from a GPU MMU adapter; a faulted context's queued work is completed, its later submissions are refused, ResetEngine reports nothing more, and `ContextLoss` is deleted (`docs/kb/05-scheduling.md`).

The echo executor gains stall and fault controls; it is a test double in
`tests/qemu/`, not a class in `src/`.

## Project Structure

```text
src/core/scheduling/engine_progress.{hpp,cpp}     # FR-002
src/core/scheduling/context_loss.{hpp,cpp}        # FR-003
src/core/adapter/power_transition.{hpp,cpp}       # US1
src/core/adapter/adapter.{hpp,cpp}                # the DDIs that use them

tests/unit/core/scheduling/test_engine_progress.cpp
tests/unit/core/scheduling/test_context_loss.cpp
tests/unit/core/adapter/test_power_transition.cpp
tests/unit/scenarios/test_ddi_scenarios.cpp       # the power and TDR sequences
tests/qemu/echo_executor.cpp                      # stall and fault controls
tools/guest/run_guest_suite.py                    # suspend, resume, cycle, upgrade
tools/guest/provision.ps1                         # pnputil stages
```

**Structure Decision**: the two scheduling classes live beside `ExecutionNode`,
which owns the progress they read; `PowerTransition` lives beside the adapter,
because what it puts back is the adapter's own.

## Phase 0: research

`research.md` answers what only dxgkrnl and a live guest can: what dxgkrnl does
between `QueryEngineStatus` and `ResetEngine`, what it expects a driver to have
done by the time `SetPowerState(D0)` returns, what survives S4 in the guest's
own object graph, and how a stall is injected into the executor from inside the
guest without a rebuild.

## Phase 1: design

`data-model.md` gives each class its state and transitions, `contracts/` the
DDI sequences this feature must survive (power, TDR, disable) as the tests will
drive them, and `quickstart.md` how to run the new L5 scenarios.

## Complexity Tracking

None.
