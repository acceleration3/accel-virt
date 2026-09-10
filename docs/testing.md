# Testing a WDDM driver

Yes, a display miniport can be unit tested — thoroughly, on Linux, without
booting Windows. It is not the default way people write drivers, which is why
most driver bugs are found by bugchecking a VM. It works only if the driver is
structured for it from the first commit, which is what Principle II of the
constitution buys.

There are five layers. The first four run on this Linux box in seconds. The
fifth needs the VM and is the smallest.

```
 L5  in-guest integration      real Windows, real dxgkrnl        minutes, manual/CI-VM
 L4  loopback protocol         guest core ↔ device core, one process
 L3  fuzzing                   libFuzzer + ASan/UBSan on every parser
 L2  DDI conformance harness   a fake dxgkrnl drives the real DDI table
 L1  unit tests                every function, every branch, invariants, garbage
```

---

## L1 — Unit tests on the portable core

The whole point of splitting `src/core` from `src/km` is that `src/core`
compiles as ordinary C11 on Linux. It has no `#include <ntddk.h>`, no
`ExAllocatePool`, no `KeAcquireSpinLock`. It gets those through
`src/core/platform.h`, which has two implementations: the kernel one in
`src/km/`, and the test one in `tests/harness/platform_mock.c`.

So the same object code paths that run in the guest kernel run in a Linux
process, under ASan, UBSan and a coverage instrument.

What lives in core, and is therefore unit-testable:

| Module | What it decides |
|---|---|
| `gpuva` | GPU virtual address space: reserve, map, unmap, free, the handle encoding, overlap rules |
| `alloc` | Allocation records, segment placement, residency state, refcounts, shared/NT-shared identity |
| `segment` | Segment descriptors, what `QueryAdapterInfo` reports, placement legality |
| `paging` | `BuildPagingBuffer` operation emission and the byte budget |
| `ctx` | Contexts, nodes, engine affinity, priority |
| `submit` | Descriptor construction, fence id assignment, queue depth, backpressure |
| `fence` | Monitored/native fence bookkeeping, monotonicity, threshold wakeups |
| `tdr` | Progress tracking, timeout decision, preempt/reset state machine |
| `vidpn` | Topology validation, cofunctional modality enumeration, mode sets |
| `edid` | EDID block synthesis and its checksum |
| `virtq` | Split virtqueue rings: avail/used indices, descriptor chains, wrap |
| `vpci` | virtio PCI capability walk, feature negotiation, device status FSM |
| `ring` | The rvk submission ring: head/tail, wrap, full/empty, slot lifetime |
| `proto` | Wire structs, encode/decode, version negotiation |

What does **not** live in core, and is therefore not unit tested: mapping a
BAR, creating a system thread, calling `DxgkCbNotifyInterrupt`, writing an MMIO
register. Those are shim, they contain no decisions, and L2 covers their
sequencing.

### The mock platform

`tests/harness/platform_mock.c` is not a stub. It is an adversarial
implementation:

- **Simulated IRQL.** Raise and lower are tracked. Calling a function annotated
  `AV_AT_PASSIVE` while at DISPATCH aborts the test. Allocating at DISPATCH
  aborts. Blocking at DISPATCH aborts. This turns the most common class of
  driver bugcheck into a failing unit test.
- **Failing allocator.** Every allocation site can be made to fail on the Nth
  call, so error paths get covered rather than assumed. A test sweep runs the
  whole suite once per allocation site, failing that site, and asserts no leak
  and no crash — this is how out-of-memory paths reach 100% branch coverage.
- **Leak and balance tracking.** Allocation, lock acquire/release, and refcount
  operations are counted; a test that ends unbalanced fails.
- **Lock order checking.** The declared order in `src/core/locks.h` is enforced
  at runtime.
- **Deterministic time.** A test controls the clock, so TDR timeouts are exact
  and not flaky.

### The shape of a test file

Every core module `x.c` has `tests/unit/test_x.c` with three sections:

```c
/* 1. branches — one test per decision, named for the decision */
TEST(gpuva_map, rejects_zero_length)
TEST(gpuva_map, rejects_range_crossing_object_boundary)
TEST(gpuva_map, accepts_exact_fit_at_upper_bound)
TEST(gpuva_map, rejects_overlap_with_existing)
TEST(gpuva_map, returns_no_memory_when_node_alloc_fails)

/* 2. invariants — asserted after every mutation, not once at the end */
TEST(gpuva, invariants_hold_across_random_operation_sequence)

/* 3. garbage — the hostile-input table plus a fuzz entry point */
TEST(gpuva_decode_handle, hostile_inputs)
```

The hostile-input table is a shared idiom, not per-test boilerplate:

```c
static const struct av_hostile_case cases[] = {
    AV_HOSTILE_EMPTY(),
    AV_HOSTILE_TRUNCATED_AT_EACH_FIELD(struct rvk_submit_desc),
    AV_HOSTILE_LEN_EXCEEDS_BUFFER(),
    AV_HOSTILE_MISALIGNED(),
    AV_HOSTILE_SCALAR_EXTREMES(struct rvk_submit_desc),
    AV_HOSTILE_OFFSET_OVERFLOW(),
    AV_HOSTILE_CYCLIC_CHAIN(),
    AV_HOSTILE_BITFLIP_SWEEP(valid_desc, sizeof valid_desc),
};
```

