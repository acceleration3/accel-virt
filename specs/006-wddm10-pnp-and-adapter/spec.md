# Feature Specification: WDDM 1.0: PnP, power and adapter lifetime

**Feature Branch**: `006-wddm10-pnp-and-adapter`

**Created**: 2026-09-10

**Status**: Draft

**Input**: `/dev` autonomous run after 005

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 006 · **Status**: `implement` · **Depends on**: 003, 005
· **WDDM level**: WDDM 1.0 (Windows Vista)

The adapter comes up and goes down cleanly: AddDevice through RemoveDevice,
power transitions, child enumeration, and the interrupt/DPC pair.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### DDI slots in scope (18)

| Slot | Field | Introduced |
|--:|---|---|
| 0 | `Version` | BASE |
| 1 | `DxgkDdiAddDevice` | BASE |
| 2 | `DxgkDdiStartDevice` | BASE |
| 3 | `DxgkDdiStopDevice` | BASE |
| 4 | `DxgkDdiRemoveDevice` | BASE |
| 5 | `DxgkDdiDispatchIoRequest` | BASE |
| 6 | `DxgkDdiInterruptRoutine` | BASE |
| 7 | `DxgkDdiDpcRoutine` | BASE |
| 8 | `DxgkDdiQueryChildRelations` | BASE |
| 9 | `DxgkDdiQueryChildStatus` | BASE |
| 10 | `DxgkDdiQueryDeviceDescriptor` | BASE |
| 11 | `DxgkDdiSetPowerState` | BASE |
| 12 | `DxgkDdiNotifyAcpiEvent` | BASE |
| 13 | `DxgkDdiResetDevice` | BASE |
| 14 | `DxgkDdiUnload` | BASE |
| 15 | `DxgkDdiQueryInterface` | BASE |
| 16 | `DxgkDdiControlEtwLogging` | BASE |
| 60 | `DxgkDdiLinkDevice` | BASE |

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
| `ObjectStorage<T>` | object | aligned bytes, constructed flag | Holds one object whose lifetime is started and ended explicitly |
| `NtStatus` | object | the NTSTATUS value | Carries a core Status across the DDI boundary as an NTSTATUS |
| `Miniport` | system | platform, the DDI table, a fixed array of adapters | Owns the driver's DDI table and adapter lifetimes |
| `MiniportBinding` | system | the one registered `Miniport` pointer | Lets context-less entry points (AddDevice, Unload) reach the root |
| `DxgkrnlInterface` | object | a copy of the callback table received at StartDevice | Calls dxgkrnl back on behalf of one adapter |
| `ResourceList` | object | a copy of the translated resource descriptors | Finds BARs and message-signalled interrupts in a CM resource list |
| `MappedRegisterWindow` | object | platform, mapped base address, length | Reads and writes one mapped register region through the platform |
| `Adapter` | object | platform, state, dxgkrnl interface, register windows, virtio transport, queues, power state | Carries one PCI device from AddDevice to RemoveDevice |
| `FakeDxgkrnl` (extended) | system | registered table, adapter context, fake device, callback records | Drives the real entry points through dxgkrnl's recorded sequences |

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

