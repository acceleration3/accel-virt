# Feature Specification: QEMU device: fence interrupts that follow the driver's lifetime

**Feature Branch**: `036-device-interrupt-lifetime`

**Created**: 2026-09-11

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 036 · **Status**: `implement` · **Depends on**: 031, 035

The device's fence interrupts are connected while the guest driver holds
DRIVER_OK and released when it clears it or resets the device, and a guest
that disables and re-enables MSI-X - as Windows does whenever it restarts a
display driver - can no longer abort QEMU with a duplicate irqfd. Found by the
first live run of the in-guest suite.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### Design rules — binding on this feature

Constitution Principle III and [`CLAUDE.md`](../../CLAUDE.md). Restated here because it is checked at review, not assumed:

- **Every unit of behaviour is a class that owns state.** No static member functions. No stateless class used as a namespace. No free function outside the two ABI boundaries (`DxgkDdi*` entry points in `src/core/ddi/`, and `extern "C"` entry points), each one line with no logic.
- If functions thread the same handle, buffer, bounds or platform through their parameters, **that parameter is a member**. Make it a class and inject its dependencies at construction.
- Name the classes this feature adds before writing them, and say which kind each is: an **object class** (one instance per real thing) or a **system class** (a long-lived service owning a collection).
- Dependencies injected at construction. No global, no singleton, no file-scope state.
- Data members private, trailing underscore. Classes `PascalCase`, methods and locals `snake_case`. No `Manager`, `Helper`, `Utils`, `Handler`, `Impl`, `process()`, `handle()`.
- One class per header, named for it. A header declares; it does not define. One responsibility per class, statable without an "and".
- Errors are `av::Status` values. No exceptions, no RTTI, no C++ standard library, no `new`/`delete` (placement new only in `ObjectStorage`). Cleanup is RAII via `ScopeExit`, never repeated per branch.
- Fallible construction is two-phase: trivial constructor, `initialize(...)` returning `Status`.
- Every public method declares its IRQL ceiling via `platform_.require_irql_at_most(...)`.

### Classes this feature introduces

<!-- Fill this in during /speckit-plan, before writing code. One row per class; if a row has no state, it is not a class. -->

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `DriverStatus` | object | The last status byte the guest wrote; the `FenceInterrupts` it drives; its `Host` | Matches the fence interrupts to the driver's status |

No class models an MSI-X vector, mask or pending bit: those belong to QEMU's
`msix.c` and `virtio-pci.c`, which FR-006 forbids patching. `FenceInterrupts`
keeps owning the connection and the lock that holds workers apart from it.

### Test requirements

Constitution Principle III applies in full. Concretely, this feature is not done until:

- every branch of every function it adds to `src/core`, `src/device` or `src/proto` is covered, or marked `AV_UNTESTABLE` with a justification;
- each stateful module it adds exports an invariant function that tests call after **every** mutation;
- every function it adds that reads bytes from outside its own address space has a hostile-input table test and a libFuzzer target with a committed corpus;
- the allocation-failure sweep passes: with each allocation site failed in turn, no crash and no leak;
- IRQL annotations are present and the mock's IRQL checker is satisfied;
- `tools/check_symbol_tests.py` reports no untested exported symbol.

### Reference material

Knowledge base:

- [`docs/kb/05-scheduling.md`](../../docs/kb/05-scheduling.md)
- [`docs/kb/11-implementation-plan.md`](../../docs/kb/11-implementation-plan.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## User Scenarios & Testing *(mandatory)*

Users: the host operator, whose QEMU process no guest may be able to abort,
and the driver's developers, whose every L5 run reinstalls a display driver.
The first live run of the in-guest suite (feature 035) aborted QEMU with
`msix.c:122: msix_fire_vector_notifier: Assertion 'ret >= 0' failed` while
`pnputil` replaced `viogpudo` with `rvkgpu`. The trace showed the sequence:
`viogpudo` set `DRIVER_OK` with MSI-X enabled, the device connected its fence
interrupts and KVM routes were added for vectors 0-2; stopping `viogpudo`
cleared `DRIVER_OK` (status 11) without a reset, so nothing was released;
`rvkgpu` then disabled and re-enabled MSI-X, `msix_write_config` fired the
vector-use notifier again, virtio-pci registered the same eventfd a second
time, KVM refused it, and QEMU asserted.

### User Story 1 - A guest restarting its display driver cannot abort QEMU (Priority: P1)

**Independent Test**: L1 table tests of the device core over `MockHost` drive
every status and MSI-X sequence below, asserting connect and disconnect counts
and invariants after each step; the L5 runner's install stage (`viogpudo`
replaced by `rvkgpu`) completes with QEMU alive and no MSI-X assertion in
`qemu.log`.

**Acceptance Scenarios**:

