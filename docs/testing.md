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

`avvk.dll`'s decisions live in `src/icd/vulkan/` and are tested the same way
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

The window-system extensions (feature 046) are tested in the same layers.
`SurfaceTable`, `SurfaceProperties`, `Swapchain` and `Presentation` are
portable, and what they call is injected: `EntryLookup` is the ICD's own entry
table in the driver (`IcdEntryLookup`) and a `ScriptedVulkanDevice` in a test -
a scripted GPU that hands out images, records the commands a present's copy
makes and holds the bytes a test paints, so a unit test judges the frame the
swapchain blitted; `WindowSurface` is `GdiWindowSurface` in the driver and
`ScriptedWindowSurface` in a test, which keeps every blit. `Swapchain`'s
creation is judged against `contracts/wsi.md` by a hostile table and by
`fuzz_swapchain_create`, whose input is a whole `VkSwapchainCreateInfoKHR` -
an application's structure is foreign bytes, so every enumeration field is read
as the integer it is (`foreign_enum.hpp`).

### The trace tracker in user mode

What decides in the tracker's Windows side is portable and tested like the
ICD (docs/trace.md): `InspectProgram` - `avinspect.exe`'s commands, the
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

### The trace tracker in the driver

`DriverTrace` and `DriverImage` run over `MockPlatform` and `MockTraceHost`:
the table laid out before the miniport, the address line logged, the bugcheck
routine registered and run through `MockPlatform::bugcheck`, whose lines
(`log_at_bugcheck`) a test reads apart from the ordinary log; an allocation
that fails, a registration the kernel refuses, a table overwritten before the
bugcheck. The lock marks are read back from a bound table while the lock is
being acquired (`MockPlatform::on_acquire`), and every `try_acquire` is
refused by `MockPlatform::hold_elsewhere(rank)`, which is also how each
table's report is shown writing only `busy=true`. `TraceEscape` is paged the
way `avinspect.exe` pages it and reassembled; `Adapter::escape` answers the
whole dump of a started adapter byte for byte; `fuzz_adapter_info` can give an
escape the trace magic.

`MockTraceHost` models the one thread that made it: every other thread gets no
key and so no slot. That keeps the full-stack rig's device workers - which
share the process, and so the driver's bound table - from racing its slots.

### The trace tracker in the device

`Worker`, `DeviceContext`, `DeviceContextTable`, `VirtioGpuAv` and
`DeviceModule` report over `MockHost`, each checked against the exact dump.
A worker's mutex held elsewhere (`MockHost::hold_mutex_elsewhere`) reports
`busy=true`, and `TableHost` is shown marking a bound table as its mutexes
are taken, tried and released.

The POSIX shims are tested for real in
`tests/unit/device/shim/test_device_trace.cpp`:
- `/dev/shm` files: made at mode 0600, and swept only for processes that do
  not exist;
- `dladdr1` names for libc and the test binary;
- the state socket, answered through a scheduler the test controls, with
  every refusal;
- the crash file: written once, with this thread's frames and native stack,
  chained to a counting `SIGBUS` handler, never written for a machine check,
  and an ignored action given back.

Mutating the machine-check guard or the chaining fails it.

`tests/unit/device/shim/test_thread_signal_stack.cpp` (with ASan's own SEGV
handling and signal stacks off, so they cannot stand in for the code under
test): a thread's first question to `PosixTraceHost` gives it a guarded
alternate stack of at least 64 KiB, a thread's own stack is kept, the mapping
is gone when the thread ends, and a forked process whose worker thread
overflows its stack inside traced frames dies of `SIGSEGV` leaving the crash
file with those frames and a native stack. Skipping the install fails it.
`tests/unit/device/shim/test_state_socket.cpp` serves a reporter's dump, a
source's refusal, an unknown request and a name too long for a socket path;
`tests/unit/device/executor/test_executor_state.cpp` checks `ExecutorRoot`'s
exact dump.

Live, `qemu_smoke` and `qemu_virtio_gpu` still pass with tracing started in
realize. A qtest QEMU answers `host_threads` and `host_state` through
`tools/mcp/av_inspect.py`. A `kill -SEGV` leaves a crash file whose native
stack runs from the handler through libc into `qemu-system-x86_64`, and the
process ends.

### The OpenGL ICD