Each case asserts the same contract: an error is returned, nothing outside the
destination is written, the call terminates, and ASan/UBSan stay quiet.

---

## L2 — The DDI conformance harness

A unit test proves `gpuva_map` is correct. It does not prove the driver
survives the sequence `dxgkrnl` actually performs, at the IRQLs it actually
uses, with the reentrancy it actually allows.

So `tests/harness/fake_dxgk.c` is a fake `dxgkrnl`: it builds a
`DRIVER_INITIALIZATION_DATA`, calls `DriverEntry`, and then drives the DDI
table through scripted scenarios — start the adapter, query every
`DXGKQAITYPE`, create a device, create allocations, create a context, render,
submit, complete a fence, preempt, time out, reset, stop, remove.

**The scenarios are derived from reverse engineering, not from guesswork.**
`dxgkrnl.sys` 10.0.26100.9444 is imported into Ghidra with 106k symbols in
`../claude-scratch/wddm-kb/ghidra-proj`. The real call order, the real
argument invariants, and the real IRQLs are readable there. `docs/ddi-sequences.md`
records each sequence with the function in `dxgkrnl` it was taken from, so a
scenario can be re-derived when it is questioned.

The harness also injects what a well-behaved `dxgkrnl` is allowed to do and a
naive driver forgets to handle:

- `QueryAdapterInfo` for a type the driver does not support — must return
  `STATUS_NOT_SUPPORTED`, not garbage.
- `StopDevice` while packets are in flight.
- `PreemptCommand` for a fence id that already completed.
- `DestroyAllocation` for an allocation still referenced by a submitted packet.
- `ResetFromTimeout` followed by `RestartFromTimeout`, twice.
- Re-entrant `SubmitCommand` on two nodes concurrently.
- Surprise removal in the middle of paging.

This layer is where "the driver loaded and D3D12 still refuses to create a
device" gets caught in a second instead of in a VM.

---

## L3 — Fuzzing

Every parser gets a libFuzzer target in `tests/fuzz/`, built with ASan+UBSan,
with a committed seed corpus in `tests/corpus/<target>/`.

Targets, at minimum: the rvk command decoder, the submission descriptor, the
capset payload, virtqueue descriptor chains, the virtio PCI capability walk,
device configuration space, EDID input, VidPn structures arriving from
`dxgkrnl`, and every DDI argument struct the driver dereferences.

Two directions matter, and both are real:

- **Guest → device.** The threat model from the constitution: guest bytes
  decoded inside QEMU next to the host GPU driver.
- **Device → guest.** A hostile or buggy host must not be able to corrupt the
  guest kernel. The driver must validate what the device tells it, including
  fence values, ring indices, and configuration space.

CI runs a 60-second smoke per target. A longer nightly run is a separate job
with corpus minimization committed back.

---

## L4 — Loopback: the whole protocol, one process, no VM

`tests/loopback/` links the guest core and the device core into a single Linux
binary and connects them with an in-memory transport that mimics the real one:
a ring in a shared buffer, a doorbell that is a function call, an interrupt
that is a callback.

That gives end-to-end tests of everything except the actual hardware
mechanisms:

- allocate → map GPU VA → submit → fence → complete
- shared allocation opened by a second simulated process
- ring full, backpressure, and recovery
- a context faulting and being torn down without disturbing another context
- protocol version mismatch handled on both sides
- a hostile guest driving the device core with malformed commands
- a hostile device driving the guest core with malformed completions

Loopback tests are the cheapest place to find protocol design errors, and
protocol design errors are the expensive kind. Write the loopback test for a
protocol change before writing either side of it.

---

## L5 — In the guest

The smallest layer, and the only one that needs Windows.

- **Driver Verifier**, always on for `rvkgpu.sys` during development: special
  pool, IRQL checking, DMA verification, low-resource simulation. Not optional.
- **D3DKMT integration tests**: a user-mode test program calling
  `D3DKMTCreateAllocation2`, `MapGpuVirtualAddress`, `SubmitCommand`,
  `WaitForSynchronizationObjectFromCpu` against the real driver. These mirror
  the L4 loopback tests, so a divergence points at the shim.
- **The application suite**: vkd3d-proton and DXVK samples, then real
  applications.
- **HLK / WHLK** graphics tests, only if the driver is ever to be signed for
  distribution. Out of scope until then, but the display tests are a useful
  correctness oracle even unsigned.

Guest runs are driven from Linux over the QEMU guest agent, with a `virsh`
snapshot before every driver install, because a miniport that fails in
`StartDevice` can leave the guest with no display.

---

## What this does and does not buy

**Does:** every branch of every decision, error paths including out-of-memory,
IRQL violations, lock-order violations, leaks, protocol misuse in both
directions, and the entire wire format — all before Windows boots.

**Does not:** prove the shim's platform calls are correct, prove `dxgkrnl`
behaves as the harness assumes, or catch a hardware/hypervisor interaction.
Those are L5's job, and they are why L5 exists even though it is small.

The honest failure mode of this approach is a harness that encodes a wrong
belief about `dxgkrnl`. That is why every scenario cites the function it was
derived from: when the guest disagrees with the harness, one of them is wrong
and the citation says where to look.
