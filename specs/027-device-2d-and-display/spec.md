# Feature Specification: QEMU device: 2D resources, scanout and EDID

**Feature Branch**: `027-device-2d-and-display`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 027 · **Status**: `implement` · **Depends on**: 026

The base class's 2D protocol, which feature 012 talks to. Testable against
stock virtio-gpu-pci before the custom device exists.

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
| `Qtest` (test tool, Python) | object | a QEMU process, its qtest pipes, its stderr file | Drives one QEMU over qtest |
| `GuestMemory` (test tool) | object | qtest, next free guest address | Hands out guest RAM for queues and buffers |
| `PciFunction` (test tool) | object | qtest, assigned BAR addresses | Reads and writes one function's configuration space as firmware would |
| `SplitQueue` (test tool) | object | qtest, ring addresses, next descriptor, next available, last used | Exchanges request chains on one split virtqueue |
| `VirtioPciDevice` (test tool) | object | PCI function, capability regions, notify multiplier, queues | Brings up one modern virtio-pci device |
| `VirtioGpu` (test tool) | object | the device, its control and cursor queues, negotiated features | Sends virtio-gpu requests on the right queue |
| `gpu_request_bytes` (test tool, C++) | executable | - | Prints the driver's `GpuRequestWriter` bytes for one request |

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

- [`docs/kb/06-display.md`](../../docs/kb/06-display.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11

- Q (agent, verified): What does the device add for 2D? -> A: Nothing. The rvk
  device overrides no command, so the base class answers `GET_DISPLAY_INFO`,
  `GET_EDID`, `RESOURCE_CREATE_2D`, `RESOURCE_ATTACH_BACKING`,
  `TRANSFER_TO_HOST_2D`, `SET_SCANOUT`, `RESOURCE_FLUSH`, `RESOURCE_UNREF` and
  the cursor queue exactly as stock virtio-gpu does, with its defaults: one
  output, 1280x800 (the driver's preferred mode), EDID on. This feature proves
  that against the real device.
- Q (agent): How, without a guest? -> A: qtest. `tests/qemu/virtio_qtest.py` is
  a virtio-gpu driver over qtest: it assigns BARs, walks the virtio capabilities,
  negotiates, lays split queues in guest RAM and polls the used rings. The
  requests `rvkgpu.sys` sends today (feature 013's cursor path) are the driver's
  own bytes from `GpuRequestWriter`, printed by `gpu_request_bytes`; the display
  requests are laid out from the virtio 1.2 structures.
- Q (agent): Stock too? -> A: Every test runs against the patched QEMU's stock
  `virtio-gpu-pci` and `virtio-gpu-rvk-pci`, which must answer alike, and the
  rvk device must offer exactly stock's feature bits (no virgl, blob or
  context-init until 028/029).
- Q (agent): Guest primaries on the host display? -> A: Not this feature. The
  driver today stores the scanout address for vsync (012) and does not yet send
  `SET_SCANOUT` / `RESOURCE_FLUSH` for its primaries; the device path it will use
  is proven here, and wiring the driver to it belongs to the driver's display
  work, recorded in docs/roadmap notes.

## User Scenarios & Testing *(mandatory)*

Users: the guest driver's cursor and display requests; the device features
that follow, which reuse the qtest driver.

### User Story 1 - The 2D protocol the driver relies on (Priority: P1)

1. **Given** either device, **When** the display info and EDID are asked,
   **Then** one enabled 1280x800 output and a valid EDID base block come back.
2. **Given** the driver's cursor requests (create, attach, transfer, update,
   move, hide), **Then** each succeeds.
3. **Given** a 1280x800 resource with backing, **When** it is scanned out,
   flushed, disabled and unreferenced, **Then** each succeeds.
4. Duplicate or zero resource ids, a second scanout, unknown resources and an
   unknown command get the documented errors.

## Requirements *(mandatory)*

- **FR-001**: The rvk device offers stock virtio-gpu's features exactly.
- **FR-002**: Driver requests are the driver's own bytes.

## Success Criteria *(mandatory)*

- **SC-001**: `tools/gate.sh`'s qemu configuration runs the suite against both
  devices.

## Out of Scope

- The driver sending SET_SCANOUT and RESOURCE_FLUSH for primaries; GL scanout
  of host images; blobs and hostmem (028).