`avgl.dll`'s decisions live in `src/icd/opengl/` (feature 050), tested the way
the Vulkan ICD's are: `GlIcdRoot` over the seams (`WindowSurface`,
`ThreadSlot`, the system opengl32 it creates HGLRCs through) with
`ScriptedWindowSurface`, `ScriptedThreadSlot` and `FakeOpengl32`, which loads
the ICD through its 15 exports and refuses it as `pgldrvLoadAndAllocDriverInfo`
does, installs a proc table per thread only for 336 or 306 entries, and names
an extension function (`wglGetProcAddress`) only to a thread with a context
current - the rule the first live boot tripped over. `GlIcdRig` runs a context
over MockKmt and a scripted host; `GlProbe` runs its nine steps
(`pixel_format`, `context`, `version`, `draw`, `share`, `busy`, `attribs`,
`lost`, `recreated`) over `ScriptedGl` in unit tests, over `FakeOpengl32` on
`FullStackRig` and the host GPU in `test_fullstack_opengl_icd.cpp`, and over the
system `opengl32.dll` in the guest as `avglprobe.exe`, which
`run_guest_suite.py --opengl` runs in the gate's real-executor boot. The GL and WGL
parsers that read application bytes have hostile tables and fuzz targets
(`fuzz_context_attributes`, `fuzz_pixel_format_query`).

### Render pictures

Every render check keeps a picture of what it judged, as a 24-bit BMP
(`BmpImage`, `ImageSink`): the Vulkan probe's `vulkan-draw` (64x64),
`vulkan-present` (the window the swapchain presented onto, read back) and
`vulkan-resized` (the same window after it grew to 96x96), and the OpenGL
probe's `opengl-draw`, `opengl-share` (the 2x2 shared texture) and
`opengl-recreated`. In the guest the probes write them beside themselves in
`C:\av`, and `run_guest_suite.py --vulkan` / `--opengl` pull them into
`<work>/images/` (a passing probe without its pictures fails the run). The
full-stack tests on the host GPU write them to `build/images/`. Both scenes are
a full-viewport triangle in solid.frag's colour, so a correct picture is one
flat colour, RGB (64, 128, 191).

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
  `TableHost` and the `av_device.h` entry points run against it too.

The glue itself is exercised by the `qemu` gate configuration: the patched
QEMU is built and `tests/qemu/qemu_smoke.py` reads the PCI function over qtest.

### The device under qtest

`tests/qemu/virtio_qtest.py` is a virtio-gpu driver that needs no guest: over
qtest it assigns the function's BARs, walks the virtio capabilities, negotiates
features, lays split virtqueues out in guest RAM and polls their used rings.
The `qemu_virtio_gpu` ctest runs `tests/qemu/test_*.py` with it against the
patched QEMU's stock `virtio-gpu-pci` and `virtio-gpu-av-pci` side by side, so
a behaviour the av device changes shows up as a difference from stock. Where
the driver already sends a request, the test sends the driver's own bytes:
`gpu_request_bytes` prints what `GpuRequestWriter` writes. Device features past
the base class extend this suite: `test_virtio_gpu_blobs.py` finds the
host-visible shared memory region among the capabilities and reads and writes
mapped blobs through BAR 4 with qtest's memory commands, and
`test_virtio_gpu_contexts.py` reads the capset, checking its GPU identity
against `vulkaninfo` through `tools/host/list_gpus.py`, and creates, sets up and
destroys contexts. `test_virtio_gpu_ring.py` writes rings as the guest does
(`av_context.py`), stores to the doorbell pages in BAR 0 and waits for head -
QEMU dispatches the ioeventfd itself under qtest - and checks that a hostile
ring stops only its own context. `test_virtio_gpu_fences.py` reads fence pages
and the MSI-X pending bits of masked vectors, and the ISR with MSI-X off, to see
the fence queues' interrupts arrive; it also drives the sequences a restarting
Windows display driver performs - MSI-X toggled while `DRIVER_OK` stands, a
driver that stops by clearing `DRIVER_OK` without a reset and is replaced by one
that toggles MSI-X, MSI-X enabled only after `DRIVER_OK`, and a sweep of toggles
that must leak no descriptor - because that sequence aborted QEMU on 035's first
live run. `test_virtio_gpu_execution.py` runs
batches through `av_echo_executor` (`tests/qemu/echo_executor.cpp`), an executor
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
valid input it feeds empty, every truncation, the valid input, a bit-flip sweep
over it, all-zero and all-ones. `GuardedBuffer` surrounds the destination with
poisoned bands so a write past the end is caught.

Each case asserts the same contract: an error is returned, nothing outside the
destination is written, the call terminates, and ASan/UBSan stay quiet.

Surviving is half the contract. `cases_run()` says how many inputs were fed and
`rejections()` how many the parser refused, and every sweep asserts a floor on
the second: the cases fed, less the acceptances the format itself explains - a
field it leaves free, a flip that lands on another value the format defines -
with the subtraction written out in a comment a reader can check against the
format. A decoder that stopped enforcing a magic, a reserved word or a range
then fails its own sweep, where before it would have passed by not crashing.
Where the accepted set cannot be derived from the format alone - a chain whose
framing moves when a length changes - the comment says so and the floor is the
weakest true one, rather than a number read off the implementation.

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

