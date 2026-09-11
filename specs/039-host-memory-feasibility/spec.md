# Feature Specification: Host memory and interrupt feasibility on real host GPUs

**Feature Branch**: `039-host-memory-feasibility`

**Created**: 2026-09-11

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 039 · **Status**: `implement` · **Depends on**: 028, 031, 036

M2's spikes, run on every host GPU class and recorded: exported host GPU
memory mapped into a KVM memslot through the hostmem BAR, guest-backed pages
imported with VK_EXT_external_memory_host through a memfd window, and irqfd
injection from workers under vector-masking stress. Each outcome becomes a
capset bit with a tested fallback, so no guest component guesses.

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
| | object / system | | |

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

Users: the designers of features 041 to 043, who must know - per host GPU
class, not by assumption - whether host GPU memory can be mapped into the
guest, whether guest pages can be imported by the host driver, and whether
interrupt injection from worker threads holds under stress. kb 11's M2 names
these spikes because each can reshape M3; this feature runs them, records the
answers, and turns each answer into a capset bit with a tested fallback.

### User Story 1 - Host-visible memory in a KVM memslot (Priority: P1)

**Independent Test**: a repeatable host probe run on each host GPU, plus a
KVM-accelerated check that a guest reads and writes the mapped memory. The
guest may be a minimal Linux guest: the question is the host's KVM and GPU
driver behaviour, not Windows.

**Acceptance Scenarios**:

1. **Given** a host Vulkan device that exports memory
   (`VK_KHR_external_memory_fd` or dma-buf), **When** the device maps it into
   the hostmem BAR at a `RESOURCE_MAP_BLOB` offset, **Then** bytes the guest
   writes are visible to the host GPU and the reverse, for sizes from 4 KiB to
   1 GiB.
2. **Given** a host driver whose memory KVM cannot map (the guest access faults
   in KVM), **Then** QEMU does not crash, the probe records the failure with
   its errno, and the device does not report the bit.
3. **Given** the mapping survives a guest reboot and a blob unmap and remap,
   **Then** no host memory leaks.

### User Story 2 - Guest-backed pages imported by the host driver (Priority: P1)

**Independent Test**: the probe imports a memfd window built from scattered
guest pages with `VK_EXT_external_memory_host` and verifies contents through a
host GPU copy.

**Acceptance Scenarios**:

1. **Given** guest RAM on `memory-backend-memfd` with `share=on`, **When** the
   device builds a contiguous window over locked guest pages and imports it,
   **Then** the host GPU reads what the guest wrote with no copy.
2. **Given** a host without the extension, or RAM not shareable, **Then** the
   copy fallback is used and its throughput is recorded.

### User Story 3 - Interrupt injection under stress (Priority: P2)

**Independent Test**: a stress run of worker-raised fence interrupts at the
highest rate the host sustains while the guest masks and unmasks vectors.

**Acceptance Scenarios**:

1. **Given** 10 minutes of fences raised from several workers while vectors are
   masked and unmasked, **Then** no final interrupt is lost, no MSI is injected
   while masked, and the latency distribution is recorded.

### User Story 4 - Every answer is a capset bit with a fallback (Priority: P1)

**Independent Test**: L1 tests of the device reporting each bit from its probe
result, and L4 loopback runs with every bit cleared.

**Acceptance Scenarios**:

1. **Given** a probe that passes at realize on this host, **Then** the capset
   carries its bit; **given** one that fails or was not run, **Then** it does
   not.
2. **Given** each bit cleared, **Then** the loopback suite passes on the
   fallback path.

### Edge Cases

- The host driver refuses export, or `mmap` of the export raises SIGBUS.
- The hostmem BAR too small for the requested blob.
- Transparent huge pages or hugetlbfs guest RAM.
- A host GPU reset or driver unload while memory is mapped into the guest.
- Two devices sharing one host GPU, or one host GPU class with two drivers
  (NVIDIA proprietary and nouveau).
- 32-bit guest processes needing mappings below 4 GiB (consumed by 041).

## Requirements *(mandatory)*

- **FR-001**: Each spike is a repeatable host-side tool with machine-readable
  output, not a one-off experiment.
- **FR-002**: Results per host GPU class - driver name and version, Vulkan
  version, outcome, errno, throughput - are recorded in the knowledge base.
- **FR-003**: The device decides each capset bit from a probe at realize on the
  actual host, and never reports a bit whose probe did not pass
  (constitution VI).
- **FR-004**: A failed probe never crashes QEMU or the guest.
- **FR-005**: Every bit has an L1 test for both values and a fallback exercised
  at L4.

## Success Criteria *(mandatory)*

- **SC-001**: Results recorded for this host's NVIDIA (proprietary driver),
  Intel (Mesa ANV) and llvmpipe devices; AMD recorded as not measured.
- **SC-002**: The stress run completes 10 minutes with no lost interrupt.
- **SC-003**: The loopback suite passes with every bit cleared and with every
  bit set that this host supports.

## Assumptions

- The host has an NVIDIA RTX 3070 Ti Laptop GPU, Intel Iris Xe (Alder Lake)
  and llvmpipe; AMD hardware is not available.
- Feature 036's interrupt lifetime is in place before the stress run.

## Out of Scope

- Mapping host-visible memory into a Windows process (041).
- The executor importing memory (042).
- Choosing a different design if every probe fails; that is a spec change to
  041.