- [`docs/kb/02-kmd-contract.md`](../../docs/kb/02-kmd-contract.md)
- [`docs/kb/06-display.md`](../../docs/kb/06-display.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11

- Q (user): Where do the `DxgkDdi*` entry points live, given AddDevice and
  Unload carry no context? -> A: In `src/core/ddi/`, `extern "C"`, one line
  each, over layout-proven mirror types, so the fake dxgkrnl drives the real
  table and entry points on Linux. One registered pointer to the root
  (`MiniportBinding`) is the only process-lifetime state; constitution III's
  exception is amended (2.1.0, MINOR) to say so.
- Q (agent): How are WDK argument structs used in core? -> A: Hand-written
  mirrors with fixed-width fields; `src/km` asserts every field's offset and
  the size against the WDK header, as `ddi_table_layout.cpp` does.
- Q (agent): How does core touch mapped BARs? -> A: `Platform` gains register
  read/write at an address (`READ_REGISTER_*` in the kernel); core's
  `MappedRegisterWindow` wraps a mapped base. The mock routes addresses to the
  fake device's regions. `src/km/mmio_register_window` is removed.
- Q (agent): How many adapters, and how does the kernel construct objects with
  no global constructors and no `new`? -> A: A fixed array of 8 adapter slots
  in the root (one adapter per host GPU). `ObjectStorage<T>` - aligned storage
  with explicit construct/destroy - is the only place a placement-new
  expression may appear, enforced by `tools/check_design.py`; `DriverEntry`
  constructs the root in the shim's static storage, `Unload` destroys it.
- Q (agent): EDID and QueryAdapterInfo? -> A: `QueryDeviceDescriptor` returns
  "no more descriptor data" until feature 011 synthesizes EDID; QueryAdapterInfo
  is feature 007, so the recorded bring-up sequence runs up to the first
  QueryAdapterInfo call.

## User Scenarios & Testing *(mandatory)*

Users: every later WDDM feature, which adds DDIs to this skeleton; reviewers
checking the driver survives dxgkrnl's real call order.

### User Story 1 - The driver registers and its log reaches the host (Priority: P1)

**Independent Test**: `DriverEntry` builds the table from core, registers it
through `DxgkInitialize`, and its first log line is written to the host debug
port; the fake dxgkrnl accepts the same table under the recorded rules.

**Acceptance Scenarios**:

1. **Given** the harness binds a `Miniport` over the mock platform, **When**
   the fake dxgkrnl registers its table, **Then** registration succeeds with
   every slot 006 implements set and every other slot NULL.
2. **Given** the driver image, **When** it is inspected, **Then** `DriverEntry`
   calls `DxgkInitialize` and imports only kernel modules.
3. **Given** AddDevice or Unload is called with no bound root, **Then** it
   returns a failure status instead of dereferencing.

### User Story 2 - Adapter bring-up and teardown in dxgkrnl's order (Priority: P1)

**Independent Test**: the fake dxgkrnl runs AddDevice, StartDevice
(interrupt connected first, per `DpiFdoStartAdapter`), QueryChildRelations,
then StopDevice, RemoveDevice, Unload against a fake virtio-gpu device; the
device ends DRIVER_OK after start and reset after stop; nothing leaks.

**Acceptance Scenarios**:

1. **Given** AddDevice, **Then** an adapter context is returned and each call
   gets a distinct one up to the adapter limit, then `STATUS_NO_MEMORY`.
2. **Given** StartDevice with the recorded callback table and resources,
   **Then** the adapter reads config space, maps the virtio BARs, negotiates
   `VERSION_1` (plus `EDID`, `RESOURCE_BLOB`, `CONTEXT_INIT` when offered),
   activates control and cursor queues, sets DRIVER_OK, and reports one video
   present source and one child.
3. **Given** a device missing a required feature, a malformed config space, or
   no memory BAR, **Then** StartDevice fails with a specific NTSTATUS and
   leaves no mapping or allocation behind.
4. **Given** StopDevice after start, **Then** the device is reset and mappings
   released; a second StopDevice is harmless; RemoveDevice frees the slot.
5. **Given** SetPowerState to D3 then D0, **Then** the device is reset and
   brought back up.
6. **Given** ResetDevice (called at bugcheck, any IRQL), **Then** the device
   status is written to 0 with no allocation, lock or callback.

### User Story 3 - Interrupts, DPCs and the one child (Priority: P2)

**Acceptance Scenarios**:

1. **Given** an interrupt with ISR bits set, **When** InterruptRoutine runs,
   **Then** it claims the interrupt and queues a DPC; with no bits it does not
   claim it.
2. **Given** a DPC, **Then** used control-queue elements are harvested.
3. **Given** QueryChildRelations with room for one child, **Then** one video
   output child with interruptible HPD awareness is returned; with no room,
   `STATUS_BUFFER_TOO_SMALL`.
4. **Given** QueryChildStatus for connectivity, **Then** connected.
5. **Given** QueryDeviceDescriptor, **Then** `STATUS_MONITOR_NO_MORE_DESCRIPTOR_DATA`.

### User Story 4 - Everything else in the slot list answers correctly (Priority: P3)

**Acceptance Scenarios**:

1. DispatchIoRequest, QueryInterface: `STATUS_NOT_SUPPORTED`.
2. NotifyAcpiEvent: `STATUS_SUCCESS`, no action; ControlEtwLogging: no action.
3. LinkDevice: not linked (one adapter, not part of a chain).

### Edge Cases

- StartDevice for a context the root did not create, or twice: failure status.
- A resource list with zero descriptors or a truncated partial list: rejected.
- Fewer MSI-X messages than queues: all queues share vector 0.
- An interrupt before StartDevice finished (dxgkrnl connects first): not claimed.
- Stop while a control request is in flight: device reset, queue memory released.

## Requirements *(mandatory)*

- **FR-001**: Entry points MUST be `extern "C"`, one statement, in `src/core/ddi/`.
- **FR-002**: `MiniportBinding` MUST be the only process-lifetime state; unbound
  entry points MUST fail safely.
- **FR-003**: Every mirror MUST be layout-proven against the WDK in `src/km`.
- **FR-004**: The resource list MUST be treated as variable-length input:
  bounded, validated, fuzzed.
- **FR-005**: Bring-up MUST use the feature 005 transport and follow
  `docs/ddi-sequences.md`; teardown MUST release every mapping and allocation.
- **FR-006**: ISR and ResetDevice paths MUST not allocate, block or lock.
- **FR-007**: The fake dxgkrnl MUST run bring-up and teardown in the recorded
  order, plus StopDevice twice, RemoveDevice without StopDevice, and an
  interrupt before start.
- **FR-008**: `DriverEntry` MUST log to the host debug port before registering.

## Success Criteria *(mandatory)*

- **SC-001**: The fake dxgkrnl scenario passes with no leak, violation or unexpected register access.
- **SC-002**: 100% branch coverage; resource-list fuzz target clean.
- **SC-003**: The driver image imports only `ntoskrnl.exe`/`hal.dll` and contains `DxgkInitialize`.

## Assumptions

- The virtio-gpu device is the modern PCI one; control queue 0, cursor queue 1.
- The host debug port is QEMU `isa-debugcon` at 0x402 (`docs/kb/11-implementation-plan.md` M0).

## Out of Scope

- QueryAdapterInfo (007), VidPn and EDID (011), present (012).
- Loading in a real guest (M0 in L5, feature 035).
