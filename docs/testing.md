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
compiles as ordinary freestanding C++23 on Linux. It has no `#include <ntddk.h>`,
no `ExAllocatePool`, no `KeAcquireSpinLock`. It reaches the platform only
through an injected `av::Platform&`, which has two implementations: the kernel
one in `src/km/`, and `MockPlatform` in `tests/harness/`.

So the same object code paths that run in the guest kernel run in a Linux
process, under ASan, UBSan and a coverage instrument.

The classes that live in core, and are therefore unit-testable. Each is either
an **object class** (one instance per real thing) or a **system class** (a
service owning a collection); nothing here is a namespace with static methods.

| Class | Kind | What it decides |
|---|---|---|
| `Alignment` | object | Power-of-two alignment; rounding up without wrapping |
| `ByteRange` | object | A `[base, base + size)` span and whether it is representable |
| `VirtualRange` | object | One reserved-or-bound span and its binding |
| `GpuVirtualAddressSpace` | system | Reserve, map, unmap, release, translate; overlap and ordering rules |
| `Allocation` | object | One allocation: its descriptor, page-rounded size, open count |
| `AllocationTable` | system | The adapter's allocations, created and destroyed all-or-nothing |
| `DeviceTable` | system | The adapter's devices and what each has open |
| `ApertureSegment` | object | The memory segment reported to VidMm |
| `PagingEngine` | system | `BuildPagingBuffer` operation emission and the byte budget |
| `Context` | object | One execution context: its device, node and flags |
| `ExecutionNode` | object | One node's pending packets, completed and preempted from the DPC |
| `RenderRequest` / `PatchRequest` | object | One command buffer into its DMA buffer; one submission's addresses |
| `SubmissionRing` | object | Head/tail, wrap, full/empty, slot lifetime |
| `MonitoredFence` | object | Value, monotonicity, registered thresholds |
| `TimeoutDetector` | system | Progress tracking, timeout decision, preempt/reset state machine |
| `VidPnTopology` | object | Paths, cofunctional modality enumeration, mode sets |
| `Edid` | object | EDID block synthesis and its checksum |
| `VirtQueue` | object | Split ring: avail/used indices, descriptor chains, wrap |
| `VirtioPciTransport` | system | Capability walk, feature negotiation, device status |

What does **not** live in core, and is therefore not unit tested: mapping a
BAR, creating a system thread, calling `DxgkCbNotifyInterrupt`, writing an MMIO
register. Those are shim, they contain no decisions, and L2 covers their
sequencing.

### The Vulkan ICD

