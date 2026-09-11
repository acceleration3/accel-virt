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
the fence queues' interrupts arrive. `test_virtio_gpu_execution.py` runs
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
  fails the test.

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

Still to come with the DDI wiring (M3): allocate → map GPU VA → submit through
dxgkrnl's path, and a shared allocation opened by a second simulated process.

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

- **The suite** is `src/guest/`: `GuestSuite` runs four scenarios - the adapter
  (found by PCI identity `1af4:1050` subsystem `1af4:5256`, WDDM 3.2), an
  allocation and a refused hostile descriptor, a GPU mapping through a paging
  queue, and a submission awaited through a CPU notification - over the `Kmt`
  contract. It is portable and held to the same gates as `src/core`: on Linux,
  `MockKmt` (`tests/harness/guest/`) scripts every call's failure, zero handles
  and hostile enumerations, at 100% branch coverage.
- **The program** is `src/um/`: `GdiKmt` translates `Kmt` into gdi32's D3DKMT
  functions with every wait bounded by an event and a timeout, and
  `GuestProgram` writes one line per step, then `rvktest: P passed, F failed`,
  and exits with the failure count. `cmake/windows_user_toolchain.cmake` builds
  it with no C runtime; `guest_image` checks it imports only gdi32, kernel32
  and ntdll.
- **The package** is `src/km/rvkgpu.inf`, test-signed in the guest by
  `tools/guest/provision.ps1` with PowerShell's own certificate, catalog and
  Authenticode cmdlets.
- **The runner** is `tools/guest/run_guest_suite.py`. The snapshot is a qcow2
  overlay of the prepared image, created before QEMU starts and deleted after
  it stops; the image is only ever a backing file. It boots the overlay with
  `virtio-gpu-rvk-pci`, the agent channel, QMP and `isa-debugcon`; pushes the
  package, `rvktest.exe` and the script; signs, installs with Driver Verifier's
  standard checks on `rvkgpu.sys`, waits through each reboot on QMP's `RESET`;
  checks the adapter started on `rvkgpu`; runs the suite; collects the guest's
  diagnostics whatever happened; and powers off. Every wait names its step when
  it times out. Its tests run it against a fake QEMU with a scripted guest
  (`tests/tools/fake_guest/`), bugchecks and boot loops included.
- **The gate**: `tools/gate.sh` runs it when `AV_GUEST_IMAGE` names a prepared
  image, and says it skipped the guest otherwise.

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
| `diag/dumps/` | minidumps, `LiveKernelReports` (TDR live dumps), and after a bugcheck `MEMORY.DMP` |
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
tools/guest/run_guest_suite.py --hold             # on failure, keep the guest running until Enter
tools/guest/run_guest_suite.py --vnc 1            # the guest's display on 127.0.0.1:5901
tools/guest/run_guest_suite.py --gdb 1234         # QEMU's gdbstub: gdb -ex 'target remote :1234'
tools/guest/run_guest_suite.py --on-panic pause   # stop at the bugcheck instead, for --gdb (no dump)
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
and the citation says where to look.
