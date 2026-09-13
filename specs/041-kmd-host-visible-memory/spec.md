# Feature Specification: KMD: host-visible memory a process can map

**Feature Branch**: `041-kmd-host-visible-memory`

**Created**: 2026-09-11

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 041 · **Status**: `implement` · **Depends on**: 037, 039

Host-visible blobs become allocations in a CPU-visible memory segment backed
by the hostmem BAR, so D3DKMTLock2 and a GPU mapping hand a process real host
GPU memory with no copy; guest-backed rings and fence pages map the same way;
32-bit processes get views below 4 GiB; budgets, residency and eviction are
reported truthfully from the capset bits feature 039 defines.

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
| `HostVisibleSegment` | object | the hostmem region's guest physical base and size, and whether the device offers it | Describe the CPU-visible segment to VidMm |
| `MemorySegments` | system | the segments this adapter reports, in the order VidMm numbers them | Answer what memory this adapter has |
| `HostVisibleBlob` | object | one allocation's blob: its resource id, its placement, and whether it is mapped | Keep one allocation's memory on the device |

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

- [`docs/kb/04-memory.md`](../../docs/kb/04-memory.md)
- [`docs/kb/11-implementation-plan.md`](../../docs/kb/11-implementation-plan.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## User Scenarios & Testing *(mandatory)*

Users: the guest transport (feature 043) and every driver above it - render-server's
Vulkan, OpenGL and OpenCL drivers, later `rvkumd.dll` - which need memory a
process can map that *is* host GPU memory, plus rings and fence pages mapped into
the same process, without a copy and without trusting the process that asks.

### User Story 1 - Host-visible memory mapped into a process with no copy (Priority: P1)

**Independent Test**: on Linux, the fake dxgkrnl and VidMm drive
`DxgkDdiCreateAllocation`, `BuildPagingBuffer` and the lock path against the
device core over the loopback rig; a byte written through the process view is the
byte the device's host-visible blob holds, and no copy operation is recorded.

**Acceptance Scenarios**:

1. **Given** an adapter whose capset reports host-visible mapping, **When** a
   process creates an allocation whose descriptor asks for host-visible memory,
   **Then** the KMD places it in a CPU-visible memory segment backed by the
   hostmem BAR, creates the host-visible blob, and maps it at the BAR offset the
   device returns.
2. **Given** that allocation, **When** the process locks it (`D3DKMTLock2`) or
   maps it at a GPU virtual address, **Then** its CPU view addresses the BAR
   range for that blob and a write through it is visible to the host executor
   without a transfer command.
3. **Given** an adapter whose capset does not report host-visible mapping,
   **When** the same allocation is requested, **Then** it is created
   guest-backed, the allocation's reported kind says so, and the copy fallback
   feature 039 defines is the only path - never a mapping that silently aliases
   guest RAM.

### User Story 2 - Rings and fence pages a process can map (Priority: P1)

**Independent Test**: the harness creates an rvk context's ring and fence page as
guest-backed allocations, maps them into a simulated process, and checks the ring
offsets and fence values the process sees are those the device writes.

**Acceptance Scenarios**:

1. **Given** an rvk context, **When** its ring and fence page are created,
   **Then** each is a guest-backed allocation the KMD pins for the context's
   lifetime and a process holding the device can map read-write (ring) or
   read-only (fence page).
2. **Given** a mapped fence page, **When** the device advances a fence, **Then**
   the process reads the new value with no kernel transition.

### User Story 3 - 32-bit processes and budgets (Priority: P2)

**Independent Test**: the harness requests a view for a WoW64 process and checks
the returned address is below 4 GiB; `QueryAdapterInfo` segment and budget
answers are checked against the capset.

**Acceptance Scenarios**:

1. **Given** a 32-bit process, **When** it maps a host-visible allocation,
   **Then** the view lies below 4 GiB, or the map fails with a status and a log
   line - never a truncated pointer.
2. **Given** the adapter, **When** the runtime queries segments and memory
   budgets, **Then** the CPU-visible segment's size is the hostmem BAR size and
   the budget never exceeds what the device can back.

### Edge Cases

- A hostile process's allocation descriptor names a size that overflows when
  page-aligned, a kind outside the enumeration, an offset past the BAR, or a
  blob id it does not own: `DxgkDdiCreateAllocation` refuses each with a status;
  no blob is created and nothing is mapped.
- The hostmem BAR is exhausted: creation fails with an out-of-memory status the
  runtime turns into a budget failure, not a bugcheck.
- VidMm evicts a host-visible allocation under memory pressure: the CPU view is
  invalidated through the paging path, the blob is unmapped from the BAR, and a
  later lock re-maps it; a stale view never addresses another allocation's blob.
- The device resets or the adapter TDRs while a process holds a view: every
  mapping is torn down, locks fail with device-lost, and nothing reads a freed
  BAR range.
- Two processes open the same shared allocation: both views address the same
  blob, and destroying one handle leaves the other mapping valid.

## Requirements *(mandatory)*

- **FR-001**: Host-visible allocations live in a CPU-visible memory segment whose
  physical range is the hostmem BAR; the segment is reported only when the
  capset's host-visible bit is set.
- **FR-002**: Every allocation descriptor from user mode is validated - magic,
  kind, sizes, alignment, overflow - before any blob is created; a refusal logs
  once with the offending value (constitution IX).
- **FR-003**: Guest-backed allocations for rings and fence pages are pinned for
  their context's lifetime and mappable by the owning process only.
- **FR-004**: Views for 32-bit processes are placed below 4 GiB or refused.
- **FR-005**: Eviction and residency changes unmap the blob from the BAR before
  its range is reused; a CPU view after eviction faults into the paging path,
  never into another allocation.
- **FR-006**: Budgets, segment sizes and commit limits reported through
  `QueryAdapterInfo` and `D3DKMTQueryVideoMemoryInfo` derive from the capset and
  never exceed what the device backs (constitution VI).
- **FR-007**: The copy fallback is chosen per adapter from the capset bits feature
  039 defines, and is observable: the allocation's kind reports it.

## Success Criteria *(mandatory)*

- **SC-001**: 100% branch coverage of the new core code, invariants after every
  mutation, a hostile-descriptor table test and a libFuzzer target for the
  descriptor reader.
- **SC-002**: The loopback rig proves a write through a mapped view reaches the
  device's blob with zero transfer commands.
- **SC-003**: In the guest, a test program maps a 256 MiB host-visible allocation
  and the host executor reads back a pattern written through the view.

## Assumptions

- Feature 039 has established, per host GPU class, whether exported host memory
  can be inserted into the hostmem BAR, and defined the capset bits.
- Guest RAM is on `memory-backend-memfd`, as the runner already configures.

## Out of Scope

- The transport that consumes these allocations (043).
- Sharing allocations across processes by NT handle (053).
- Live migration and dirty tracking of host-visible memory.