`rvkvk.dll`'s decisions live in `src/icd/vulkan/` and are tested the same way
(feature 045): the ICD is portable C++ over injected seams, and `src/um/vulkan/`
holds only their Windows implementations - `ProcessHeap` (a `BlockSource`),
`SrwLock` (a `ThreadLock`), `FileLog` (a `LogSink`), `GdiKmt` - and the two
exports. On Linux the seams are `MockPlatform`, `RecordingThreadLock` (every
acquire and release written to one `LockTrace`, so a test asserts the order
across the root's, the send and the memory locks) and `RecordingLog`, and the
Kmt is `MockKmt`, which plays the host's replies through `ScriptedHost`.

`IcdRig` (`tests/harness/icd/`) builds an `IcdRoot` over those and reaches it
only through the entry points the loader would be handed, so a unit test is a
Vulkan call sequence: negotiation, `vkCreateInstance`, adapter enumeration by
LUID, device creation under `ExtensionPolicy`, command buffers recording into
`CommandBatch` (read back off MockKmt's ring with `ShippedBatch`), sliced
waits against MockKmt's counter, host-visible allocations placed in chunks and
mapped as MockKmt's lock views, and device loss through a withheld answer. The
generated wrappers are linked as the DLL links them (`av_icd`), so a reply the
host shapes wrongly reaches the ICD's own parsing, which has hostile tables and
fuzz targets (`fuzz_physical_device_view`, `fuzz_extension_policy`,
`fuzz_memory_type_table`).

### The trace tracker in user mode

What decides in the tracker's Windows side is portable and tested like the
ICD (docs/trace.md): `InspectProgram` - `rvkinspect.exe`'s commands, the
names it tries, the snapshot files it writes - over `MockInspectHost`, which
scripts processes, named sections and state services and keeps every file
written; `EscapeReader`, the driver's table and dump read a window at a time,
over `MockKmt`'s adapter-only escapes answered by `ScriptedKernelTrace`, which
cuts a window short, changes the total, refuses or answers another request at
any page; `CrashReport` over `ScriptedModuleResolver`; and the ICD's state
dump through `IcdRig`, with `RecordingThreadLock::held_elsewhere` standing for
a root lock another thread holds. `fuzz_command_line` and `fuzz_escape_reader`
feed the two readers of foreign bytes. The Windows shims in `src/um/trace/` and
`src/um/inspect/` only translate and are proven by the guest run.

### The mock platform

`Platform` is an abstract base with exactly two implementations — that is what
keeps the abstraction from being speculative. `KernelPlatform` in `src/km/`
wraps the real kernel; `MockPlatform` in `tests/harness/` is injected into every
core class under test.

`MockPlatform` is not a stub. It is adversarial:

- **Simulated IRQL.** Spin locks raise to DISPATCH and mutexes to APC; release
  restores. A function called above the ceiling it declares with
  `platform_.require_irql_at_most(...)`, an allocation at DISPATCH, and a
  mutex taken at DISPATCH are each recorded as a violation. This turns the most
  common class of driver bugcheck into a failing unit test.
- **Violations fail the test unless claimed.** Every rule break is printed once
  with its cause and counted by kind (`tests/harness/platform/violation.hpp`). A test
  that means to break a rule claims it with `platform.claim(Violation::...)`;
  anything left unclaimed when the mock is destroyed fails the test. Recording
  instead of aborting is what lets a test prove the violation is detected. Fuzz
  targets trap on any unclaimed violation.
- **Failing allocator.** `fail_allocation_at(n)` makes the Nth allocation
  fail, so error paths get covered rather than assumed. A sweep runs the same
  scenario once per allocation site, failing that site, and asserts no leak and
  no crash — this is how out-of-memory paths reach 100% branch coverage.
- **Register access through a mapping only.** `read_register_*` and
  `write_register_*` reach a fake device region only inside a window the test
  (or the fake dxgkrnl's `DxgkCbMapMemory`) attached; any other address is an
  `unmapped_register` violation, so a driver touching a BAR it unmapped, or
  reading past a structure, fails the test.
- **Leak and balance tracking.** Live allocations and held locks are counted;
  a test that ends with either fails. Releasing null is a violation (the kernel
  bugchecks on it). Refcount balance arrives with the objects that own
  refcounts (feature 008).
- **Lock order checking.** Ranks are declared once in `src/core/platform/lock_rank.hpp`.
  Acquisition must strictly increase the rank, release must be LIFO, and both
  are checked on every acquire and release. This runs in the native test build;
  in the guest, Driver Verifier's deadlock detection covers it.
- **Deterministic time.** A test controls the clock, so TDR timeouts are exact
  and not flaky.

### The mock host

The device core's counterpart is `Host` (`src/device/host/host.hpp`), with
`TableHost` over the C operations table the QEMU glue fills and `MockHost`
(`tests/harness/host/`) in tests. Where the guest core is single-threaded under
test, the device core's point is threads, so `MockHost` runs real pthreads and
the worker tests run under ASan and UBSan:

- **The threading contract is checked.** The thread that constructed the mock is
  the main loop. A lifecycle or virtqueue method called from a worker, or worker
  work run on the main loop, is a `HostViolation`, claimed like the platform's.
- **Main-loop work is explicit.** `run_on_main_loop` queues; a test waits for
  the work it expects (`wait_for_main_loop_work`) and runs it
  (`run_main_loop`), so completion order is asserted, not hoped for.
- **Failing creation.** Any thread, event or mutex creation can be failed, and
  a thread, event, mutex or main-loop routine left over when the mock is
  destroyed fails the test. `MockHostOps` exposes the mock as the C table, so
  `TableHost` and the `rvk_device.h` entry points run against it too.

The glue itself is exercised by the `qemu` gate configuration: the patched
QEMU is built and `tests/qemu/qemu_smoke.py` reads the PCI function over qtest.

### The device under qtest

`tests/qemu/virtio_qtest.py` is a virtio-gpu driver that needs no guest: over
qtest it assigns the function's BARs, walks the virtio capabilities, negotiates
features, lays split virtqueues out in guest RAM and polls their used rings.
The `qemu_virtio_gpu` ctest runs `tests/qemu/test_*.py` with it against the
patched QEMU's stock `virtio-gpu-pci` and `virtio-gpu-rvk-pci` side by side, so
a behaviour the rvk device changes shows up as a difference from stock. Where
the driver already sends a request, the test sends the driver's own bytes:
`gpu_request_bytes` prints what `GpuRequestWriter` writes. Device features past
the base class extend this suite: `test_virtio_gpu_blobs.py` finds the
host-visible shared memory region among the capabilities and reads and writes
mapped blobs through BAR 4 with qtest's memory commands, and
`test_virtio_gpu_contexts.py` reads the capset, checking its GPU identity
against `vulkaninfo` through `tools/host/list_gpus.py`, and creates, sets up and
destroys contexts. `test_virtio_gpu_ring.py` writes rings as the guest does
(`rvk_context.py`), stores to the doorbell pages in BAR 0 and waits for head -
QEMU dispatches the ioeventfd itself under qtest - and checks that a hostile
ring stops only its own context. `test_virtio_gpu_fences.py` reads fence pages
and the MSI-X pending bits of masked vectors, and the ISR with MSI-X off, to see
the fence queues' interrupts arrive; it also drives the sequences a restarting
Windows display driver performs - MSI-X toggled while `DRIVER_OK` stands, a
driver that stops by clearing `DRIVER_OK` without a reset and is replaced by one
that toggles MSI-X, MSI-X enabled only after `DRIVER_OK`, and a sweep of toggles
that must leak no descriptor - because that sequence aborted QEMU on 035's first
live run. `test_virtio_gpu_execution.py` runs
batches through `rvk_echo_executor` (`tests/qemu/echo_executor.cpp`), an executor
library the qemu configuration builds so the execution path runs on the real
device without render-server, and reads answers from reply slots.
`test_virtio_gpu_faults.py` faults contexts for every reason the echo executor
can reach and reads the faulted page, the pending vector, a healthy neighbour,
a recreated context and progress per command.

### The shape of a test file

Every core class `X` has `tests/unit/test_x.cpp` with three sections:

```cpp
/* 1. branches — one test per decision, named for the decision */
TEST(address_space_map, rejects_an_unreserved_base)
TEST(address_space_map, rejects_object_zero_because_zero_means_unmapped)
TEST(address_space_map, rejects_an_offset_that_overflows_with_the_size)
TEST(address_space_map, rejects_a_double_map)
TEST(address_space_reserve, propagates_allocation_failure)

/* 2. invariants — asserted after every mutation, not once at the end */
TEST(address_space_invariants, hold_across_a_pseudorandom_operation_sequence)

/* 3. garbage — the hostile sweep plus a fuzz entry point */
TEST(address_space_translate, hostile_addresses_never_translate_out_of_bounds)
```

A fixture injects the mock and owns the leak check, so no test repeats it:

```cpp
AddressSpaceFixture fixture;
const std::uint64_t reserved = fixture.reserve_page();
MUTATE(fixture, fixture.space().map(reserved, kPage, 7u, 0u), Status::ok);
```

`HostileSweep` is an object, not per-test boilerplate: given a parser and one
valid input it feeds empty, every truncation, oversized length, all-zero,
all-ones and a bit-flip sweep. `GuardedBuffer` surrounds the destination with
poisoned bands so a write past the end is caught.

Each case asserts the same contract: an error is returned, nothing outside the
destination is written, the call terminates, and ASan/UBSan stay quiet.

---

## L2 — The DDI conformance harness

A unit test proves `GpuVirtualAddressSpace::map` is correct. It does not prove the driver
survives the sequence `dxgkrnl` actually performs, at the IRQLs it actually
uses, with the reentrancy it actually allows.

So `tests/harness/dxgkrnl/fake_dxgkrnl.cpp` is a fake `dxgkrnl`. It registers the
driver's DDI table - applying the checks `DpiInitializeInternal` applies - and
then drives the table through scripted scenarios: start the adapter, query
every `DXGKQAITYPE`, create a device, create allocations, create a context,
render, submit, complete a fence, preempt, time out, reset, stop, remove.
Each scenario arrives with the DDIs it calls. Since feature 006:

- `FakeDxgkrnl` applies the registration checks.
- `FakeDxgkrnlFdo` drives one adapter's entry points in `DpiAddDevice` and
  `DpiFdoStartAdapter` order - AddDevice then LinkDevice, StartDevice with the
  interrupt already connected, QueryChildRelations, child enumeration, the ISR
  at device IRQL with its DPC at DISPATCH, power, ResetDevice, StopDevice,
  RemoveDevice (stopping a started device first) - including dxgkrnl's
  unwinding when a step fails.
- `FakeDxgkrnlAdapter` is the `DXGKRNL_INTERFACE` callback table over
  `FakeVirtioGpuFunction`, QEMU's virtio-gpu-pci with its captured config
  space, resource list and register regions; mapping a BAR attaches the
  regions to the mock, unmapping detaches them, and a mapping left at the end
  fails the test. A `DXGK_INTERRUPT_DMA_FAULTED` notification fails the test
  and is not delivered: `VidSchDdiNotifyInterruptWorker` (dxgmms2.sys 26100
  @140031b80) bugchecks 0x119/0xD for every one on this GPU MMU adapter, which
  a live guest hit while this fake still accepted it.

- `FakeDxgadapter` is `DXGADAPTER::Initialize` (feature 007): the
  `DRIVERCAPS` query and the ones it gates (`PHYSICALADAPTERCAPS`,
  `FRAMEBUFFERSAVESIZE`, `DIRTYBITTRACKINGCAPS`, `64BITONLYCAPS`,
  `NATIVE_FENCE_CAPS`), run by `FakeDxgkrnlFdo` after QueryChildRelations, with
  one `CapabilityRule` per check dxgkrnl applies - so a capability claimed
  without the DDIs it needs fails start in a test, as it would in the guest.

- `FakeVidSch` is VidSch's side of one node (feature 009): render, patch,
  submit at DISPATCH, the DPC, and a check of every completion and preemption.
- `FakeVidMm` is VidMm's segment creation for a render adapter (feature 008),
  and `FakeHandleTable` dxgkrnl's allocation handles behind
  `DxgkCbGetHandleData`.

`tests/unit/scenarios/test_ddi_scenarios.cpp` runs the bring-up and teardown plus stop
twice, remove without stop, an interrupt before start, a failed start, reset at
bugcheck, two adapters, and the adapter ceiling - all through the registered
table, with no core class called directly.

This runs natively on Linux because the table is core's `DdiTable`, a mirror of
`DRIVER_INITIALIZATION_DATA` built from fixed-width types. The driver build
proves, per slot, that its layout is byte-for-byte the WDK's
(`src/km/ddi_table_layout.cpp`, generated), so the table the harness drives is
the table `dxgkrnl` reads. The structures the DDIs exchange are DDI records in
`src/core/ddi/` whose every field offset `src/km/ddi_record_layout.cpp` proves
the same way.

**The scenarios are derived from reverse engineering, not from guesswork.**
`dxgkrnl.sys` 10.0.26100.9444 is imported into Ghidra with 106k symbols in
`re/ghidra-proj`, rebuilt from the pinned `tools/re/binaries.txt` by
`tools/re/build_workspace.sh`. The real call order, the real
argument invariants, and the real IRQLs are readable there. `docs/ddi-sequences.md`
records each sequence with the function in `dxgkrnl` it was taken from, so a
scenario can be re-derived when it is questioned.

The harness also injects what a well-behaved `dxgkrnl` is allowed to do and a
naive driver forgets to handle:

- `QueryAdapterInfo` for a type the driver does not answer — must return
  `STATUS_NOT_IMPLEMENTED` and touch neither buffer.
  `DXGADAPTER::DdiQueryAdapterInfo` logs any status other than success,
  `STATUS_NOT_IMPLEMENTED`, `STATUS_INVALID_PARAMETER` or `STATUS_NO_MEMORY`
  as invalid, and `FakeDxgadapter` fails the test on one.
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
binary and connects them with an in-memory transport that mimics the real one.
`LoopbackRig` (`tests/harness/loopback/`) holds both sides:

- the guest's `MockPlatform`, whose contiguous blocks are the guest memory;
- the device core (`VirtioGpuRvk`) over a `MockHost` whose `map_guest` returns
  those blocks directly, running commands through a `MockExecutorOps` on real
  worker threads;
- control requests passed as bytes by `LoopbackDriver`, which can cut an answer
  short or rewrite one of its bytes on the way - a hostile device;
- a doorbell store (`LoopbackDoorbells`) that rings the worker's event, as a
  KVM ioeventfd would;
- an interrupt that is `MockHost` counting a raise on a fence line; the test
  waits for it and serves the context on its own thread, as the DPC would.

The thread that builds the rig is both the guest and the device's main loop.
The executor can be held, so a test keeps a worker busy for as long as it
needs a full ring.

What it covers today (M2's rvk contexts):

- capset read and judged; a version mismatch refused on both sides
- setup → submit → doorbell → execute → fence → complete, with a reply
- ring full, backpressure, and recovery once the head moves
- a context faulting beside a healthy one, torn down and set up again
- a device reset, after which a new context sets up
- a hostile guest: a tail skipped or rewound, a fence repeated, commands
  misframed
- a hostile device: malformed or refused answers at each step, a fence page
  moving backwards, completing an unsubmitted fence or clearing a fault, a head
  past the tail
- a driver that stops: `DRIVER_OK` cleared while a worker holds a batch, so the
  fence it raises afterwards reaches no notifier while the fence page still
  advances, and the next `DRIVER_OK` connects them again

### The full-stack rig: L2 and L4 joined

`LoopbackRig` stands in for the miniport, and `LoopbackKmt` (043) does the
miniport's job in a shim - it publishes blobs, sends `CTX_CREATE` and rings
doorbells itself. `FullStackRig` (`tests/harness/fullstack/`) removes that
shim: a Windows guest's whole path runs in one Linux process under ASan and
UBSan, from the in-guest code down to the executor.

```
src/guest (GuestSuite, GuestConnection, KmtTransport)   unchanged
  -> DdiKmt : Kmt            dxgkrnl's D3DKMT layer over the registered DDI table
  -> Adapter (src/core)      DriverEntry, AddDevice, StartDevice by FakeDxgkrnlFdo
  -> VirtioBridge            virtio-pci's device side: status, notify, split rings
  -> LoopbackDevice          VirtioGpuRvk over MockHost, MockExecutorOps, doorbells
  <- InterruptPump           fence raises and queue completions as ISR + DPC
```

- `RegisteredDriver` runs DriverEntry on the `MockPlatform` and has
  `FakeDxgkrnl` accept the table; its destructor unloads, which is when the
  platform's leak and violation checks run.
- The PCI function is `FakeVirtioGpuFunction` set up the way
  `tools/guest/run_guest_suite.py` starts QEMU: subsystem `1af4:5256`, the
  doorbell region on BAR 0, a hostmem region, 66 queues and eleven MSI-X
  messages. Its notify region is `VirtioBridge` and its doorbell pages are
  `LoopbackDoorbells`.
- `VirtioBridge` follows the driver's status into the device core as
  virtio-pci does (`set_status`, and a reset on 0), and on each notify takes
  every chain from the driver's ring in guest memory (`FakeVirtqueueDevice::
  answer_all`): a control request goes to `VirtioGpuRvk::control` first and to
  `BaseVirtioGpu` - what QEMU's base device answers for the 2D protocol, display
  info among it - when the core leaves it; a cursor request completes with
  nothing written. Answers are written at once, and the queue is left pending
  an interrupt.
- `InterruptPump` runs on the test thread between the calls a process makes:
  a DPC a DDI queued, a queue the bridge completed and every fence line a
  device worker raised, each delivered through `FakeDxgkrnlFdo::interrupt` with
  the MSI-X message the driver programmed for that queue. `MockPlatform` stays
  single-threaded: device workers touch only guest memory.
- `DdiKmt` makes the DDI calls dxgkrnl makes: CreateDevice; CreateAllocation
  with the descriptor as private data, a `FakeHandleTable` handle, OpenAllocation
  on the creating device and guest pages behind it; MakeResident as
  `BuildPagingBuffer` `MAP_APERTURE_SEGMENT` for what the driver placed in the
  aperture, and `UNMAP` before CloseAllocation and DestroyAllocation;
  CreateContext with virtual addressing and copied private data, then
  SetRootPageTable; SubmitCommandVirtual at PASSIVE with node 0's next fence and
  copied private data, refused past what the context asked for. `NodeFences`
  keeps what the driver's notifications make of each fence, and a wait pumps
  interrupts until the context's last fence completed; a faulted context's
  last fence completes too, with no reply behind it.
  A vertical blank turns the driver's vsync on and runs the platform's timers
  to the next one.

What it runs (`tests/loopback/test_fullstack_*.cpp`):

- the adapter starting on the real device core: capset, host-visible region
  blob, doorbell pages
- the 043 transport round trip through the real driver - `RvkContextTable`
  binding on doorbell 0, the echo `0x10001` and its reply - with the completion
  proven to arrive only through the fence interrupt
- a context the executor faults, whose wait completes with no reply and whose
  next submission the driver refuses, while its neighbour echoes
- D3 and D0, ResetFromTimeout and RestartFromTimeout, and StopDevice and
  StartDevice, each under a live transport: the context lost, the region blob
  made again, a new transport echoing
- 3000 batches wrapping the smallest ring; five transports interleaved on
  their own doorbells and messages, and a freed doorbell reused
- a process that exits holding everything, cleaned up to nothing
- `GuestSuite` unchanged: all 41 steps the guest run reports pass; the fence
  stress run for one second of mock time; and the timeout run, which fails at
  `timeout.held_lost` alone because the rig has no TDR

What it has found in the driver and the guest: the host-visible region's blob
was never made again after D3/D0 or a timeout reset, a second D0 closed a
working region, and `TransportRing` stored the ring tail before the miniport
checked the submission, so a device worker woken for its ring attach consumed
an unsubmitted batch and the driver failed the context - one run in thirty
under parallel load.

What it cannot see, and L5 still must:

- dxgkrnl's and VidMm's real behaviour - the rig's sequences are the ones
  `docs/ddi-sequences.md` records, not the kernel's; no TDR, no eviction or
  paging decisions of VidMm's own, no GPU page tables built, no
  `D3DKMTLock2` of a CPU-visible segment (the host-visible scenario locks guest
  pages, as the guest does while 041's segment is parked)
- time: the platform clock moves only where a wait moves it, so vsync proves
  the period the driver asks for and the stress run's worst wait is not a
  measurement
- Driver Verifier's checks, the kernel's IRQL, pool and DMA rules beyond what
  `MockPlatform` models, and real interrupt concurrency - an ISR here never
  preempts a DDI
- clang-cl's codegen for the driver and `rvktest.exe` (feature 037 found an
  argument-order bug only that compiler showed), and QEMU's glue itself

- the rvk scenarios on this machine's GPU (`test_fullstack_rvk.cpp`, 044):
  `FullStackRig(executor table, host GPU)` realizes the device core on the
  executor library's own root - `src/device/shim/executor_ops.cpp` linked into
  the test and `rvk_executor_ops()` handed over, not a dlopen - bound to the
  GPU the host's loaders say runs the most sub-APIs (`WidestHostGpu`). The
  generated clients encode in the guest code, the generated server decodes on
  a device worker, and the host's Vulkan, OpenCL and EGL run it: the Vulkan
  chain, vector add read back exact, fault isolation and the whole
  `rvktest.exe rvk` run pass, and the guest is shown one physical device and one
  OpenCL platform - the bound GPU's - on a host that has two GPUs and three
  platforms. A host without a scenario's sub-API returns without judging it
- the Vulkan probe through the ICD (`test_fullstack_vulkan_icd.cpp`, 045):
  `VulkanProbe` over `LoaderVulkan`, whose `vkGetInstanceProcAddr` is the bound
  `IcdRoot`'s as the DLL's export answers the loader, and that root over the
  rig's `DdiKmt`, so instance, adapter LUID, device, a compute dispatch and a
  draw read back through `vkMapMemory`, a forced context fault and a recreated
  device run through the real driver, device and executor. The executor is the
  library file opened `RTLD_LOCAL` (`ExecutorLibraryFile`), not the entry point
  linked in, because the ICD and the executor both define the generated handle
  hooks; the test runs with `VK_LOADER_DISABLE_DYNAMIC_LIBRARY_UNLOADING=1`, so a
  driver the host loader unloaded cannot leave a leak ASan reports from an
  unknown module

Still to come: a shared allocation opened by a second simulated process, and a
display path - a VidPn committed and a primary scanned out - through the rig.

Loopback tests are the cheapest place to find protocol design errors, and
protocol design errors are the expensive kind. Write the loopback test for a
protocol change before writing either side of it.

---

## L5 — In the guest

The smallest layer, and the only one that needs Windows.

- **Driver Verifier**, always on for `rvkgpu.sys` during development: special
  pool, IRQL checking, DMA verification, low-resource simulation. Not optional.
- **D3DKMT integration tests**: `rvktest.exe`, a user-mode program calling
  `D3DKMTCreateAllocation2`, `MakeResident`, `MapGpuVirtualAddress`,
  `SubmitCommand` and `WaitForSynchronizationObjectFromCpu` against the real
  driver. These mirror the L4 loopback tests, so a divergence points at the
  shim.
- **The application suite**: vkd3d-proton and DXVK samples, then real
  applications.
- **HLK / WHLK** graphics tests, only if the driver is ever to be signed for
  distribution. Out of scope until then, but the display tests are a useful
  correctness oracle even unsigned.

Guest runs are driven from Linux over the QEMU guest agent, with a snapshot
before every driver install, because a miniport that fails in `StartDevice`
can leave the guest with no display.

### As built

- **The suite** is `src/guest/`: `GuestSuite` runs its scenarios - the adapter
  (found by PCI identity `1af4:1050` subsystem `1af4:5256`, WDDM 3.2), an
  allocation and a refused hostile descriptor, a GPU mapping through a paging
  queue, a submission awaited through a CPU notification, the vertical
  blank (`VsyncScenario` waits for a fixed number of blanks on source 0 and
  judges the rate the performance counter measured against the mode's 60 Hz,
  within 5%), and the transport round trip (`TransportScenario` opens a
  `GuestConnection` on the adapter the search found, sets up one context's
  `KmtTransport` and sends the transport `NOP`, which the device answers itself
  with an empty reply whichever executor is loaded, and reports the round trip
  as `transport.latency`) - over the `Kmt` contract.
- **The rvk scenarios** (feature 044) are `rvktest.exe rvk`, `GuestSuite::run_rvk`:
  the transport no-op on Vulkan and on OpenGL (whose setup is the host making
  an EGL context on the display it found for the GPU at realize and kept, so the
  no-op waits for a context and never for a driver start-up), `VulkanScenario` (instance, physical device, device, buffer,
  memory, bind, destroy through the generated client), `OpenClScenario` (vector
  add of 64 floats read back and compared exactly) and `IsolationScenario` (a
  context whose malformed `vkCreateInstance` the host faults fails its sends
  while a healthy one keeps working and a new one sets up). They need the real
  executor library and a host GPU, so the runner asks for them with `--rvk
  --executor build-qemu/librvk_executor.so`, records the executor and the
  no-op's latency in the verdict and fails a no-op past 50 ms (SC-002; the first
  live runs measured 0.87 ms idle and 4.4 ms under load). The generated Vulkan
  and OpenCL clients build freestanding into the program for them. It is portable and held to the same gates as `src/core`: on Linux,
  `MockKmt` (`tests/harness/guest/`) scripts every call's failure, zero handles
  and hostile enumerations, at 100% branch coverage.
- **The Vulkan run** (feature 045) is `--vulkan`, a third boot on the real
  executor: the pinned Khronos loader and `vulkaninfo` from
  `third_party/vulkan-runtime/` (refused unless `tools/fetch_vulkan_runtime.py`
  put the pinned files there) and `rvkvkprobe.exe` beside them in `C:\rvk`. The
  runner fails a global `HKLM\SOFTWARE\Khronos\Vulkan\Drivers` value naming
  rvk, reads `vulkaninfo`'s full output (`--summary` prints no LUID) and requires
  exactly one `GPUn:` section whose `deviceLUID` LowPart is the one the probe
  printed as `vulkan.adapter_luid`, requires every probe step (`instance`,
  `adapter_luid`, `physical_device`, `device`, `compute`, `draw`, `lost`,
  `recreated`) and `rvkvkprobe: N passed, 0 failed`, and requires
  `vulkan.physical_device` to equal `vulkan.adapter_luid`. The collect stage
  copies the ICD's `C:\ProgramData\rvk\*.log` into `diag/` on every run.
- **The program** is `src/um/`: `GdiKmt` translates `Kmt` into gdi32's D3DKMT
  functions with every wait bounded by an event and a timeout, and
  `GuestProgram` writes one line per step, then `rvktest: P passed, F failed`,
  and exits with the failure count. `cmake/windows_user_toolchain.cmake` builds
  it with no C runtime; `guest_image` checks it imports only gdi32, kernel32
  and ntdll.
- **The package** is `src/km/rvkgpu.inf`, test-signed in the guest by
  `tools/guest/provision.ps1` with PowerShell's own certificate, catalog and
  Authenticode cmdlets. It carries the Vulkan ICD (`rvkvk.dll`, `rvkvk.json`)
  for every run, because the INF names them and pnputil refuses a package
  without a file it names.
- **The runner** is `tools/guest/run_guest_suite.py`. The snapshot is a qcow2
  overlay of the prepared image, created before QEMU starts and deleted after
  it stops; the image is only ever a backing file. It boots the overlay with
  `virtio-gpu-rvk-pci`, the agent channel, QMP and `isa-debugcon`; pushes the
  package, `rvktest.exe` and the script; signs, installs with Driver Verifier's
  standard checks on `rvkgpu.sys`, waits through each reboot on QMP's `RESET`;
  checks the adapter started on `rvkgpu`; runs the display scenarios below;
  runs the suite; collects the guest's diagnostics whatever happened; and
  powers off. Every wait names its step when
  it times out. Its tests run it against a fake QEMU with a scripted guest
  (`tests/tools/fake_guest/`), bugchecks and boot loops included.
- **The display scenarios** are what only a real desktop can answer, and each
  is a runner step with the host, not the guest, as the oracle:
  - `check`: how many monitors Windows has, and that each is parented to this
    adapter's device instance (`DEVPKEY_Device_Parent`), so a monitor belonging
    to some other display path cannot be mistaken for ours.
  - `display`: **the screendump**. QMP `screendump` writes a PPM per head from
    the host's scanout; the runner asserts each dump is the committed mode's
    size and counts its colours, and that it is not QEMU's "display output is
    not active" placeholder - which is exactly what an earlier 038 run mistook
    for a working scanout. It then reads the driver's own log for the rest: the
    primary's aperture pages were recorded, and no frame was refused for want of
    a backing. What a dump cannot prove yet is that the picture is a desktop:
    DWM composites with the GPU and nothing executes GPU work until features
    042-044, so the primary's pages are still zeros.
  - `modes`: `EnumDisplaySettings` lists at least the modes the driver offers,
    and the mode in force is one of them.
  - `setmode`: **the mode switch**. `ChangeDisplaySettings` to a second offered
    mode, then a screendump: the host's scanout is that mode's size if the guest
    took the switch, and the committed mode's if it refused it - a session 0
    service cannot move the console session's mode, so the step asserts whatever
    the guest actually did and never lets a refusal pass as a switch. A mode the
    driver never offered must be refused, and a refused switch must leave the
    host scanning out the size it already was.
  - `extend`: `SetDisplayConfig(SDC_TOPOLOGY_EXTEND)` with two outputs. A
    session 0 service cannot change the console session's topology - it answers
    ERROR_ACCESS_DENIED (5) - so the step asserts the refusal and the first
    display's scanout, and in-guest desktop extension waits for an interactive
    session.
  - `monitortimeout` and `power`: the policy's display power-off timeout is set
    to a minute, the runner polls the scanout until it goes dark, sends a key
    and a mouse move, and polls until it is lit at the same mode again. The
    system's idle timer needs no session, which is what makes display power
    testable where an in-guest mode switch is not.
- **The dumps are judged, not just kept.** A display TDR recovers: every step
  can pass while the driver has just been reset, and the only trace is a live
  kernel dump in `C:\Windows\LiveKernelReports`. The runner reads each pulled
  dump's `DUMP_HEADER64.BugCheckCode` and fails the run for any code but
  `VIDEO_DXGKRNL_BLACK_SCREEN_LIVEDUMP` (0x1A8) - which is expected until
  something renders into the primary, since dxgkrnl takes that dump when a
  source shows nothing. Two of feature 038's five consecutive runs left one.
- **The host probes** are the other thing only real hardware answers, and they
  need no guest at all: `tools/host/probe_host_memory.py --qemu build-qemu/qemu`
  starts one device per host GPU and prints what its feasibility probes found at
  realize - whether memory the GPU exports reaches a guest through the hostmem
  BAR, and whether the GPU can address guest pages directly (feature 039). A
  refused probe is an answer; the per-GPU results live in `docs/kb/04-memory.md`.
- **The power scenarios** are what a laptop does to a GPU every day
  (feature 040): `cycle` disables and enables the adapter `--power-cycles N`
  times inside one provisioning stage and reads Driver Verifier's pool tracking
  afterwards; `suspend` asks the guest what it offers before asking it to sleep
  (`powercfg /availablesleepstates`, the *available* half of it) and waits for
  QEMU's own SUSPEND event before waking, because waking a machine that has not
  reached S3 wakes nothing; `hibernate` turns hibernation on inside the overlay,
  suspends to disk, and boots the overlay again `--hibernate N` times, running
  the whole in-guest suite after each resume. S3 is disabled at the machine
  unless `--s3` says the firmware can resume from it - OVMF on q35 cannot, and
  the guest's own HAL bugchecks 0x5C when it tries.
- **The recovery scenario** is `tdr`, and a run asks for it by telling the host
  to misbehave: `--stall-ms N` holds every command of one context that long and
  `--fault-after N` loses the device on its Nth, both aimed at one executor with
  `--stall-context` (`RVK_ECHO_ONLY_NTH`), so the other contexts keep
  completing. The guest is then asked for `rvktest.exe tdr` - `TimeoutScenario`,
  two devices with a context each - and nothing else, because every other
  scenario would be measuring the injection. The run wants the guest's verdict,
  dxgkrnl's own event 4101 (the `timeouts` stage), and the live kernel report
  the reset leaves, which is the one live dump a run may leave besides the black
  screen. Until a context's batches reach the host executor (features 042 and
  043, live in 044) a held context holds nothing the guest waits on, so the
  guest half of this cannot time out yet; what the injection proves today is at
  L3 and L4.
- **The long run** is `--stress-minutes N`: the suite's `stress` scenario
  submits one fence at a time and waits for each for N minutes, waiting out a
  vertical blank between batches so the blank's interrupt is enabled and
  disabled hundreds of times over the run (dxgkrnl turns that source off when
  the last waiter leaves). The runner reads back the fences completed and the
  worst wait. It is not part of an ordinary run: a gate run asks for no stress
  and gets none.
- **The gate**: `tools/gate.sh` runs it when `AV_GUEST_IMAGE` names a prepared
  image, then boots the guest a second time with `--rvk` and the real executor
  (the suite's timeout scenario needs the echo library's stall, the rvk
  scenarios a host GPU), and says it skipped the guest otherwise. From feature 037 on a skipped
  guest is not a pass: no feature merges until the suite has run live, because
  every defect in that feature - a VidPn dxgkrnl would not commit, a segment set
  VidMm could not place, a handle callback WDDM 2.0 had replaced - was invisible
  to all four Linux configurations and visible in the guest immediately.

### Preparing an image

A Windows installation, done once by hand; nothing in the repository installs
Windows:

1. Windows 11 24H2 (build 26100) or later, x64, installed into a qcow2 on
   UEFI firmware without enrolled Secure Boot keys, on the q35 machine's
   AHCI (SATA) disk. BitLocker off. A libvirt domain's disk serves, once the
   domain is shut off: `virsh shutdown` is not enough while fast startup is on,
   so turn it off first (`powercfg /h off`) or shut down from the guest agent.
2. The QEMU guest agent from the virtio-win guest tools, installed and set to
   start automatically; PowerShell 5 or later (inbox).
3. With the image not in use by anything else:

   ```bash
   tools/guest/run_guest_suite.py --image /path/to/windows.qcow2 --prepare-image
   ```

   This is the one run that writes the image. It boots the image itself and
   runs `provision.ps1 -Stage prepare`: the test-signing certificate and test
   signing (one reboot), kernel memory dumps with automatic reboot, verbose
   `setupapi.dev.log`, and nothing that interrupts a run - no fast startup,
   hibernation, sleep, automatic updates, automatic maintenance or boot
   recovery environment, and a Defender exclusion for `C:\rvk`. It is
   idempotent; run it again after the image's Windows changes.
4. `AV_GUEST_IMAGE=/path/to/windows.qcow2 tools/guest/run_guest_suite.py`.

The image needs about 25 GB. It is never modified by a suite run, so one image
serves every run. Each run's overlay grows by the pagefile, the installed
driver and any kernel dump - a few GB at most - and is deleted when it ends.

### Debugging a run

Everything a run leaves is in its work directory (`build-guest/` by default):

| File | What it holds |
|---|---|
| `verdict.txt` | every step, `ok` or why it failed; bugchecks by name |
| `rvkgpu.log` | the driver's `KernelPlatform::log` lines from `isa-debugcon`, across every reboot of the run |
| `qemu.log` | QEMU and the device's own warnings, timestamped (`-msg timestamp=on`), guest errors, and any `--trace` events |
| `report.txt`, `device.txt` | `rvktest.exe`'s report; the device's status, problem code and verifier settings |
| `panic.txt` | each bugcheck: code, name, and its four parameters |
| `diag/` | pulled from the guest after the run: `system.txt`, `dxgkrnl-admin.txt`, `dxgkrnl-operational.txt`, `kernel-pnp.txt` (the last events of each log), `setupapi.dev.log`, `device.txt` (every device property), `verifier.txt` (`verifier /query`) |
| `diag/dumps/` | minidumps and `LiveKernelReports` (TDR live dumps); `MEMORY.DMP` only with `--memory-dump`, because a kernel dump copied through the agent outlasts the step timeout and its failure takes the minidumps with it |
| `symbols/` | the `rvkgpu.sys` and `rvkgpu.pdb` the run installed, to open the dumps with |

**Bugchecks.** QEMU runs Windows with `hv-crash`, so Windows reports its
bugcheck code and parameters through the Hyper-V crash MSRs and QEMU emits
`GUEST_PANICKED` - the runner names the bugcheck without reading a dump. The
guest is left running (`-action panic=none`) to write its kernel dump and
reboot; the runner then collects `MEMORY.DMP`. A driver that bugchecks on every
boot never lets the agent answer: the runner notices the second panic, kills
QEMU and boots the same overlay again without `virtio-gpu-rvk-pci`, so
`rvkgpu.sys` never loads, and pulls the dumps from that. Open them with WinDbg
and `symbols/` on any Windows machine: `!analyze -v`.

**Options for a debugging run:**

```bash
tools/guest/run_guest_suite.py --quick            # install, check and the suite: ~4 minutes, no display/power/sleep scenarios
tools/guest/run_guest_suite.py --hold             # on failure, keep the guest running until Enter
tools/guest/run_guest_suite.py --vnc 1            # the guest's display on 127.0.0.1:5901
tools/guest/run_guest_suite.py --gdb 1234         # QEMU's gdbstub: gdb -ex 'target remote :1234'
tools/guest/run_guest_suite.py --on-panic pause   # stop at the bugcheck instead, for --gdb (no dump)
tools/guest/run_guest_suite.py --memory-dump      # also pull C:\Windows\MEMORY.DMP, in base64 through the agent
tools/guest/run_guest_suite.py --trace 'virtio_gpu_*' --trace 'virtio_queue_*'   # QEMU trace events into qemu.log
```

A held guest is reached with `tools/guest/guest_shell.py`, over the agent
socket the runner lets go of:

```bash
tools/guest/guest_shell.py ps 'Get-PnpDevice -Class Display | Format-List *'
tools/guest/guest_shell.py exec 'C:\rvk\rvktest.exe'
tools/guest/guest_shell.py push build/driver/rvkgpu.sys 'C:\rvk\package\rvkgpu.sys'
tools/guest/guest_shell.py pull 'C:\Windows\INF\setupapi.dev.log' setupapi.dev.log
socat - UNIX-CONNECT:build-guest/qmp.sock    # QMP: {"execute":"qmp_capabilities"} then e.g. cont
```

**A bugcheck with no dump.** Windows sometimes cannot write one - "Crash dump
initialization failed" in `diag/system.txt`, or a panic so early that the crash
MSRs carry no parameters and `verdict.txt` says *panicked without bugcheck
parameters*. The code and its parameters are still in the running kernel, so
pause the guest and read them:

```bash
tools/guest/run_guest_suite.py --on-panic pause --gdb 1234 --hold
gdb -q -batch -ex 'target remote 127.0.0.1:1234' -x tools/guest/read_bugcheck.py
```

Find `ntoskrnl`'s base by walking pages down from any kernel `$rip` to the
`MZ` header, read `nt!KiBugCheckData` - five `ULONG64`: the code and its four
parameters - and walk `nt!PsLoadedModuleList` to attribute every address on the
faulting stack to a module. `re/guest-26200/publics.txt` holds the guest
kernel's public symbols and their section offsets.

Two things bite here:

- **`gdb` detaching resumes the guest.** End the script with
  `gdb.execute("monitor stop")`, or the guest finishes dying and reboots before
  the next read.
- **A `--gdb` run leaves QEMU holding the port.** The next run then fails with
  *gdbstub: couldn't create chardev*; kill the held QEMU first.

**Never map an address with a pinned binary.** `re/bin/` is pinned to the build
`docs/kb/` was derived from, and the guest's is usually a different one: a
`dxgmms2` offset read off the pinned copy landed mid-instruction and named the
wrong function. Take the faulting module's PDB GUID from its debug directory in
guest memory, fetch that PDB (`docs/kb/12-re-cookbook.md`), and map the RVA
against the section headers the PDB itself carries
(`llvm-pdbutil dump --section-headers --publics`). Disassemble the bytes read
out of guest memory, not the pinned file.

The application suites join with the drivers they test: Vulkan with the CTS,
vkd3d-proton and DXVK samples in feature 048, OpenGL and OpenCL conformance in
054, and Direct3D through the Microsoft runtime in 059.

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
and the citation says where to look. It happened once: the fake accepted
`DMA_FAULTED`, and dxgmms2.sys bugchecks 0x119/0xD on it (see
`docs/kb/05-scheduling.md`).