1. **Given** `DRIVER_OK` set, MSI-X enabled and fence interrupts connected,
   **When** the guest clears MSI-X enable and sets it again, **Then** QEMU keeps
   running, each fence queue's eventfd is registered with KVM at most once, and
   the next fence interrupt arrives on its vector.
2. **Given** `DRIVER_OK` set, **When** the guest writes a status without
   `DRIVER_OK` (for example 11), **Then** the fence interrupts are disconnected
   and a fence raised by a worker afterwards is dropped, not injected.
3. **Given** connected fence interrupts, **When** the guest resets the device
   (status 0), **Then** they are disconnected exactly once.
4. **Given** a disconnected device, **When** the guest sets `DRIVER_OK` again,
   **Then** the interrupts are connected once, on the vectors programmed at that
   moment.

### User Story 2 - Masking and vector programming are honoured (Priority: P1)

**Independent Test**: L1 tests of the pending and mask state; an L4 loopback
scenario raising fences while the guest side masks and unmasks.

**Acceptance Scenarios**:

1. **Given** a masked vector (per-vector mask bit or function mask), **When** a
   worker raises a fence interrupt, **Then** no MSI is injected, the pending bit
   is set, and unmasking delivers it exactly once.
2. **Given** `DRIVER_OK` set, **When** the guest moves a fence queue to another
   vector, **Then** later interrupts arrive on the new vector only.
3. **Given** a fence queue left at `VIRTIO_MSI_NO_VECTOR` - the driver asks for
   only three messages - **When** a fence is raised, **Then** it is dropped and
   logged once, and fence progress remains readable from the fence page.
4. **Given** MSI-X enabled only after `DRIVER_OK`, **Then** fence interrupts are
   still delivered.

### User Story 3 - Resource exhaustion is reported, not fatal (Priority: P2)

**Independent Test**: L1 tests with `MockHost` refusing the connection.

**Acceptance Scenarios**:

1. **Given** the host cannot provide an interrupt route, **When** the guest sets
   `DRIVER_OK`, **Then** the device logs the refusal once with its value, keeps
   serving the control queue, and refuses rvk context creation with a status
   the driver reports, instead of aborting.

### Edge Cases

- A guest that never sets `DRIVER_OK`: nothing is connected; contexts that need
  fence interrupts are refused; the 2D path works.
- A guest toggling MSI-X enable or function mask thousands of times per second:
  no growth in KVM routes or open eventfds.
- A worker raising a fence while the main loop disconnects: no use after free,
  no injection after the disconnect returns.
- A vector index beyond the vectors the device allocated.
- The device unrealized, or the VM reset, while connected.
- Several `virtio-gpu-rvk-pci` devices in one VM, each toggled independently.
- No in-kernel irqchip (TCG, qtest): virtio-pci delivers from the main loop and
  the same sequences hold.

## Requirements *(mandatory)*

- **FR-001**: No sequence of guest writes to the virtio status, common
  configuration, MSI-X capability or MSI-X table aborts QEMU or leaks an
  eventfd or interrupt route. Guest writes are hostile input (constitution V).
- **FR-002**: Fence interrupts are connected if and only if `DRIVER_OK` is set
  and no reset has followed it.
- **FR-003**: At any moment each fence queue's eventfd is registered with KVM at
  most once.
- **FR-004**: A masked vector latches a pending interrupt, reported through the
  device's pending query, and delivers it once on unmask.
- **FR-005**: Delivery works whether MSI-X was enabled before or after
  `DRIVER_OK`.
- **FR-006**: The decisions - when to connect, what is pending - live in
  `src/device` at 100% branch coverage; `src/device/qemu` stays translation
  only; QEMU's own `msix.c` and `virtio-pci.c` are not patched (kb 11: the
  QEMU patch is new files only).
- **FR-007**: Every refusal is logged once, at the decision, with the value
  that caused it.

## Success Criteria *(mandatory)*

- **SC-001**: The L1 table covers every status, enable, mask and vector
  sequence above, with invariants checked after each step.
- **SC-002**: The L5 runner's install stage passes the point where 035's run
  aborted; with `--trace 'kvm_irqchip_*'` each vector's route is added once per
  connect.
- **SC-003**: An L5 scenario disables and enables the adapter 20 times, then
  the submission scenario in `rvktest.exe` passes with QEMU still running.
- **SC-004**: A stress of 10,000 MSI-X enable toggles leaves QEMU's open file
  descriptors and KVM route count where they started.

## Assumptions

- KVM with an in-kernel irqchip, as the in-guest runner uses.
- Windows restarts a display driver by clearing `DRIVER_OK` and toggling MSI-X,
  as observed on build 26200; a real reset is not guaranteed.

## Out of Scope

- Migrating connected interrupt state (live migration is deferred by 025).
- Changing QEMU's virtio-pci or MSI-X code, or reporting the duplicate-irqfd
  assert upstream.
- vhost or vhost-user delivery.
