# Feature Specification: DDI conformance harness (fake dxgkrnl)

**Feature Branch**: `003-ddi-conformance-harness`

**Created**: 2026-09-10

**Status**: Draft

**Input**: `/dev` autonomous run after 002

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 003 · **Status**: `implement` · **Depends on**: 002

A fake dxgkrnl that builds DRIVER_INITIALIZATION_DATA, calls DriverEntry, and
drives the DDI table through sequences derived from reverse engineering the
real dxgkrnl.sys, including the abusive-but-legal sequences a naive driver
forgets.

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
| `DdiTable` | object | 32-bit version, padding, 187 routine pointers (the WDK layout) | Holds the routines registered with dxgkrnl |
| `FakeDxgkrnl` (`tests/harness`) | system | the accepted table, the rule that last failed | Applies dxgkrnl's registration validation to a table |

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

- [`docs/kb/02-kmd-contract.md`](../../docs/kb/02-kmd-contract.md)
- [`docs/kb/10-warp-kmd.md`](../../docs/kb/10-warp-kmd.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11 (autonomous run: decided by the agent, not asked)

- Q: How does a Linux test process drive a DDI table whose types come from
  Windows headers? -> A: Core owns a byte-exact mirror of
  `DRIVER_INITIALIZATION_DATA` built from fixed-width types. `src/km` proves
  at compile time, against the real WDK header, that the mirror's size and
  every one of the 188 slot offsets match, so the table core builds is the
  table `dxgkrnl` reads. The harness then runs natively under ASan and UBSan.
  Rejected: running the shim under Wine (no sanitizers, kernel shared-data
  reads fault in user mode, every kernel import needs a fake) and compiling
  WDK headers for Linux (`long` is 64-bit there, so every `ULONG` layout and
  the headers' own `C_ASSERT`s break).
- Q: What can the harness drive before any DDI exists? -> A: Registration -
  the validation `dxgkrnl` applies to a `DRIVER_INITIALIZATION_DATA`, derived
  from `dxgkrnl.sys` with citations - and nothing else yet. The scenario
  runner that calls DDIs in sequence arrives with the first DDIs (feature 006),
  which is also when the argument-struct mirrors it needs exist (Principle
  VIII). The sequences themselves are researched now and written down, so 006
  implements against evidence.
- Q: Where do the `DxgkDdi*` entry points live, given the harness must call
  the real ones? -> A: Decided in feature 006 with the first entry point; this
  feature does not need one. Recorded as an open design question there.

## User Scenarios & Testing *(mandatory)*

Users: developers implementing WDDM features 006-025, and reviewers checking
that the driver's registration and call sequences match what `dxgkrnl` does.

### User Story 1 - The DDI table core builds is byte-identical to the one dxgkrnl reads (Priority: P1)

A developer fills DDI slots in core. The build proves the core table's layout
equals the WDK's `DRIVER_INITIALIZATION_DATA` at WDDM 3.2 - size and every slot
offset - so a slot set in a Linux test is the slot `dxgkrnl` calls in the guest.

**Why this priority**: every later feature puts function pointers in this
table; a one-slot offset error calls the wrong routine with the wrong
arguments, which surfaces as an unrelated bugcheck.

**Independent Test**: change one slot's position in the core mirror; the
driver build fails naming that slot.

**Acceptance Scenarios**:

1. **Given** the core table and the WDK header, **When** the driver builds,
   **Then** a compile-time check per slot proves its offset, and the total
   size matches.
2. **Given** a new table, **When** it is created, **Then** its version is
   `0x11007` and every routine slot is empty.
3. **Given** a slot, **When** a routine is set, **Then** reading that slot
   returns it and no other slot changes.
4. **Given** the version slot or an index past the last slot, **When** a
   routine is set there, **Then** the call is rejected and the table is
   unchanged.
5. **Given** the slot list in `docs/ref/driver_init_table.md`, **When** the
   generated slot names are regenerated, **Then** they match what is committed.

---

### User Story 2 - The harness rejects a table exactly where dxgkrnl would (Priority: P1)

A developer registers the driver's table with the fake `dxgkrnl`. It applies
the checks the real `DxgkInitialize` path applies - accepted versions, the
slots required for a render-capable driver at the declared version, anything
else it validates - and fails with the same outcome, citing the `dxgkrnl`
function each rule came from.

**Why this priority**: a table `dxgkrnl` rejects produces a device that never
starts and no diagnostic; catching it on Linux is the harness's first job.

**Independent Test**: for each rule, a table breaking only that rule is
rejected with that rule named; the table satisfying all rules is accepted.

**Acceptance Scenarios**:

1. **Given** a version `dxgkrnl` does not accept, **When** the table is
   registered, **Then** registration fails naming the version rule.
2. **Given** a table missing one slot `dxgkrnl` requires, **When** it is
   registered, **Then** registration fails naming that slot.
3. **Given** every required slot filled, **When** it is registered, **Then**
   registration succeeds and the harness keeps the table for later scenarios.
4. **Given** each rule, **When** reviewed, **Then** its source is a named
   `dxgkrnl.sys` function recorded in `docs/ddi-sequences.md`.

---

### User Story 3 - The sequences dxgkrnl drives are written down with evidence (Priority: P2)

A developer starting feature 006 reads, for adapter bring-up and teardown, the
order `dxgkrnl` calls DDIs, the IRQL of each call, and the abusive-but-legal
variations, each citing the `dxgkrnl.sys` function it was read from.

**Independent Test**: every sequence step and every abusive variation in the
document names a function present in the pinned `dxgkrnl.sys`.

**Acceptance Scenarios**:

1. **Given** the document, **When** a step is questioned, **Then** its cited
   function can be decompiled in `re/ghidra-proj` and shows the call.
2. **Given** `docs/testing.md`'s list of abusive sequences, **When** the
   document is read, **Then** each is either confirmed possible with a
   citation or marked not observed.

### Edge Cases

- A table whose version is older than WDDM 3.2 but otherwise valid: registration
  accepts any version from 0x104E (`DpiInitializeInternal`); any rejection of an
  old level on Windows 11 happens after registration, not in it. The harness
  mirrors that, and the driver never declares anything but 0x11007.
- Registering twice: the second registration fails with
  `STATUS_OBJECT_NAME_COLLISION` from `IoAllocateDriverObjectExtension` and the
  first stays registered (`DpiInitializeEx`).
- A null table pointer: `dxgkrnl` would dereference it; the harness refuses it
  instead.
- A slot index equal to the slot count: rejected by the table.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Core MUST provide a DDI table object with the exact layout of
  `DRIVER_INITIALIZATION_DATA` at `DXGKDDI_INTERFACE_VERSION_WDDM3_2`: a 32-bit
  version, padding to 8, then 192 pointers (187 DDI routines, 5 reserved).
- **FR-002**: The table MUST start at version `0x11007` with every routine
  empty, allow setting and reading a routine by named slot, and reject the
  version slot and out-of-range slots without changing the table.
- **FR-003**: The named slots MUST be generated from
  `docs/ref/driver_init_table.md`, and a test MUST fail when the generated file
  is stale.
- **FR-004**: The driver build MUST assert, per slot, that the core table's
  offset equals the WDK field's offset, and that the sizes are equal.
  (193 fields: the reference table originally dropped the 5 reserved ones.)
- **FR-005**: The fake `dxgkrnl` MUST implement registration validation rule
  by rule as recovered from `dxgkrnl.sys`, report the failing rule, and keep an
  accepted table.
- **FR-006**: Each validation rule MUST cite its `dxgkrnl.sys` function in
  `docs/ddi-sequences.md`.
- **FR-007**: `docs/ddi-sequences.md` MUST record adapter bring-up and
  teardown DDI order, per-call IRQL, and the abusive-but-legal variations from
  `docs/testing.md`, each with a citation or "not observed".

### Key Entities

- **DDI table** - version plus 192 pointer slots (187 DDIs, 5 reserved), layout-identical to the WDK.
- **DDI slot** - a named index into the table, generated from the reference table.
- **Registration rule** - one check `dxgkrnl` applies, with its citation.

## Success Criteria *(mandatory)*

- **SC-001**: Moving any one slot in the core table breaks the driver build
  with that slot's name.
- **SC-002**: Every registration rule has a rejecting and an accepting test.
- **SC-003**: 100% of rules and sequence steps cite a function present in the
  pinned `dxgkrnl.sys`.
- **SC-004**: Branch coverage stays 100% with no new untestable marks.

## Assumptions

- The pinned `dxgkrnl.sys` 10.0.26100.9444 is the behaviour of record.
- `displib.lib`'s `DxgkInitialize` forwards the table to `dxgkrnl` without
  validating it; validation happens inside `dxgkrnl.sys`.

## Out of Scope

- Calling any DDI, argument-struct mirrors, and the scenario runner (006).
- Where the `DxgkDdi*` entry points live (decided in 006).
- Changing `DriverEntry` - it still declines to load.
