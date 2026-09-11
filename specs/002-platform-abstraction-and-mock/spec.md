# Feature Specification: Platform abstraction and adversarial mock

**Feature Branch**: `002-platform-abstraction-and-mock`

**Created**: 2026-09-10

**Status**: Draft

**Input**: `/dev 002`

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 002 · **Status**: `implement` · **Depends on**: 001

src/core/platform.h and its two implementations: the Windows kernel shim and
the test mock. The mock simulates IRQL, fails allocations on demand, tracks
leaks and lock order, and controls time.

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
| `SpinLock` | object | `Platform &`, kernel lock word, `LockRank` | Serializes DISPATCH-reachable state under one rank |
| `SpinLockGuard` | object | `SpinLock &`, previous `Irql` | Holds one spin-lock acquisition for a scope |
| `Mutex` | object | `Platform &`, 64-byte kernel mutex storage, `LockRank` | Serializes PASSIVE state that may block under one rank |
| `MutexGuard` | object | `Mutex &` | Holds one mutex acquisition for a scope |
| `KernelPlatform` (`src/km`) | system | pool tag | Translates each platform call into one kernel call |
| `MockPlatform` (`tests/harness`, extended) | system | allocation table, IRQL, lock stack, violation counts, clock | Enforces the platform rules inside a unit test |

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

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11 (autonomous run: decided by the agent, not asked)

- Q: The standard asks for a virtual destructor on a polymorphic base; the
  kernel has no `operator delete`. -> A: `Platform` has a protected non-virtual
  destructor. A probe link proved a public virtual destructor emits a deleting
  destructor that references `operator delete(void *, unsigned __int64)` and
  fails to link as a driver. Nothing deletes a platform through a base pointer.
  Recorded as a freestanding adaptation in `CLAUDE.md`.
- Q: Where are IRQL, blocking and lock-order rules enforced? -> A: In the mock,
  in every unit test. The kernel implementation translates only; Driver
  Verifier is the kernel-side enforcement. Constitution VII's "checked at
  runtime in debug builds" is read as the native test build; a per-thread lock
  stack inside the kernel is not built.
- Q: Abort or record on a violation? -> A: Record. A violation the test did not
  explicitly claim fails the test when the mock is destroyed. An abort would
  make "the violation is detected" untestable, and recording without failing
  lets a test pass while ignoring one.
- Q: Which primitives? -> A: Pool allocation, release, zero, copy, current
  IRQL and its ceiling, spin lock, blocking mutex, monotonic interrupt time,
  log, broken-invariant report. Events, timers, DPCs and stalls arrive with the
  first feature that calls them (Principle VIII).
- Q: Lock ranks for locks that do not exist yet? -> A: The rank type is
  declared once; each named rank is added by the feature that introduces the
  lock. Tests use explicit rank values.

## User Scenarios & Testing *(mandatory)*

The users are the developers writing `src/core`, and every core test.

### User Story 1 - A driver bug that would bugcheck a VM fails a unit test instead (Priority: P1)

A developer writes core code that allocates, blocks, or takes locks at the
wrong IRQL, takes two locks in the wrong order, releases memory it never had,
or leaks. The unit test for that code fails, naming the function and the rule,
on Linux, in seconds - not a bugcheck in a guest.

**Why this priority**: This is the reason the platform is injected at all
(`docs/testing.md`, L1). Without it every IRQL and locking rule is a comment.

**Independent Test**: For each rule, a core operation that breaks it under the
mock fails the enclosing test unless the test claims that exact violation.

**Acceptance Scenarios**:

1. **Given** simulated IRQL is DISPATCH, **When** core allocates, **Then** the
   allocation fails and an allocation-at-dispatch violation is recorded naming
   the caller's ceiling function.
2. **Given** simulated IRQL is DISPATCH, **When** core acquires a blocking
   mutex, **Then** a blocking-at-dispatch violation is recorded.
3. **Given** a lock of rank 20 is held, **When** core acquires a lock of rank
   10 or 20, **Then** a lock-order violation is recorded naming both ranks.
4. **Given** two locks are held, **When** the first-acquired is released
   first, **Then** a release-order violation is recorded.
5. **Given** a function declares an IRQL ceiling of PASSIVE, **When** it is
   called at DISPATCH, **Then** an IRQL violation naming the function is
   recorded.
