# Feature Specification: QEMU device: decode and execute

**Feature Branch**: `032-device-execution-and-codec`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 032 · **Status**: `implement` · **Depends on**: 031

Bind render-server's generated decoders to a host dispatch table and execute
on host Vulkan/GL/CL. The decoders are the primary attack surface of the whole
system.

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
| `ContextSetup` (extended) | object | + reply resource | Encodes one context setup, now naming its reply slot |
| `CommandHeader` | object | command id, total size, flags | Encodes the 8-byte header in front of each command in a batch |
| `ReplyHeader` | object | status, fence id, reply size, required size | Encodes the 32-byte header of a context's reply slot |
| `ExecutorLibrary` | object | the executor operations table, whether it was accepted | Tells what one loaded executor library can execute |
| `ContextExecutor` | object | library, the executor handle, sub-API, GPU UUID, created flag | Runs one context's commands on its host API |
| `CommandStream` | object | the staged payload bytes, read offset | Splits one batch payload into commands, bounded by the payload |
| `BatchStaging` | object | host, a host block of 2^order bytes and one for replies | Holds the copies of one context's batch and reply the worker works on |
| `BatchExecutor` | object | context executor, staging, reply window, fence of the reply, counters | Executes one context's batches command by command, then writes its reply |
| `RingConsumer` (extended) | object | + `BatchExecutor&` | Consumes one context's complete batches on its worker |
| `Worker` (extended) | object | - | + detaches its ring on its own thread before exiting |
| `DeviceCapset` (extended) | object | - | Describes the sub-APIs the executor library offers |
| `DeviceContext` / `ContextBinding` (extended) | object | + reply resource pin, `BatchExecutor`, staging | One guest API context on the device |
| `VirtioGpuRvk` (extended) | object | + `ExecutorLibrary` | One virtio-gpu-rvk device instance |
| `MockExecutorOps` (harness) | object | scripted answers, created and destroyed executors, calls by thread | The executor C table for tests |

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

### Session 2026-09-11

- Q (agent, verified): Is `librvk_codec` available to link? -> A: No.
  `~/Coding/render-server` has the seam this needs - `rvk::api::ServerBackend::dispatch(command_id, reader, reply)`,
  one backend per API - but builds it as C++ static libraries with the
  standard library and exceptions, over ivshmem regions, and exports no C ABI
  and no pkg-config file. Building it is render-server's work (and its M4), not
  this repository's, and nothing outside this repository may be referenced.
- Q (agent): Then how does the device execute? -> A: The device owns the
  contract it needs, as it owns `RvkHostOps`: `rvk_executor.h` declares
  `RvkExecutorOps`, the one table an executor library exports as
  `rvk_executor_ops`. The glue `dlopen`s the library the `executor` property
  names at realize and hands the table to the core; render-server's future
  `librvk_codec` implements it over `ServerBackend`. Without an executor the
  device has no capset: no `VIRTIO_GPU_F_CONTEXT_INIT`, 2D and blobs only -
  no sub-API is reported that nothing executes (constitution VI). A named
  library that cannot be loaded, lacks the symbol or has another ABI version
  fails realize with the reason.
- Q (agent): How are tests and the qtest suites run without render-server? ->
  A: Unit tests use `MockExecutorOps`, a scripted C table in the harness. The
  qemu configuration builds `rvk_echo_executor`, a test library that executes
  a small command set (echo, fail to decode, lose the device) for the Vulkan
  sub-API, and every qtest device that uses contexts names it.
- Q (agent): The command framing? -> A: render-server's
  (`rvk/wire/command_header.hpp`, `api_id.hpp`): each command is `u32 id`,
  `u32 size` with flags in the top 8 bits (bit 0: generate a reply) and `size`
  covering the 8-byte header; the id's top 16 bits name the API (0 transport,
  1 Vulkan, 2 OpenCL, 3 OpenGL). The device parses the framing itself; the
  executor decodes the payload. Commands must fill the batch payload exactly.
- Q (agent): Unknown commands? -> A: Survivable, as in render-server: a
  transport id (none are defined yet), an id of an API other than the
  context's, or one the executor does not know is skipped and counted.
  A payload the executor cannot decode, or a lost host device, stops the
  context (033 turns stops into faults).
