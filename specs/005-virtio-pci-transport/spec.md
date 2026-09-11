# Feature Specification: virtio-pci transport inside the miniport

**Feature Branch**: `005-virtio-pci-transport`

**Created**: 2026-09-10

**Status**: Draft

**Input**: `/dev` autonomous run after 004

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 005 · **Status**: `implement` · **Depends on**: 002, 004

Capability walk, feature negotiation, device status FSM, split virtqueues and
MSI-X, written from the virtio 1.2 spec because no virtio library exists under
a WDDM miniport.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### Design rules — binding on this feature

Constitution Principle III and [`CLAUDE.md`](../../CLAUDE.md). Restated here because it is checked at review, not assumed:

- **Every unit of behaviour is a class that owns state.** No static member functions. No stateless class used as a namespace. No free function outside the two ABI boundaries (`DxgkDdi*` thunks in `src/km/`, and `extern "C"` entry points), each one line with no logic.
- If functions thread the same handle, buffer, bounds or platform through their parameters, **that parameter is a member**. Make it a class and inject its dependencies at construction.
- Name the classes this feature adds before writing them, and say which kind each is: an **object class** (one instance per real thing) or a **system class** (a long-lived service owning a collection).
- Dependencies injected at construction. No global, no singleton, no file-scope state.
- Data members private, trailing underscore. Classes `PascalCase`, methods and locals `snake_case`. No `Manager`, `Helper`, `Utils`, `Handler`, `Impl`, `process()`, `handle()`.
- One class per header, named for it. A header declares; it does not define. One responsibility per class, statable without an "and".
- Errors are `av::Status` values. No exceptions, no RTTI, no C++ standard library, no `new`/`delete`. Cleanup is RAII via `ScopeExit`, never repeated per branch.
- Fallible construction is two-phase: trivial constructor, `initialize(...)` returning `Status`.
- Every public method declares its IRQL ceiling via `platform_.require_irql_at_most(...)`.

### Classes this feature introduces

<!-- Fill this in during /speckit-plan, before writing code. One row per class; if a row has no state, it is not a class. -->

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `DmaBuffer` | object | platform, virtual address, physical address, size | Owns one physically contiguous device-visible allocation |
| `VirtioRegion` | object | BAR, offset, length, present | Locates one virtio structure inside a BAR |
| `VirtioPciLayout` | object | common, notify, ISR, device regions, notify multiplier, shared-memory regions | Records where a device's virtio structures live |
| `VirtioCapabilityWalk` | object | copied config-space bytes | Parses the virtio capabilities from one config-space snapshot |
| `RegisterWindow` | interface | - (kernel MMIO and the fake device implement it) | Reads and writes one mapped register region |
| `VirtioPciTransport` | system | platform, four register windows, notify multiplier, negotiated features, status | Drives one device through initialization and queue activation |
| `VirtqSegment` | object | physical address, length, writable | Describes one buffer in a descriptor chain |
| `VirtQueue` | object | platform, ring `DmaBuffer`, size, free list, shadow chain table, avail and used indices | Moves descriptor chains to a device and validates what comes back |
| `FakeVirtioDevice` (`tests/harness`) | system | offered features, status, per-queue registers, device config, hostile knobs | Plays a virtio-pci device against the transport in unit tests |

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

