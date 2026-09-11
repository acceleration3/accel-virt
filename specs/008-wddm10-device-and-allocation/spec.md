# Feature Specification: WDDM 1.0: devices, allocations and segments

**Feature Branch**: `008-wddm10-device-and-allocation`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 008 · **Status**: `implement` · **Depends on**: 007
· **WDDM level**: WDDM 1.0 (Windows Vista)

The memory object model: segments reported to VidMm, allocations created,
described, opened and closed, and the standard allocations the runtime makes
without the UMD.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### DDI slots in scope (9)

| Slot | Field | Introduced |
|--:|---|---|
| 18 | `DxgkDdiCreateDevice` | BASE |
| 19 | `DxgkDdiCreateAllocation` | BASE |
| 20 | `DxgkDdiDestroyAllocation` | BASE |
| 21 | `DxgkDdiDescribeAllocation` | BASE |
| 22 | `DxgkDdiGetStandardAllocationDriverData` | BASE |
| 50 | `DxgkDdiDestroyDevice` | BASE |
| 51 | `DxgkDdiOpenAllocation` | BASE |
| 52 | `DxgkDdiCloseAllocation` | BASE |
| 61 | `DxgkDdiSetDisplayPrivateDriverFormat` | BASE |

Slot numbers are indices into `DRIVER_INITIALIZATION_DATA` as laid out by the WDDM 3.2 headers; see `docs/ref/driver_init_table.md`. They are the fastest way to navigate an unsymbolized vendor driver.

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
| `ObjectPool<T>` | system | platform, chunk list, live count | Holds objects of one type at stable addresses |
| `AllocationDescriptor` | object | kind, flags, width, height, format, pitch, size, refresh rate, source | Encodes one allocation's private data (`docs/protocol.md`) |
| `StandardAllocation` | object | platform, one `DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA` | Describes one runtime standard allocation as a descriptor |
| `ApertureSegment` | object | platform, size | Answers VidMm's segment query for guest system memory |
| `Allocation` | object | descriptor, page-rounded size, open count | One allocation VidMm manages |
| `AllocationTable` | system | platform, mutex, allocation pool | Owns the adapter's allocations |
| `Device` | object | adapter, dxgkrnl device handle, creation flags | One D3D device |
| `DeviceAllocation` | object | device, allocation | One device's open of an allocation |
| `DeviceTable` | system | platform, mutex, device pool, open pool, allocation table | Owns the adapter's devices and what they have open |
| `DisplaySources` | object | private format attribute per source | Remembers each source's private driver format |
| DDI records (13) | DDI record | the fields of one WDK structure | Overlays memory dxgkrnl owns, layout-proven |
| `FakeVidMm` (harness) | object | platform, table, segment answer, failed rule | Queries and validates segments the way `CreatePhysicalAdapterSegments` does |

### Test requirements

Constitution Principle III applies in full. Concretely, this feature is not done until:

- every branch of every function it adds to `src/core`, `src/device` or `src/proto` is covered, or marked `AV_UNTESTABLE` with a justification;
- each stateful module it adds exports an invariant function that tests call after **every** mutation;
- every function it adds that reads bytes from outside its own address space has a hostile-input table test and a libFuzzer target with a committed corpus;
- the allocation-failure sweep passes: with each allocation site failed in turn, no crash and no leak;
- IRQL annotations are present and the mock's IRQL checker is satisfied;
- the DDI conformance harness (feature 003) drives every slot above, including the abusive-but-legal sequences for it;
- `tools/check_symbol_tests.py` reports no untested exported symbol.

### Reference material

Knowledge base:

- [`docs/kb/03-object-model.md`](../../docs/kb/03-object-model.md)
- [`docs/kb/04-memory.md`](../../docs/kb/04-memory.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11

- Q (agent -> user): Devices and allocations have no fixed bound, and
  `ObjectStorage<T>` holds one object in memory that already exists. How are
  unbounded objects constructed? -> A (user): `ObjectPool<T>` in
  `src/core/object_pool.hpp`, a second placement-new site: fixed-size
  non-paged chunks from `Platform`, objects constructed and destroyed in place
  at stable addresses usable as dxgkrnl handles. Constitution 2.2.0 (MINOR);
  `tools/check_design.py` allows placement new in exactly the two headers.
- Q (agent, RE): Which segment query does VidMm issue? -> A:
  `CreatePhysicalAdapterSegments @ 14009de94` (dxgmms2) asks
  `DXGKQAITYPE_QUERYSEGMENT4` twice: first with a null descriptor array for
  `NbSegment` (0 or more than 32 fails), then with `SegmentDescriptorStride`
  0x68; `ValidateSegmentDescriptors @ 14009ec8c` requires page-aligned sizes,
  no `Agp` flag, flags below bit 22, hibernate-preservation flags only with
  standby preservation, no `Aperture`/`CpuVisible` with a CPU host aperture, and
  `PagingBufferSegmentId` <= `NbSegment`.
- Q (agent, RE): What does a render adapter need registered? ->
  A: `ADAPTER_RENDER::CreateRenderCore @ 1401b13d8` creates a render core only
  when CreateDevice, DestroyAllocation, DescribeAllocation,
  GetStandardAllocationDriverData, SubmitCommand, PreemptCommand,
  BuildPagingBuffer, ResetFromTimeout, RestartFromTimeout, CollectDbgInfo,
  QueryCurrentFence, ControlInterrupt, DestroyDevice, OpenAllocation,
  CloseAllocation and Present are all registered - and for WDDM 1.x also
  CreateAllocation, Patch and Render. With none registered the adapter is not a
  render adapter; with some but not all, start fails with
  `STATUS_REVISION_MISMATCH` ("Not all the render relevant DDIs are NULL").
  So from this feature until 012 registers Present, the adapter is a partial
  render adapter; the harness names the missing slots instead of pretending
  otherwise.
- Q (agent): What segment does a WDDM 1.0-phase adapter on a stock virtio-gpu
  report? -> A: One aperture segment - `Aperture | CpuVisible |
  CacheCoherent`, the size of system memory - as `basicrender.sys` reports
  (`WarpKMQueryAdapterInfo`, types 5 and 11). The allocation's pages are guest
  memory the device already reaches; host-visible memory is a later segment
  (feature 028).
- Q (agent): What is the allocation private data? -> A: A new rvk structure,
  `AllocationDescriptor` (`docs/protocol.md`), 48 bytes: magic, kind, flags,
  width, height, D3DDDIFORMAT, pitch, size, refresh rate, VidPn source.
  `GetStandardAllocationDriverData` writes it for the runtime's standard
  allocations; a user-mode driver writes it for its own. `basicrender.sys` does
  the same with its 0x60-byte `WARPALLOCATIONINFO`.

## User Scenarios & Testing *(mandatory)*

Users: VidMm, which learns the memory model; the runtime and DWM, which create
standard allocations without a user-mode driver; later features (009 submits
DMA buffers referencing these allocations, 012 scans one out).

### User Story 1 - VidMm learns one aperture segment (Priority: P1)

**Independent Test**: the harness's VidMm model issues both
`QUERYSEGMENT4` passes after StartDevice and applies
`ValidateSegmentDescriptors`; it accepts the answer.

**Acceptance Scenarios**:

1. **Given** a started adapter and a `QUERYSEGMENT4` with a null descriptor
   array, **Then** `NbSegment` = 1.
2. **Given** the second pass with stride 0x68, **Then** one descriptor:
   aperture, CPU visible, cache coherent, base 0xC0000000, size and commit
   limit = system memory rounded down to a page; paging buffer in segment 0 of
   one page.
3. **Given** a stride below 0x68, room for no descriptor, a physical adapter
   index other than 0, or no started adapter, **Then**
   `STATUS_INVALID_PARAMETER` and nothing written.

### User Story 2 - The runtime's standard allocations (Priority: P1)

**Acceptance Scenarios**:

1. **Given** `GetStandardAllocationDriverData` with no private-data buffer,
   **Then** it reports the descriptor size and no resource data.
2. **Given** a shared primary, shadow, staging or GDI (texture, staging,
   CPU-visible) surface of a 32-bpp format, **Then** a descriptor with pitch
   `width * 4` and size `pitch * height`, and the type's `Pitch` output set.
3. **Given** a format that is not 32-bpp, a GDI lookup table, existing system
   memory, cross-adapter surface, vGPU or fence-storage type, or a size that
   overflows 32 bits, **Then** `STATUS_INVALID_PARAMETER`.

### User Story 3 - Devices and allocation lifetime (Priority: P1)

**Acceptance Scenarios**:

1. **Given** CreateDevice, **Then** a driver device handle; DestroyDevice
   releases it and every allocation it still has open.
2. **Given** CreateAllocation with N descriptors, **Then** each allocation gets
   a driver handle, size rounded up to a page, segment set {1}, preferred
   segment 1, CPU visible when the descriptor says so.
3. **Given** a malformed descriptor anywhere in the list, a missing list,
   resource private data, or an allocation larger than the segment, **Then** the
   call fails and no allocation from it survives.
4. **Given** OpenAllocation on a device, **Then** each dxgkrnl allocation handle
   resolves through `DxgkCbGetHandleData` to one of this adapter's allocations
   and gets a per-device handle; CloseAllocation releases them.
5. **Given** DestroyAllocation of allocations this adapter created, **Then**
   they are released; a handle that is not a live allocation fails the whole
   call before anything is released.
6. **Given** DescribeAllocation, **Then** width, height, format, refresh rate
   and the source's private format attribute, identity rotation.
7. **Given** each allocation site failing in turn, **Then** no leak and no
   partial object survives.

### User Story 4 - The adapter's render DDI set is tracked (Priority: P2)

**Acceptance Scenarios**:

1. **Given** the registered table, **When** the harness applies
   `CreateRenderCore`'s rule, **Then** it reports the adapter as a partial
   render adapter and names the first missing slot (`SubmitCommand` at this
   feature).
2. **Given** SetDisplayPrivateDriverFormat for source 0, **Then** the attribute
   is stored and a primary on that source describes with it; another source is
   `STATUS_INVALID_PARAMETER`.

### Edge Cases

- A handle pointing inside a pool chunk but not at a live object.
- DestroyDevice with opens outstanding; RemoveDevice with devices outstanding.
- An allocation count of zero or above the per-call ceiling.
- A descriptor whose `pitch * height` exceeds its size.

## Requirements *(mandatory)*

- **FR-001**: Devices, allocations and per-device opens MUST live in
  `ObjectPool`s; every handle dxgkrnl passes back MUST be checked to be a live
  object of the right pool before use.
- **FR-002**: Every multi-object DDI MUST be all-or-nothing.
- **FR-003**: `AllocationDescriptor` MUST be decoded from a copy, validated
  (magic, kind, flags, sizes overflow-checked), hostile-swept and fuzzed.
- **FR-004**: The segment answer MUST pass VidMm's recorded validation.
- **FR-005**: No capability bit changes: WDDM 1.0, physical addressing.
- **FR-006**: The harness MUST model `CreateRenderCore`'s DDI rule and VidMm's
  segment queries, citing their addresses.

## Success Criteria *(mandatory)*

- **SC-001**: 100% branch coverage of added core; allocation-failure sweeps
  over CreateDevice, CreateAllocation and OpenAllocation leak nothing.
- **SC-002**: The descriptor and argument-block fuzz targets run clean.
- **SC-003**: A scenario creates a device, the runtime's primary through
  GetStandardAllocationDriverData and CreateAllocation, opens, describes,
  closes and destroys it through the registered table with no leak.

## Assumptions

- `DXGK_DEVICE_INFO.SystemMemorySize` is the system memory VidMm may place
  aperture allocations in.
- dxgkrnl serializes nothing on our behalf: the object pools take a mutex.
- 32-bpp formats only until a mode needs another (011).

## Out of Scope

- Paging (`BuildPagingBuffer`, 009), placement and `MAP_APERTURE_SEGMENT`.
- Host-visible memory segments (028), GPU virtual addressing (017), shared
  NT handles and cross-adapter resources.