- Q (agent): Replies? -> A: The setup names a reply resource at offset 28
  (formerly reserved; 0 means the context sends no reply-generating command).
  The device writes the reply of the last reply-generating command of a batch
  into the blob before it publishes head past the batch, so a producer that
  waits for head to pass reads a finished reply, as in render-server. An answer
  larger than the slot is not truncated: the slot says how much was needed.
- Q (agent): Copying? -> A: Each batch's payload is copied once from the ring
  into a staging block of `2^order` bytes the context holds from its setup, and
  the executor reads only the copy; it writes its answer into a staging block of
  the reply slot's size, which the device copies into the blob. Nothing
  allocates on the worker.
- Q (agent): Which thread creates and destroys the host executor? -> A: The
  worker, on attach and just before it exits: a GL context must be current on
  the thread that uses it, and creating a Vulkan device must not hold QEMU's
  big lock. A failed creation stops the context.

## User Scenarios & Testing *(mandatory)*

<!-- Written by /speckit-specify or by hand. The autogen block above is regenerated by tools/gen_specs.py; everything below is preserved. -->

Users: the guest's API drivers (render-server's ICDs moved onto this device,
M4) and the executor library that runs their commands on the host GPU. A
context's batches stop being consumed-and-dropped: each is copied, split into
commands, run through the context's host executor in order, answered, and only
then completed.

### User Story 1 - Commands run on the host executor in order (Priority: P1)

**Independent Test**: over qtest with the echo executor, send a batch of echo
commands, one generating a reply, and read the reply slot once head passes.

**Acceptance Scenarios**:

1. **Given** a device realized with an executor offering Vulkan, **Then** its
   capset offers Vulkan and context init; **given** no executor, no capset.
2. **Given** a bound Vulkan context, **When** a batch of commands arrives,
   **Then** the executor sees each command's id and payload, in order, on the
   context's worker, created there on attach and destroyed there on exit.
3. **Given** a reply-generating command, **Then** before head passes its
   batch the reply slot reads status ok, the batch's fence, the answer's size and
   bytes; an answer larger than the slot reads status too-large and the size
   it needed.
4. **Given** unknown commands (transport ids, another API's, ones the executor
   does not know), **Then** they are skipped and counted, and later commands run.

### User Story 2 - Malformed streams stop only their context (Priority: P1)

**Acceptance Scenarios**:

1. **Given** a command whose size is below 8 or runs past the batch, reserved
   flag bits, trailing bytes too short for a header, a reply-generating command
   on a context with no reply slot, a payload the executor cannot decode, a
   lost device, or an executor that could not be created, **Then** the context
   stops (logged once) and its fence does not advance; other contexts run.

### User Story 3 - The setup names a reply slot (Priority: P2)

1. A reply resource of 0 or a blob of at least 64 bytes other than the ring
   and fence blobs is accepted and pinned; a missing resource is
   `ERR_INVALID_RESOURCE_ID`, a smaller blob or one shared with the ring or
   fence is `ERR_INVALID_PARAMETER`.

### Edge Cases

- A batch with no commands (a zero-length payload): completes, nothing runs.
- A batch whose commands generate no reply leaves the slot untouched.
- Several reply-generating commands in one batch: the last one's answer is kept.
- Executor calls never happen on the main loop.

## Requirements *(mandatory)*

- **FR-001**: `rvk_executor.h` defines the executor ABI (version 1); the core
  refuses a table of another version or with a missing operation.
- **FR-002**: The capset's `sub_apis` are the executor's offered sub-APIs that
  the protocol knows; with none, the device offers no capset.
- **FR-003**: Every command header is validated against the staged copy of the
  payload before its payload reaches the executor.
- **FR-004**: The worker neither allocates nor touches the main loop while
  executing; staging is allocated at setup.
- **FR-005**: `docs/protocol.md` documents commands, the reply slot, the setup's
  reply field and the executor ABI's contract.

## Success Criteria *(mandatory)*

- **SC-001**: The qtest suite runs echo batches on the real device and reads
  replies, and checks every malformed stream stops only its context.
- **SC-002**: 100% branch coverage of the new core; `fuzz_command_stream` clean.

## Out of Scope

- `librvk_codec` itself, and host-visible memory for executors (render-server, M4).
- Faults reported in the fence page and TDR (033).