6. **Given** any violation was recorded and not claimed, **When** the mock is
   destroyed, **Then** the test fails and lists each unclaimed violation.
7. **Given** a violation was claimed by the test, **When** the mock is
   destroyed, **Then** the test is not failed for it.
8. **Given** core releases a null pointer, **When** release is called,
   **Then** a null-release violation is recorded (the kernel bugchecks on it).
9. **Given** a lock is still held or memory is still allocated, **When** the
   mock is destroyed, **Then** the test fails naming the count.

---

### User Story 2 - Core has locks it can take correctly at every IRQL (Priority: P1)

A developer needs to serialize state in core: a spin lock for code reachable at
DISPATCH, a mutex for PASSIVE code that may allocate. Each is an object holding
its own rank; a scope-bound guard acquires and releases it, restoring IRQL, so
no path can return with a lock held.

**Why this priority**: Every system class from feature 005 on needs one, and
the lock-order rule needs a lock to check.

**Independent Test**: Guarded acquisition under the mock raises IRQL to the
primitive's level, restores it on scope exit on every return path, and
produces no violation when ranks increase.

**Acceptance Scenarios**:

1. **Given** PASSIVE, **When** a spin-lock guard is in scope, **Then**
   simulated IRQL is DISPATCH, and after the scope it is PASSIVE again.
2. **Given** DISPATCH already, **When** a spin-lock guard is taken and
   released, **Then** IRQL returns to DISPATCH, not PASSIVE.
3. **Given** PASSIVE, **When** a mutex guard is in scope, **Then** IRQL is APC
   and returns to PASSIVE afterwards.
4. **Given** ranks 10 then 20, **When** guards nest in that order, **Then** no
   violation is recorded.
5. **Given** a function with an early return inside a guard's scope, **When**
   it returns early, **Then** the lock is released.

---

### User Story 3 - Time in tests is exact (Priority: P2)

A developer tests timeout logic (TDR, feature 010) and needs time to advance
exactly when the test says, never with the wall clock.

**Independent Test**: Interrupt time starts at a known value, only changes when
the test advances it, and never goes backwards.

**Acceptance Scenarios**:

1. **Given** a fresh mock, **When** time is read twice, **Then** both reads are
   equal.
2. **Given** a read of T, **When** the test advances by D, **Then** the next read
   is T + D.
3. **Given** an advance that would overflow, **When** it is requested, **Then**
   time saturates at the maximum and a violation is recorded.

---

### User Story 4 - The same core runs on the real kernel through one translation (Priority: P2)

The driver build contains a kernel platform that maps each primitive to exactly
one kernel call and carries no decision of its own, so what the mock proves is
what the kernel executes.

**Independent Test**: The kernel build compiles and links it; it contains no
conditional branch; the opaque storage core reserves for a mutex is proven
large enough and aligned for the kernel's mutex at compile time.

**Acceptance Scenarios**:

1. **Given** the driver build, **When** it links, **Then** every kernel routine
   the platform uses resolves against the kit's kernel import libraries.
2. **Given** the kernel's mutex type, **When** the driver compiles, **Then** a
   compile-time check proves core's opaque mutex storage fits and is aligned.
3. **Given** a pool selector, **When** it is translated, **Then** the kernel
   pool flag comes from a table, not a branch.

### Edge Cases

- Allocation of zero bytes: fails, no violation (a caller bug is a returned
  error, not a rule break); the kernel rejects zero-byte pool requests too.
- Fail-at-N allocator with N beyond the number of allocations: no failure.
- Releasing a pointer the mock never handed out: memory corruption; the test
  aborts immediately (continuing would free a foreign pointer).
- Lock stack deeper than the mock's fixed capacity: violation recorded, lock
  still acquired, depth not tracked further.
- Releasing a lock that is not held at all: release-order violation.
- IRQL left raised at mock destruction by a test that set it: not a violation
  (tests simulate DDI entry IRQL); a guard never leaves it raised.
- `GpuVirtualAddressSpace` destroyed before `initialize`, or after a failed
  first allocation: must not release a null table.
