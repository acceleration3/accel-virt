# Implementation Plan: Host memory and interrupt feasibility on real host GPUs

**Branch**: `039-host-memory-feasibility` | **Date**: 2026-09-13 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/039-host-memory-feasibility/spec.md`

## Summary

M2's three spikes, run for real and turned into capabilities nothing downstream
has to guess at. The device already binds one host GPU per instance and already
owns a hostmem region, a blob table and fence interrupts; what it does not do is
*ask the host whether the fast paths work here*. This feature adds that ask.

Each spike becomes a probe the device runs at realize, on the host GPU the
instance is bound to, against a small allocation:

1. **Host-visible memory** - memory the host GPU exports, mapped into the guest
   through the hostmem BAR, written on one side and read on the other.
2. **Guest-backed import** - a window over guest RAM imported by the host GPU
   with `VK_EXT_external_memory_host`, and the copy fallback measured when it
   is refused.
3. **Interrupt injection under stress** - fences raised from workers while the
   guest masks and unmasks its vectors, measured over a long run.

A probe that passes sets a capset feature bit; one that fails, or was never
run, leaves the bit clear and the guest takes the fallback (constitution VI).
The per-host answers - driver, version, outcome, errno, throughput - are
recorded in the knowledge base by a host-side tool that runs the device over
each GPU and reads back what it decided.

## Technical Context

**Language/Version**: C++23 freestanding for the device core (`src/device`, no
STL, no exceptions, no RTTI), C17 for the QEMU glue (`src/device/qemu`),
Python 3 for the host tools.

**Primary Dependencies**: the patched QEMU 11.1.1 (`build-qemu`), the host
Vulkan loader (already included by `src/device/qemu/virtio-gpu-rvk-host.c` for
GPU enumeration), KVM.

**Storage**: results recorded as Markdown in `docs/kb/` plus the machine
readable JSON the tool writes.

**Testing**: L1 unit tests over `MockHost` for every probe and every capset
bit; L3/L4 device tests for the fallbacks; `qemu_smoke` for the realize path;
the host tool tested against captured output as `tools/host/list_gpus.py` is.

**Target Platform**: Linux host with KVM; guest side unchanged.

**Project Type**: in-process QEMU device plus host tooling.

**Performance Goals**: the probes must not delay realize noticeably - each
allocates at most one page-aligned megabyte and releases it before realize
returns. The interrupt stress run is measured, not bounded: its latency
distribution is the result.

**Constraints**: a probe may never crash QEMU (FR-004) - every failure is a
status, an errno and a cleared bit. A probe runs once per device instance, at
realize, on the main loop.

**Scale/Scope**: this host has two Vulkan devices - Intel Iris Xe (Mesa ANV
26.2.2) and NVIDIA RTX 3070 Ti Laptop (proprietary 580.x). Both advertise
`VK_EXT_external_memory_host`, `VK_KHR_external_memory_fd` and
`VK_EXT_external_memory_dma_buf`. There is no lavapipe on this host and no AMD
GPU, so SC-001's llvmpipe and AMD rows are recorded as not measured.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | How this feature satisfies it |
|---|---|
| I. Specification precedes implementation | This plan and `tasks.md` precede the code; every probe traces to a user story. |
| II. The core is portable; the shim is not | Every Vulkan and KVM call is behind `Host`, implemented once in `src/device/qemu/virtio-gpu-rvk-host.c` and once in `MockHost`. No probe logic in the glue. |
| III. Object-oriented, with state | Classes named below, each with state and one responsibility; no static methods, no free functions outside the existing ABI boundary. |
| IV. Test-first and exhaustive | Each probe's outcome table is an L1 test over `MockHost` before the probe exists; the capset bit is tested set and clear; fallbacks are exercised at L4. |
| V. Everything from outside is hostile | A probe reads what the host driver gives it: sizes, errnos and the bytes it wrote back. Every readback is checked, and a mismatch is a failed probe, not a trusted result. |
| VI. Feature levels are phases | A bit is reported only when its probe passed on this host, which is the whole point of the feature. |
| VII. IRQL and locking are typed | Device side: `require_main_loop` on realize-time probes, `require_worker` on the interrupt stress path. |
| VIII. No speculative structure | Three probes because there are three spikes; one result type because they answer the same shape of question. No abstraction for a fourth. |
| IX. Reproducible and observable | The host tool re-runs the probes on demand and writes machine-readable output; the device logs each probe's verdict with its reason. |

No violations; Complexity Tracking is empty.

## Classes this feature introduces

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `ProbeResult` | object | the outcome (not run, passed, failed), the host's errno, and the bytes or throughput it measured | Carry what one probe found |
| `HostMemoryProbe` | object | its `ProbeResult`, and the block it allocated while it runs | Decide whether host GPU memory reaches the guest through the hostmem region |
| `GuestImportProbe` | object | its `ProbeResult`, and the window it locked while it runs | Decide whether the host GPU imports a window over guest pages |
| `InterruptStressProbe` | object | the fences it raised, the ones the guest acknowledged, and the latency bounds it saw | Judge whether interrupt injection survives masking under load |
| `HostFeasibility` | system | the three results, decided once at realize | Answer which host capabilities this device may offer |

`DeviceCapset` gains `HostFeasibility` as a dependency and turns each passed
probe into a feature bit; `CapsetInfo` gains the two new bits beside
`feature_host_visible_memory`.

## Project Structure

### Documentation (this feature)

```text
specs/039-host-memory-feasibility/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output: the Host ops the probes need
└── tasks.md             # /speckit-tasks output
```

### Source code

```text
src/device/probe/
├── probe_result.{hpp,cpp}          # what one probe found
├── host_memory_probe.{hpp,cpp}     # US1
├── guest_import_probe.{hpp,cpp}    # US2
├── interrupt_stress_probe.{hpp,cpp}# US3
└── host_feasibility.{hpp,cpp}      # US4: the three results, and the bits

src/device/host/host.hpp            # the new ops the probes call
src/device/qemu/virtio-gpu-rvk-host.c  # their Vulkan and KVM implementation
src/device/abi/rvk_device.h         # the ops table the glue fills
src/proto/context/capset_info.{hpp,cpp}  # the new feature bits
src/device/capset/device_capset.{hpp,cpp}

tools/host/probe_host_memory.py     # FR-001: repeatable, machine-readable
docs/kb/04-memory.md                # FR-002: the per-GPU answers

tests/unit/device/probe/            # L1, one file per class
tests/harness/host/mock_host.{hpp,cpp}  # scripts each new op's failures
tests/tools/test_probe_host_memory.py
```

**Structure Decision**: the probes live in a new `src/device/probe/` folder
because they are device behaviour with one responsibility each, and nothing
about them belongs to memory, capset or execution. The Vulkan calls they need
join the existing `Host` contract rather than a second host interface.

## Phase 0: research

`research.md` records what the host already tells us without running anything -
the Vulkan devices, their external-memory extensions, their heaps - and what
only a probe can answer. It also settles three design questions: where the
probe allocation comes from, how the guest side of US1 is checked without a
Windows guest, and what "masking a vector" means to a driver we control.

## Phase 1: design

`data-model.md` gives each class its state and transitions (a probe is *not
run* until it runs, then *passed* or *failed*, and never runs twice).
`contracts/host_ops.md` names the exact `Host` methods the probes need, their
threading rules and their failure modes, because that contract is what the
QEMU glue and `MockHost` must both satisfy. `quickstart.md` is how to run the
probes on a host and read the answers.

## Complexity Tracking

None.