Since feature 053 it also runs the shared allocation: a create whose private
driver data asks for an identity, two devices, an open on each with the
*creating* device's own bytes - which is all dxgkrnl carries between two
processes - the close in either order, a device destroyed still holding it, and
a table of forged records (another adapter's number, an id this adapter never
minted, a kind or a size that disagrees, a private data with no record at all)
each refused with nothing opened, followed by the real record still opening.
That last line is what makes the refusals mean something rather than proving
only that an open can fail.

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

Targets, at minimum: the av command decoder, the submission descriptor, the
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
- the device core (`VirtioGpuAv`) over a `MockHost` whose `map_guest` returns
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

What it covers today (M2's av contexts):

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

### Recordings: the same run, read back and replayed

The device can write down what crossed the wire (`docs/protocol.md`, feature
065), and that turns a loopback run into two more tests, both in
`tests/loopback/test_loopback_recording.cpp`:

- `LoopbackRig(gpu, "loopback.av")` records into the `MockHost`'s memory, and
  the recording read back holds the blobs, the setup, every batch with the
  bytes the executor received, the replies and the fences, in order. That is
  the only place where what the device wrote down is compared against what the
  real guest core actually sent.
- the same recording replays into an executor library through
  `av_executor.h`, and every recorded reply is the answer the replay
  produced - a round trip with the wire in the middle.

A recording read back is bytes this side did not write, so `RecordingReader`
has a hostile-input table and `fuzz_recording_reader` at L3. The printer and
the replay are built on the reader and never frame a record themselves, but a
batch record's payload is the guest's own framed commands, which the reader
does not walk: `fuzz_recording_printer` drives the printer over arbitrary bytes
for that, with and without the payload hex and under a filter that reads every
command's name.

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
  -> LoopbackDevice          VirtioGpuAv over MockHost, MockExecutorOps, doorbells
  <- InterruptPump           fence raises and queue completions as ISR + DPC
```

- `RegisteredDriver` runs DriverEntry on the `MockPlatform` and has
  `FakeDxgkrnl` accept the table; its destructor unloads, which is when the
  platform's leak and violation checks run.
- The PCI function is `FakeVirtioGpuFunction` set up the way
  `tools/guest/run_guest_suite.py` starts QEMU: subsystem `1af4:5256`, the
  host GPU's vendor, device and revision (`1002:7550` rev `c0`, as the device
  presents a host GPU with a PCI function), the doorbell region on BAR 0, a
  hostmem region, 66 queues and eleven MSI-X messages. Its notify region is `VirtioBridge` and its doorbell pages are
  `LoopbackDoorbells`.
- `VirtioBridge` follows the driver's status into the device core as
  virtio-pci does (`set_status`, and a reset on 0), and on each notify takes
  every chain from the driver's ring in guest memory (`FakeVirtqueueDevice::
  answer_all`): a control request goes to `VirtioGpuAv::control` first and to
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
- Feature 049's doorbells, once a rig asks for them
  (`DdiKmt::allow_hardware_scheduling`, off by default as hardware-accelerated
  GPU scheduling is off on Windows until it is turned on): CreateHwQueue on the
  context with the progress fence dxgkrnl allocates, CreateDoorbell on that
  queue, and ConnectDoorbell for the device page the driver names. A wait then
  waits on both paths - the driver's fences for what it submitted, and the
  queue's progress against the value user mode queued for what it did not.
- `DoorbellFabric` is the bus between the two. The driver's own mapping of the
  doorbell BAR is a synthetic address only `MockPlatform` can reach, which no
  process can store to, so a doorbell hands out a page of real memory and the
  fabric carries what lands in it to where the driver's own store would have
  gone - or nowhere, while the doorbell is disconnected, which is what
  dxgkrnl's dummy page does on a machine. Nothing carries it by itself: like
  the rig's interrupts, a store arrives on the test thread between the calls a
  process makes, which is what makes "the store arrived late" or "not at all"
  a thing a test can write.

What it runs (`tests/loopback/test_fullstack_*.cpp`):

- the adapter starting on the real device core: capset, host-visible region
  blob, doorbell pages
- the 043 transport round trip through the real driver - `AvContextTable`
  binding on doorbell 0, the echo `0x10001` and its reply - with the completion
  proven to arrive only through the fence interrupt
- a context the executor faults, whose wait completes with no reply and whose
  next submission the driver refuses, while its neighbour echoes
- D3 and D0, ResetFromTimeout and RestartFromTimeout, and StopDevice and
  StartDevice, each under a live transport: the context lost, a new transport
  echoing
- 3000 batches wrapping the smallest ring; five transports interleaved on
  their own doorbells and messages, and a freed doorbell reused
- the doorbell path with the submission in the process's hands (feature 049,
  `test_fullstack_doorbell.cpp`): a store in user mode reaching the device
  core's worker with no `SubmitCommandVirtual` at all, a disconnect making the
  next store reach nothing, three rounds of echoes across a disconnect with
  none lost or repeated, and the hostile half - a tail past the ring, an
  arbitrary store value, a queued fence nobody submitted - each faulting only
  the context that did it
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
  `D3DKMTLock2` of a CPU-visible segment
- time: the platform clock moves only where a wait moves it, so vsync proves
  the period the driver asks for and the stress run's worst wait is not a
  measurement
- Driver Verifier's checks, the kernel's IRQL, pool and DMA rules beyond what
  `MockPlatform` models, and real interrupt concurrency - an ISR here never
  preempts a DDI
- clang-cl's codegen for the driver and `avtest.exe` (feature 037 found an
  argument-order bug only that compiler showed), and QEMU's glue itself

- the av scenarios on this machine's GPU (`test_fullstack_av.cpp`, 044):
  `FullStackRig(executor table, host GPU)` realizes the device core on the
  executor library's own root - `src/device/shim/executor_ops.cpp` linked into
  the test and `av_executor_ops()` handed over, not a dlopen - bound to the
  GPU the host's loaders say runs the most sub-APIs (`WidestHostGpu`). The
  generated clients encode in the guest code, the generated server decodes on
  a device worker, and the host's Vulkan, OpenCL and EGL run it: the Vulkan
  chain, vector add read back exact, fault isolation and the whole
  `avtest.exe av` run pass, and the guest is shown one physical device and one
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

- descriptor update templates through the whole stack
  (`test_fullstack_templates.cpp`, 047): the guest sizes a template's blob from
  what it remembers and the device refuses a length its own template does not
  account for, so only both halves together show that the two numbers agree
- two devices sharing memory and a semaphore through the Win32 external objects
  (`test_fullstack_external_objects.cpp`, 047): one device allocates exportable
  memory and signals, the other imports both by the handles the ICD answered,
  waits on the signal and copies the shared buffer into its own staging, which
  the guest reads back. Only this layer can show it, because a guest device is
  a host device and the sharing is between two host devices - what the host
  ever saw was `vkGetMemoryFdKHR` of the remembered allocation and
  `VkImportMemoryFdInfoKHR` naming a fresh fd of it. A host whose driver cannot
  share by fd makes no such device, and the test names which condition failed
  and returns
- one shared object named by two av contexts of one adapter
  (`test_fullstack_shared_objects.cpp`, 053): the miniport mints an identity
  into a carrier's private driver data, the creating context publishes it, an
  NT handle carries those bytes to the other context, and the device opens one
  host resource for it - one real descriptor with two holders, closed once, by
  the last of them. The publishing context is then torn down with the object
  still open, which is the claim US3 makes and the thing neither side can show
  alone: the miniport mints the id and knows nothing of descriptors, and the
  device holds the descriptor and knows nothing of allocations. The hostile
  half is here too - an id nobody published, a kind and a size that disagree, a
  publish under somebody else's id, and one context opening the same object
  twice - each answered with a status, the object untouched and both contexts
  still running

Still to come: a display path - a VidPn committed and a primary scanned out -
through the rig.

Loopback tests are the cheapest place to find protocol design errors, and
protocol design errors are the expensive kind. Write the loopback test for a
protocol change before writing either side of it.

---

## L5 — In the guest

The smallest layer, and the only one that needs Windows.

- **Driver Verifier**, always on for `avgpu.sys` during development: special
  pool, IRQL checking, DMA verification, low-resource simulation. Not optional.
- **D3DKMT integration tests**: `avtest.exe`, a user-mode program calling
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

### Every host GPU, named as the host names it

`run_guest_suite.py --all-gpus` starts one `virtio-gpu-av-pci` per host GPU
that is not a CPU device (`tools/host/list_gpus.py`; `--vulkaninfo FILE` reads
a capture instead of running `vulkaninfo`). Each device presents its GPU's PCI
vendor, device and revision (sysfs, through `VK_EXT_pci_bus_info`) with the
subsystem `1af4:5256` it always keeps, and names it in the capset: the Vulkan
`deviceName` without the driver's parenthesised suffix (docs/protocol.md). The
runner pushes `C:\av\adapters.txt` - vendor, device and name per GPU - and
the sign stage writes one INF model line per present
`PCI\VEN_xxxx&DEV_yyyy&SUBSYS_52561AF4`, named from that file, before the
catalog is made; avgpu.sys writes the same name to the device's DeviceDesc
and the `HardwareInformation` values at every StartDevice. After the check
stage (which now wants `outputs` monitors per device and every device
started), the `adapters` stage lists `Win32_VideoController` and DXGI's
`EnumAdapters1`, and the run fails unless each host GPU appears in both with
its name, `VEN_`/`DEV_`/`SUBSYS_52561AF4`/`REV_` and, for DXGI, VendorId,
DeviceId, SubSysId `0x52561af4` and Revision. AdapterRAM and
DedicatedVideoMemory are reported, not judged: DXGI counts the segments the
driver reports, which is the hostmem region, not the host GPU's memory.

### As built

- **The suite** is `src/guest/`: `GuestSuite` runs its scenarios - the adapter
  (found by its PCI subsystem `1af4:5256`, whatever vendor and device its host
  GPU gives it, WDDM 3.2), an
  allocation and a refused hostile descriptor, a GPU mapping through a paging
  queue, a submission awaited through a CPU notification, the vertical
  blank (`VsyncScenario` waits for a fixed number of blanks on source 0 and
  judges the rate the performance counter measured against the mode's 60 Hz,
  within 5%), and the transport round trip (`TransportScenario` opens a
  `GuestConnection` on the adapter the search found, sets up one context's
  `KmtTransport` and sends the transport `NOP`, which the device answers itself
  with an empty reply whichever executor is loaded, and reports the round trip
  as `transport.latency`) - over the `Kmt` contract.
- **The av scenarios** (feature 044) are `avtest.exe av`, `GuestSuite::run_av`:
  the transport no-op on Vulkan and on OpenGL (whose setup is the host making
  an EGL context on the display it found for the GPU at realize and kept, so the
  no-op waits for a context and never for a driver start-up), `VulkanScenario` (instance, physical device, device, buffer,
  memory, bind, destroy through the generated client), `OpenClScenario` (vector
  add of 64 floats read back and compared exactly) and `IsolationScenario` (a
  context whose malformed `vkCreateInstance` the host faults fails its sends
  while a healthy one keeps working and a new one sets up). They need the real
  executor library and a host GPU, so the runner asks for them with `--av
  --executor build-qemu/libav_executor.so`, records the executor and the
  no-op's latency in the verdict and fails a no-op past 50 ms (SC-002; the first
  live runs measured 0.87 ms idle and 4.4 ms under load). The generated Vulkan
  and OpenCL clients build freestanding into the program for them. It is portable and held to the same gates as `src/core`: on Linux,
  `MockKmt` (`tests/harness/guest/`) scripts every call's failure, zero handles
  and hostile enumerations, at 100% branch coverage.
- **The Vulkan run** (feature 045) is `--vulkan`, a third boot on the real
  executor: the pinned Khronos loader and `vulkaninfo` from
  `third_party/vulkan-runtime/` (refused unless `tools/fetch_vulkan_runtime.py`
  put the pinned files there) and `avvkprobe.exe` beside them in `C:\av`. The
  runner fails a global `HKLM\SOFTWARE\Khronos\Vulkan\Drivers` value naming
  av, reads `vulkaninfo`'s full output (`--summary` prints no LUID) and requires
  exactly one `GPUn:` section whose `deviceLUID` LowPart is the one the probe
  printed as `vulkan.adapter_luid`, requires every probe step (`instance`,
  `adapter_luid`, `physical_device`, `device`, `compute`, `draw`, `lost`,
  `recreated`, and feature 046's `surface`, `present`, `pacing` and `resized`)
  and `avvkprobe: N passed, 0 failed`, and requires
  `vulkan.physical_device` to equal `vulkan.adapter_luid`. The collect stage
  copies the ICD's `C:\ProgramData\av\*.log` into `diag/` on every run.

  The probe runs in the **console session**, not in the agent's
  (`ConsoleProgram`): the guest agent is a service, so what it starts lives on
  the window station `Service-0x0-3e7$`, where a window never becomes visible -
  `ShowWindow` leaves `IsWindowVisible` false, `BitBlt` of the window's device
  context fails with `ERROR_INVALID_HANDLE` and `PrintWindow` fails too, all
  measured in the guest for feature 046. A present the probe judges by reading
  its own window back therefore needs the session a person is logged into,
  which a scheduled task created with `/it` runs in; the runner writes a
  one-line `console.cmd` for the redirection (schtasks parses `/tr` itself and
  refuses a `>` in it), runs the task, waits for it to leave `Running`, takes
  the program's exit code from `Last Result`, pulls `C:\av\vulkan.txt` and
  deletes the task. A machine no one is logged into fails the run.
- **The D3D run** (feature 047) is `--d3d`, a boot on the real executor with
  the two translation layers beside the probes: DXVK's `d3d9.dll`, `d3d11.dll`
  and `dxgi.dll` and vkd3d-proton's `d3d12.dll` and `d3d12core.dll` from
  `third_party/dxvk/` and `third_party/vkd3d-proton/`, the pinned Khronos
  loader with them, and `avd3d9probe.exe`, `avd3d11probe.exe` and
  `avd3d12probe.exe` in `C:\av`. Windows searches a program's own directory
  first, so that placement is what makes the run the layer's rather than the
  system's. Each probe makes a device, clears a frame to one colour, reads it
  back, judges every pixel and - where the API has one - presents; the runner
  requires every step of that API's list and `avd3dNprobe: N passed, 0
  failed`, and pulls the three bitmaps into `images/`. The D3D9 and D3D11
  probes make a window, so all three run in the console session through
  `ConsoleProgram`, as the Vulkan probe does.

  A **cleared frame, not a triangle**: a triangle through either layer needs
  DXBC and no shader compiler is pinned here. What the clear still proves is
  the device, the swap chain or render target, the command list and fence, the
  read-back and the present - everything a layer has to get right before it
  draws anything.

  The layers are refused before anything boots unless
  `tools/fetch_layers.py <layer>` put the pinned files on disk, and the pins in
  `tools/dxvk_pins.txt` and `tools/vkd3d_proton_pins.txt` are **empty on
  purpose**: they are two URLs and a hash per file that no run here has ever
  downloaded, and a hash written from memory is indistinguishable from a
  tampered archive. Whoever first runs the fetch with a network fills them in.
  Until then `--d3d` says which fetch to run, and
  `tools/check_layer_requirements.py` says the layers' required extensions
  could not be checked rather than passing quietly.

  `--page-heap` (FR-006) adds full page heap to each probe - the
  `GlobalFlag`/`PageHeapFlags` values under Image File Execution Options that
  `gflags /p /enable` writes, since the debugging tools are not in the image -
  runs the three probes under it, takes the flag off again whatever happened,
  and then reports every heap, verifier or `0xc0000374` event in the guest's
  Application log. A run that found one names it and fails; a run that found
  none says so.
- **The conformance run** (feature 048) is `--cts`, a boot of its own on the
  real executor: the pinned Vulkan CTS from `third_party/vulkan-cts/` and the
  Khronos loader beside it in `C:\av\cts`. The run asks the guest's own build
  what cases there are (`--deqp-runmode=stdout-caselist`), keeps the ones
  `tools/cts_subset.txt`'s patterns name, and runs them a shard at a time - a
  shard is one step, so a shard that runs past `--step-timeout` fails the run
  rather than reporting. Each shard's `TestResults.qpa` is pulled before the
  next starts, and a shard whose log ends *inside* a case is picked up from the
  case after it: a crash, a TDR or a bugcheck leaves the log open at the case
  that caused it, which is how the run knows both what died and where to go on.
  The case that died is recorded as `Crash`, never dropped.

  **The verdict is a comparison, never a count.** `tools/compare_cts.py` reads
  the guest's log and a log of the same subset run natively on the host GPU:
  a case that passes on the host and does not in the guest fails the run and is
  named; one that fails on both is recorded and does not. `NotSupported` on
  both sides is a match - neither side has the extension - and on the host
  alone it means the two runs were not the same subset, which is exit 2 rather
  than a pass. `tools/check_cts_ratchet.py` is the half that needs no host run:
  `tools/cts_ratchet.txt` is what the guest passed, committed, so a change that
  loses a case fails the day it is made, and a gain is printed with the line to
  commit.

  `tools/check_cts_subset.py` is a ctest: every extension the ICD's policy
  table reports must have a group in the subset (FR-005), and every group must
  be for something the ICD reports. Constitution VI says nothing is reported
  that is not implemented; this says nothing is reported that is not measured.

  **What is not here.** `tools/cts_pins.txt` is empty on purpose - the CTS has
  never been downloaded on this machine and a hash written from memory pins a
  guess - so `--cts` refuses before it boots anything, naming the fetch, and
  the gate says it skipped. Everything that decides a verdict is built and
  tested over `.qpa` logs the tests write, including the logs that matter most:
  the ones a run left behind when it died.

- **Frames** (feature 048, US2) are `--frames`: after an application run the
  device's **scanout** is captured with QMP `screendump` - not a guest-side
  screenshot, which would go through the guest's own composition, the thing
  under test - and compared with the frame committed in
  `tests/references/vulkan/`, within a per-channel step a GPU
  may round by. A mismatch keeps the capture, the reference and a difference
  image. No reference is committed yet, and the run says so and keeps the
  capture rather than claiming a match: nothing the guest presents reaches the
  scanout until features 049 and 055-057.

- **The program** is `src/um/`: `GdiKmt` translates `Kmt` into gdi32's D3DKMT
  functions with every wait bounded by an event and a timeout, and
  `GuestProgram` writes one line per step, then `avtest: P passed, F failed`,
  and exits with the failure count. `cmake/windows_user_toolchain.cmake` builds
  it with no C runtime; `guest_image` checks it imports only gdi32, kernel32
  and ntdll.
- **The package** is `src/km/avgpu.inf`, test-signed in the guest by
  `tools/guest/provision.ps1` with PowerShell's own certificate, catalog and
  Authenticode cmdlets. It carries the Vulkan ICD (`avvk.dll`, `avvk.json`)
  for every run, because the INF names them and pnputil refuses a package
  without a file it names.
- **The runner** is `tools/guest/run_guest_suite.py`. The snapshot is a qcow2
  overlay of the prepared image, created before QEMU starts and deleted after
  it stops; the image is only ever a backing file. It boots the overlay with
  `virtio-gpu-av-pci`, the agent channel, QMP and `isa-debugcon`; pushes the
  package, `avtest.exe` and the script; signs, installs with Driver Verifier's
  standard checks on `avgpu.sys`, waits through each reboot on QMP's `RESET`;
  checks the adapter started on `avgpu`; runs the display scenarios below;
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
    the primary's pages are still zeros, and feature 046 measured why - with no
    D3D user-mode driver DWM composes with WARP and never presents through
    `DxgkDdiPresent`, which a boot calls twice with no allocations at all
    (docs/kb/07-usermode.md). A desktop on the scanout waits for guest GPU work
    to execute on the host (features 049 and 055-057).
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
  `--stall-context` (`AV_ECHO_ONLY_NTH`), so the other contexts keep
  completing. The guest is then asked for `avtest.exe tdr` - `TimeoutScenario`,
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
  image, then boots the guest a second time on the real executor with
  `--av --vulkan --opengl` (the suite's timeout scenario needs the echo
  library's stall, the others a host GPU); the three run in turn in that one
  boot, each whatever an earlier one answered, since each is a process of its
  own. It says it skipped the guest otherwise. `AV_GATE_JOBS` sets the gate's
  parallelism for a host short of memory. From feature 037 on a skipped
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
   recovery environment, and a Defender exclusion for `C:\av`. It is
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
| `avgpu.log` | the driver's `KernelPlatform::log` lines from `isa-debugcon`, across every reboot of the run |
| `qemu.log` | QEMU and the device's own warnings, timestamped (`-msg timestamp=on`), guest errors, and any `--trace` events |
| `report.txt`, `device.txt` | `avtest.exe`'s report; the device's status, problem code and verifier settings |
| `panic.txt` | each bugcheck: code, name, and its four parameters |
| `diag/` | pulled from the guest after the run: `system.txt`, `dxgkrnl-admin.txt`, `dxgkrnl-operational.txt`, `kernel-pnp.txt` (the last events of each log), `setupapi.dev.log`, `device.txt` (every device property), `verifier.txt` (`verifier /query`) |
| `diag/dumps/` | minidumps and `LiveKernelReports` (TDR live dumps); `MEMORY.DMP` only with `--memory-dump`, because a kernel dump copied through the agent outlasts the step timeout and its failure takes the minidumps with it |
| `symbols/` | the `avgpu.sys` and `avgpu.pdb` the run installed, to open the dumps with |

**Bugchecks.** QEMU runs Windows with `hv-crash`, so Windows reports its
bugcheck code and parameters through the Hyper-V crash MSRs and QEMU emits
`GUEST_PANICKED` - the runner names the bugcheck without reading a dump. The
guest is left running (`-action panic=none`) to write its kernel dump and
reboot; the runner then collects `MEMORY.DMP`. A driver that bugchecks on every
boot never lets the agent answer: the runner notices the second panic, kills
QEMU and boots the same overlay again without `virtio-gpu-av-pci`, so
`avgpu.sys` never loads, and pulls the dumps from that. Open them with WinDbg
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

**The display the run gives QEMU decides what the device can do.** A run uses
`-vga none -display none`, and a console with no display backend has no GL
context - it is the backend that calls `qemu_console_set_display_gl_ctx`, not
the device - so `qemu_console_has_gl` is false and the device offers no dma-buf
scanout at all (feature 058, `docs/kb/06-display.md`). `-display egl-headless`
is what gives it one. It needs QEMU built with GBM; without it
`egl_scanout_dmabuf` is compiled out and a scanout the device hands over is
dropped in silence.

What it does **not** give is a picture: `qmp_screendump` asks
`qemu_console_surface`, which answers nothing while the console's scanout is a
dma-buf (`ui/console.c`), so every screendump of such a console - ours or stock
virtio-gpu-gl's - answers "no surface". A run with the flag therefore checks
the device's own account of what its heads show instead of a PPM, and the
display scenarios that read a PPM run on the default `-display none`, where the
guest keeps the 2D path.

A held guest is reached with `tools/guest/guest_shell.py`, over the agent
socket the runner lets go of:

```bash
tools/guest/guest_shell.py ps 'Get-PnpDevice -Class Display | Format-List *'
tools/guest/guest_shell.py exec 'C:\av\avtest.exe'
tools/guest/guest_shell.py push build/driver/avgpu.sys 'C:\av\package\avgpu.sys'
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

## When the host GPU stops importing guest memory

Feature 047's host-visible import hands the host GPU a pointer into guest
memory and asks it to make a `VkDeviceMemory` over it, through
`VK_EXT_external_memory_host`. That guest memory is a **memfd mapped
`MAP_SHARED`** — in the L4 rig (`MockHost`) and in real QEMU alike, because
that is what a guest's RAM is.

On kernel 7.2.6 `amdgpu` refuses to pin a userptr whose pages are file-backed.
`vkGetMemoryHostPointerPropertiesEXT` still answers a memory-type mask, so the
pointer looks importable; `vkAllocateMemory` then fails with
`VK_ERROR_INVALID_EXTERNAL_HANDLE` (-1000072003). The guest sees blob import
status 5, `executor_refused`, and every scenario that imports fails.

`tools/re/probe_host_pointer_import.c` is the ten-second check, and it needs
none of this repository:

```
clang -O0 -D_GNU_SOURCE -I third_party/khronos/include \
      tools/re/probe_host_pointer_import.c -lvulkan -o /tmp/probe && /tmp/probe
```

It imports anonymous memory at four sizes and memfd-backed shared memory at
one. If the anonymous imports succeed and the memfd one answers
-1000072003, the host kernel is refusing file-backed userptr and the four
import tests cannot pass on it — `test_fullstack_av`,
`test_fullstack_vulkan_icd`, `test_fullstack_external_objects` and
`test_opengl_backend_device`. Nothing in the driver is wrong; the capability
the feature is built on is not there.

Two things this rules out, both of which looked likely first:

- **It is not the Mesa version.** The same Mesa passes when the memory is
  anonymous.
- **It is not memlock.** The import works at 8 MiB against a `RLIMIT_MEMLOCK`
  of 8 MiB when the pages are anonymous.

A kernel that has this behaviour is reported by `uname -r`; the one before it
did not, which is why a checkout that was green before a reboot can fail
after one with nothing changed in it.

### What the device does about it

It hands the same pages over as a **dma-buf** instead, which such a driver
takes. `BlobDmaBuf` turns a blob's extents into one descriptor through
udmabuf (`/dev/udmabuf`, `UDMABUF_CREATE_LIST`), so the host GPU sees one
contiguous range and no byte is copied — the same thing `GuestPageAlias` does
for a pointer, one layer down. The guest RAM memfd must be sealed against
shrinking; udmabuf takes no other kind, and `MockHost::add_guest_ram` seals
its own for the same reason.

Which of the two a device uses is **one decision per device, not one per
blob**: a buffer or an image is made for a single external handle type, and
the memory types the two kinds land in are not the same either - on this host
a userptr reaches 0x20 and a dma-buf 0x20, on another they differ. So
`VulkanBackend` measures both when the guest first asks anything about
importing, `ImportableTypes` holds the kind that won along with that kind's
mask, and everything downstream reads it: the memory types the guest is shown,
the handle type named on every buffer and image, and the kind `adopt` will
take. A guest that names no `VkExternalMemoryBufferCreateInfo` of its own gets
the right one; a guest that names one is not second-guessed.

The device still offers both kinds through `import_kinds` and tries the
dma-buf first, because a library that takes no descriptor - OpenCL's, whose
import is `CL_MEM_USE_HOST_PTR` - and a host with no udmabuf both have to keep
working. Both attempts are one import and carry one blob id: the refused one
left the library holding nothing.

A host where this matters can be told apart in one line:

```
ls /dev/udmabuf && vulkaninfo 2>/dev/null | grep -c VK_EXT_external_memory_dma_buf
```
