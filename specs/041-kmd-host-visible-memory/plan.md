# Implementation Plan: KMD host-visible memory a process can map

**Branch**: `041-kmd-host-visible-memory` | **Date**: 2026-09-13 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/041-kmd-host-visible-memory/spec.md`

## Summary

The device can already hand the guest real host memory: a host-visible blob is
a memfd the device places at a page-aligned offset of the hostmem BAR, and a
qtest has been writing through that BAR since feature 031. The guest driver has
never touched it. It reports one segment - an aperture over guest system memory
- and sends every allocation that is not the primary to VidMm's own system
memory, so a process that asks for host-visible memory gets guest RAM with a
flag set on it.

This feature is the driver side of that BAR:

1. **A second segment, CPU-visible, whose `CpuTranslatedAddress` is the hostmem
   BAR.** Reported only when the capset says the device has the region and the
   PCI walk found it, so an adapter without it keeps exactly today's shape.
2. **The whole region as one blob**, created and mapped at offset 0 as soon as
   the segment is offered. VidMm owns the segment's address space and maps a
   process's view itself (`MapInCpuVisibleSegment`, dxgmms2.sys @14009ea98), so
   the driver is never told where an allocation landed - and every offset VidMm
   can choose has to have host memory behind it already.
3. **A host-visible allocation with nowhere to be evicted to**: that segment is
   its only supported one and its eviction set is empty, which is what makes a
   process's view the host's memory rather than a copy of it.
4. **The fallback said out loud.** With no region, the same allocation is
   guest-backed and reports that it is (FR-007); nothing silently aliases guest
   RAM.

What this feature does not do is hand the memory to a process: no user-mode
client exists until feature 043. The tests therefore prove what this side can
prove - the segment VidMm is given, the placement, the blob, the paging
sequence, and, on the real guest, that the host's own view of the BAR holds
what the driver wrote through it.

## Technical Context

**Language/Version**: C++23 freestanding for `src/core` and `src/device`;
Python 3 for the qtests and the L5 runner.

**Primary Dependencies**: 039's capset bits and its per-GPU probe results;
040's `CapsetExchange`, which is what puts the capset in the adapter's hands at
StartDevice; 031's blob table and hostmem region on the device side.

**Testing**: L1 for every new class over `MockPlatform`; L2 for the paging and
QueryAdapterInfo sequences VidMm drives (`FakeDxgkrnl`); L3 for the blob
requests the driver now emits, against the real device; L5 for a live guest
whose QEMU has a region, with the host reading the BAR back through QMP.

**Target Platform**: Windows 11 guest on the patched QEMU, KVM; the device on
Linux/KVM with a bound host GPU.

**Performance Goals**: none of its own beyond the claim SC-002 makes - a write
through a mapped view reaches the device with no command sent per access.

**Constraints**: `DxgkDdiCreateAllocation` and the paging path run at PASSIVE,
but nothing on them may block on the device: a control exchange there would
stall VidMm, which is why the region's blob is made once at StartDevice and not
per allocation. A descriptor from user mode is hostile input.

**Scale/Scope**: one adapter, one region, one blob; as many host-visible
allocations as VidMm can fit in the region it was told about.

## Constitution Check

| Principle | How this feature satisfies it |
|---|---|
| I. Specification precedes implementation | This plan, research.md and tasks.md precede the code; each task names the story it serves. |
| II. The core is portable; the shim is not | Every decision is in `src/core`; the BAR reaches the driver through `Platform` and the existing PCI walk, and nothing new is kernel-only. |
| III. Object-oriented, with state | Three classes below, each owning state and one responsibility; the segment list owns the collection. |
| IV. Test-first and exhaustive | Each class's L1 tests precede it; the descriptor reader gains a hostile table test and its fuzz target already exists. |
| V. Everything from outside is hostile | An allocation descriptor is validated before a blob is created, and the device's `OK_MAP_INFO` answer is judged rather than assumed. |
| VI. Feature levels are phases | The segment is reported only where the capset proves the region; no capability is claimed that the device cannot back (FR-006). |
| VII. IRQL and locking are typed | Every new method states its ceiling; the blob's exchanges are PASSIVE, because they wait for the device to answer. |
| VIII. No speculative structure | No user-mode mapping interface is invented for a client that does not exist yet (043); no aperture-window DDIs for a BAR that is addressable whole. |
| IX. Reproducible and observable | A region that cannot be made is logged once with the status that stopped it; the L5 run reads the host's own view of the BAR back. |

No violations.

## Classes this feature introduces

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `HostVisibleSegment` | object | the hostmem region's guest physical base and size, and whether the device offers it | Describe the CPU-visible segment to VidMm |
| `MemorySegments` | system | the segments this adapter reports, in the order VidMm numbers them | Answer what memory this adapter has |
| `HostVisibleBlob` | object | one allocation's blob: its resource id, its placement, and whether it is mapped | Keep one allocation's memory on the device |

`HostmemPlacement` was a fourth class here and is not one any more: with the
region mapped as a single blob, VidMm owning the segment's address space and
mapping a process's view itself, nothing in this feature needs to know which
allocation holds which range. What will need it is the executor being told
where a context's buffer is (feature 042), and it can be written then against a
caller that exists.

`RvkRequestWriter` gains the two requests the driver has never sent -
`RESOURCE_CREATE_BLOB` for a host blob and `RESOURCE_MAP_BLOB` - and
`GpuResponseReader` the `OK_MAP_INFO` answer to judge. Those are wire encoders
and decoders that already exist as classes; this feature adds methods to them,
not new types.

## Project Structure

```text
src/core/memory/host_visible_segment.{hpp,cpp}    # FR-001, FR-006
src/core/memory/memory_segments.{hpp,cpp}         # FR-001
src/core/memory/host_visible_blob.{hpp,cpp}       # FR-001, FR-005
src/core/memory/allocation.{hpp,cpp}              # where a cpu-visible allocation goes
src/core/adapter/adapter.{hpp,cpp}                # the capset gate and the segment answer
src/proto/virtio_gpu/rvk_request_writer.hpp       # host blob, map blob, unmap blob
src/proto/virtio_gpu/gpu_response_reader.{hpp,cpp}# OK_MAP_INFO

tests/unit/core/memory/test_host_visible_segment.cpp
tests/unit/core/memory/test_memory_segments.cpp
tests/unit/core/memory/test_host_visible_blob.cpp
tests/unit/scenarios/test_ddi_scenarios.cpp       # the segment and blob sequence
tests/qemu/test_virtio_gpu_blobs.py               # the requests the driver emits
tools/guest/run_guest_suite.py                    # hostmem= and the host's read-back
```

**Structure Decision**: everything lives beside `ApertureSegment` in
`src/core/memory/`, because what changes is which memory the adapter has and
where an allocation goes in it; the adapter only asks.

## Phase 0: research

`research.md` settles what the device already does with blobs and offsets, which
of the WDK's two CPU-visible segment shapes to report and why, why a blob cannot
follow VidMm's placement and what is done instead,
what the absence of the region has to mean, and how far the ring and fence pages
can go before feature 043 exists.

## Phase 1: design

`data-model.md` gives each class its state, its transitions and its invariants;
`contracts/` records the DDI sequence VidMm drives for a host-visible allocation
and the wire requests the driver sends for it; `quickstart.md` says how to run
the new L3 and L5 checks.

## Complexity Tracking

None.