- A fuzz target: any unclaimed violation after an input traps, so the fuzzer
  reports it as a crash with the input.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The platform contract MUST offer: allocate and release by pool,
  zero, copy, current IRQL, declare an IRQL ceiling, acquire and release a spin
  lock, initialize, acquire and release a blocking mutex, read monotonic
  interrupt time, log a line, and report a broken invariant.
- **FR-002**: The platform contract MUST NOT be destroyable through a base
  pointer, and MUST link into the driver image.
- **FR-003**: Core MUST provide a spin lock object and a mutex object, each
  carrying a lock rank, and a scope guard for each that releases on every exit
  path and restores the previous IRQL.
- **FR-004**: Lock ranks MUST be declared in exactly one place in core.
- **FR-005**: The mock MUST record, as distinct kinds: IRQL-ceiling violation,
  allocation at DISPATCH or above, blocking at DISPATCH or above, lock acquired
  out of strictly increasing rank order, lock released out of LIFO order or not
  held, lock stack overflow, null release, broken invariant, time overflow.
- **FR-006**: Each recorded violation MUST be printed once when it happens,
  with the function or ranks that caused it.
- **FR-007**: A test MUST be able to claim a specific violation kind; claiming
  removes one recorded occurrence and reports whether one existed.
- **FR-008**: On destruction the mock MUST fail the current test for every
  unclaimed violation, every live allocation, and every held lock.
- **FR-009**: The mock MUST simulate IRQL: spin-lock acquire raises to DISPATCH,
  mutex acquire raises to APC, release restores the saved level.
- **FR-010**: The mock MUST fail the Nth allocation on request and count live
  allocations, as today.
- **FR-011**: The mock MUST keep deterministic interrupt time that only a test
  advances.
- **FR-012**: The kernel platform MUST be translation only (no conditional
  branch), own its pool tag as state, and prove the mutex storage size and
  alignment at compile time.
- **FR-013**: Every fuzz target MUST trap when an input leaves an unclaimed
  violation.
- **FR-014**: `GpuVirtualAddressSpace` MUST NOT release a null table.
- **FR-015**: `ScopeExit`, `InvariantScope`, `Status` and the untestable mark
  MUST each live in their own header (one class per header).
- **FR-016**: Every existing test that checked a violation flag MUST claim the
  violation instead, and the existing suite MUST keep 100% branch coverage.

### Key Entities

- **Platform** - the injected contract core calls; two implementations.
- **Violation** - a recorded rule break: kind, detail, claimed or not.
- **Lock rank** - an ordering value; acquisition must strictly increase it.
- **Spin lock / mutex** - core-owned lock objects with a rank and opaque
  kernel storage.
- **Guard** - a scope object owning one acquisition.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Each of the nine violation kinds is demonstrated by a test that
  provokes it and claims it, and by a test showing that leaving it unclaimed
  fails.
- **SC-002**: Zero tests in the suite pass while a violation goes unnoticed:
  running the suite reports no unclaimed violations.
- **SC-003**: The null-table release in the address space is caught by a test
  that fails before the fix and passes after.
- **SC-004**: The driver image still passes its image gate, now containing the
  kernel platform.
- **SC-005**: Branch coverage of `src/` stays at 100% with no new untestable
  marks.

## Assumptions

- Tests are single-threaded; the mock's lock stack models one thread of
  execution, which is the scenario unit tests exercise. Concurrency is L2/L5.
- The kernel mutex is `FAST_MUTEX` (raises to APC, not reentrant); spin locks
  use `KeAcquireSpinLockRaiseToDpc`; time is `KeQueryInterruptTime` (100 ns).
- Kernel logging is `DbgPrintEx`; routing the log to the host from
  `DriverEntry` onward (isa-debugcon) is feature 006.
- The kernel platform is compiled and linked by the driver build but not yet
  instantiated; `DriverEntry` constructs it in feature 006.

## Out of Scope

- Refcount balance tracking (feature 008, with the objects that own refcounts).
- Events, timers, DPCs, work items, stalls, MMIO and port I/O primitives.
- Lock-order checking inside the kernel build.
- The DDI conformance harness (003).
- Follow-up constitution PATCH amendment, adding to 001's list: VII names
  `src/core/locks.h`; the declaration is `src/core/lock_rank.hpp`, and "checked
  at runtime in debug builds" means the native test build.
