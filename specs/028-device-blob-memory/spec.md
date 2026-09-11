# Feature Specification: QEMU device: blob resources and host-visible memory

**Feature Branch**: `028-device-blob-memory`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 028 · **Status**: `implement` · **Depends on**: 027

Guest-backed blobs for rings and fence pages; host-visible blobs mapped into
the hostmem BAR so guest vkMapMemory is zero-copy onto host GPU memory.

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
| `Host` (extended) | system contract | + allocation, guest memory mapping, host memory blocks, hostmem placement, control requests, the stock resource list | What the device core may ask of QEMU |
| `CreateBlobRequest` (proto) | object | the header fields of one RESOURCE_CREATE_BLOB | Decodes that request's fixed part |
| `BlobRequest` (proto) | object | resource id, offset | Decodes MAP_BLOB, UNMAP_BLOB and UNREF |
| `GpuResponseWriter` (proto) | object | destination, length | Writes virtio-gpu control responses |
| `GuestPageMapping` | object | host, mapped extents (host-allocated), count, total length | Holds a guest-backed blob's pages mapped for the device |
| `HostMemoryBlock` | object | host, block handle, device pointer, size | Owns one host-visible blob's memory |
| `HostmemSpace` | object | hostmem size | Decides whether a mapping fits the hostmem region |
| `BlobResource` | object | id, memory kind, `GuestPageMapping`, `HostMemoryBlock`, mapped offset | One blob resource |
| `BlobTable` | system | host, `HostmemSpace`, fixed slots of `BlobResource` | Creates, maps, unmaps and releases a device's blobs |
| `ControlRequest` | object | host, request handle | Reads one control request and sends its response |
| `BlobCommands` | object | host, `BlobTable` | Answers the blob control commands the core owns |
| `MockHost` (harness, extended) | object | + fake guest RAM, memory blocks, hostmem placements, fake requests | The adversarial host |

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

## Clarifications

### Session 2026-09-11

- Q (agent, verified): What does stock QEMU do with blobs? -> A: With the
  inherited `blob=on` it advertises `VIRTIO_GPU_F_RESOURCE_BLOB` and handles
  only guest-backed blobs through udmabuf for scanout; MAP_BLOB and UNMAP_BLOB
  answer ERR_UNSPEC. Verified on this host: `virtio-gpu-rvk-pci,blob=on`
  realizes and offers the bit, a capability the core would not honour. The
  device now refuses `blob=on` at realize: its blobs are the core's.
- Q (agent): Which commands does the core own? -> A: RESOURCE_CREATE_BLOB,
  RESOURCE_MAP_BLOB, RESOURCE_UNMAP_BLOB and SET_SCANOUT_BLOB (refused:
  scanout of blobs arrives with presentation), plus RESOURCE_UNREF and
  RESOURCE_CREATE_2D when the id is a blob's (the resource id space is shared
  with the base class's list, which the core asks the host about). Every other
  command falls back to the base class. Blob commands are answered whether or
  not the feature was negotiated, as stock gates them on configuration.
- Q (agent): Guest-backed blobs? -> A: `BLOB_MEM_GUEST` with 1..16384 entries,
  blob id 0: each entry is mapped once on the main loop with `dma_memory_map`
  (a mapping may come back shorter at a region boundary, so an entry takes as
  many mappings as it needs) and the pointers stay valid for the blob's life, so
  workers use them without touching the memory API. A bounce-buffer mapping (not
  guest RAM) is refused: a ring must be shared memory. The entries must cover
  `size`.
- Q (agent): Host-visible blobs before a GPU backend exists? -> A:
  `BLOB_MEM_HOST3D` with `USE_MAPPABLE` and blob id 0 is device-allocated
  mappable memory - a departure from virtio's "blob id names a context object",
  which fits WDDM, where segment memory belongs to the adapter. Its backing is a
  host memory block the host provides: a memfd now, Vulkan exportable memory with
  execution (032). MAP_BLOB places it at a guest-chosen page-aligned offset in
  the hostmem region through one background mapping created at realize (one KVM
  and vhost memslot, not one per blob; unmap needs no RCU wait) and answers
  OK_MAP_INFO with `VIRTIO_GPU_MAP_CACHE_CACHED`. Overlapping, misaligned,
  out-of-range or duplicate mappings are refused. Documented in docs/protocol.md.
- Q (agent): Feature bits? -> A: Unchanged: no `F_RESOURCE_BLOB` until the capset
  and CTX_CREATE (029) give the guest a reason to use blobs, so Linux guests
  binding stock virtio-gpu are not offered blob scanout the device refuses.
- Q (agent): Reset? -> A: Every blob is unmapped and released.

## User Scenarios & Testing *(mandatory)*

Users: the guest driver's rings and fence pages (guest blobs) and user-mode
mappings of segment memory (host-visible blobs); feature 029 onward.

### User Story 1 - Guest-backed blobs (Priority: P1)

1. **Given** guest pages, **When** a GUEST blob is created over them, **Then** it
   succeeds and the device holds the pages mapped until UNREF or reset.
2. Id 0, a live id (blob or 2D), a short request or entry list, entries not
   covering the size, non-RAM entries, and too many entries are refused with the
   documented errors, holding nothing.

### User Story 2 - Host-visible blobs in the hostmem BAR (Priority: P1)

1. **Given** hostmem, **When** a HOST3D mappable blob is created and mapped at a
   page-aligned offset, **Then** OK_MAP_INFO answers CACHED and bytes written
   through BAR 4 are the blob's; UNMAP makes the range read zero.
2. A misaligned, overlapping or out-of-range offset, a second map, a map of a
   guest blob or unknown resource, and an unmap of an unmapped blob are refused.

### User Story 3 - The device stays honest (Priority: P1)

1. `blob=on` fails realize with a named error; the offered features equal stock.

## Requirements *(mandatory)*

- **FR-001**: Every field of a blob request is checked before memory is mapped
  or allocated (fuzz).
- **FR-002**: No virtio feature beyond stock is advertised.

## Success Criteria *(mandatory)*

- **SC-001**: The qtest suite creates, maps, reads back through BAR 4, unmaps and
  releases blobs on `virtio-gpu-rvk-pci,hostmem=64M`, and every refusal.
- **SC-002**: 100% branch coverage of the new core; fuzz_blob_request clean.

## Out of Scope

- `F_RESOURCE_BLOB`, capsets, contexts (029); blob scanout; GPU memory backing
  (032); migration.