- [`docs/kb/11-implementation-plan.md`](../../docs/kb/11-implementation-plan.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11 (autonomous run)

- Q: Source of truth? -> A: virtio 1.2 (OASIS cs01): 4.1.4 capabilities,
  4.1.4.3 common configuration, 4.1.4.4 notifications, 3.1.1 initialization,
  2.7 split virtqueues. Layouts were checked against the spec text
  (research.md R1).
- Q: How does core reach registers? -> A: A `RegisterWindow` interface with two
  implementations - kernel MMIO translation (`src/km`) and the harness's fake
  device - the same pattern and justification as `Platform`.
- Q: Ring memory? -> A: `Platform` gains physically contiguous allocation and a
  microsecond stall; the kernel uses `MmAllocateContiguousMemorySpecifyCache`
  and `KeStallExecutionProcessor`. No IOMMU (`VIRTIO_F_ACCESS_PLATFORM` is not
  negotiated), so guest physical addresses are device addresses.
- Q: What is trusted from the device? -> A: Nothing it writes: config space,
  common-config reads and the used ring are validated; the driver keeps a
  private shadow of every descriptor chain and never reads descriptors back.
- Q: Scope? -> A: Split virtqueues only (no packed, no indirect descriptors,
  no event index); one MSI-X vector per queue plus config when enough messages
  are granted, otherwise all on vector 0.

## User Scenarios & Testing *(mandatory)*

Users: the WDDM adapter (feature 006) and display/submission features, which
send virtio-gpu control commands through this transport; reviewers who need a
hostile device to be harmless to the guest kernel.

### User Story 1 - Find the device's virtio structures in a hostile config space (Priority: P1)

**Independent Test**: a 256-byte config-space snapshot yields the common,
notify, ISR and device regions and shared-memory regions per virtio 1.2
4.1.4, or a named rejection; no snapshot can loop, read out of bounds or
crash (table test plus fuzz target).

**Acceptance Scenarios**:

1. **Given** a config space with the five virtio capabilities (as QEMU's
   virtio-gpu-pci presents them), **Then** each region's BAR, offset, length
   and the notify multiplier are reported.
2. **Given** two capabilities of one type, **Then** the first is used.
3. **Given** a reserved `cfg_type` or `bar` > 5, **Then** that capability is
   ignored.
4. **Given** a capability list that loops, a pointer below 0x40, a
   capability running past 0xFF, or `cap_len` shorter than its structure,
   **Then** `malformed`.
5. **Given** no common, notify, ISR or device capability, **Then**
   `not_supported`.
6. **Given** a notify multiplier that is neither 0 nor an even power of two,
   **Then** `malformed`.

### User Story 2 - Initialize and negotiate by the book, fail safely (Priority: P1)

**Independent Test**: against the fake device, initialization follows 3.1.1
exactly and every device misbehaviour ends in a status plus FAILED written to
the device.

**Acceptance Scenarios**:

1. **Given** a device offering `VERSION_1` and the wanted features, **When**
   negotiating, **Then** the status sequence is reset, ACKNOWLEDGE, DRIVER,
   FEATURES_OK (read back), and the negotiated set is wanted ∩ offered.
2. **Given** a device not offering a required feature (at least `VERSION_1`),
   **Then** `not_supported` and FAILED is set.
3. **Given** a device that clears FEATURES_OK on read-back, **Then**
   `not_supported` and FAILED.
4. **Given** a device that never finishes reset within the bound, **Then**
   `invalid_state`.
5. **Given** a device that sets DEVICE_NEEDS_RESET, **Then** the transport
   reports it.
6. **Given** MSI-X vector assignment, **When** the device reads back
   `VIRTIO_MSI_NO_VECTOR`, **Then** `no_space`.

### User Story 3 - Move requests through a split virtqueue, trusting nothing back (Priority: P1)

**Independent Test**: requests round-trip through the fake device; every
hostile used-ring value is rejected without corrupting the queue (table test
plus fuzz target).

**Acceptance Scenarios**:

1. **Given** a queue of size N, **When** activated, **Then** the device sees
   the desc/avail/used physical addresses, size N and the chosen vector, and
   `queue_enable` reads back 1.
2. **Given** a chain of read and write segments, **When** added and
   completed, **Then** the device sees the segments in order with the right
   flags, and the driver gets the head token and written length back.
3. **Given** the queue is full, **When** adding, **Then** `no_space`; after
   completions free descriptors, adding succeeds.
4. **Given** a used element whose id is out of range, not an in-flight head,
   or whose length exceeds the chain's writable bytes, **Then**
   `invalid_state`, and the queue stays usable.
5. **Given** a used index that advances by more than N, **Then** `invalid_state`.
6. **Given** a queue size not a power of two or above 32768 from the device,
   **Then** activation is `malformed`.

### Edge Cases

- A queue size the device reports as 0: the queue does not exist (`not_found`).
- `num_queues` smaller than the requested index: `not_found`.
- Adding a chain longer than the free descriptors: `no_space`, nothing published.
- Used index wrapping 65535 -> 0: handled by 16-bit modular arithmetic.
- Allocation failure for ring memory or shadow table: `no_memory`, nothing leaked.

## Requirements *(mandatory)*

- **FR-001**: The capability walk MUST follow virtio 1.2 4.1.4.1 driver
  requirements, bound its iterations, and reject loops and out-of-range reads.
- **FR-002**: Initialization MUST follow 3.1.1 and set FAILED on any failure
  after ACKNOWLEDGE.
- **FR-003**: Negotiated features MUST be wanted ∩ offered, and MUST include
  every required feature.
- **FR-004**: Virtqueue activation MUST program size, vector, and the three
  ring addresses before enabling, and verify vector and enable by read-back.
- **FR-005**: The virtqueue MUST keep a private shadow of chains and validate
  every used element against it.
- **FR-006**: Adding, notifying and harvesting MUST not allocate or block
  (DISPATCH-callable); initialization and activation are PASSIVE.
- **FR-007**: `Platform` MUST provide contiguous DMA allocation with a physical
  address, and a bounded stall; the mock MUST translate its fake physical
  addresses for the fake device and record allocation at DISPATCH.
- **FR-008**: The kernel implementations (`KernelPlatform` additions, MMIO
  register window) MUST be translation only.
- **FR-009**: Config-space parsing and used-ring harvesting MUST each have a
  hostile table test and be reachable from a fuzz target with seeds.

## Success Criteria *(mandatory)*

- **SC-001**: The config space captured from QEMU's `virtio-gpu-pci` is parsed
  to the regions QEMU documents.
- **SC-002**: 100% branch coverage of the new core code; no untestable marks.
- **SC-003**: Fuzz smoke of both targets clean for 60 s.
- **SC-004**: The driver build links the kernel register window and platform
  additions against `ntoskrnl` only.

## Assumptions

- Modern (non-transitional) virtio 1.x only; legacy I/O port interface not supported.
- The adapter (006) maps BARs and supplies the windows; this feature does not
  touch `DXGK_DEVICE_INFO`.

## Out of Scope

- Packed virtqueues, indirect descriptors, `VIRTIO_F_RING_EVENT_IDX`.
- virtio-gpu command encoding (006/011/012).
- Interrupt routing through `DxgkDdiInterruptRoutine` (006).
