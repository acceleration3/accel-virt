# Feature Specification: OpenCL: the ICD on rvkgpu

**Feature Branch**: `052-opencl-icd`

**Created**: 2026-09-11

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 052 · **Status**: `implement` · **Depends on**: 044

This project's generated OpenCL ICD over the guest transport, registered
through the adapter's driver registration: OpenCL 3.0 with the host device's
optional features, coarse-grained shared virtual memory over host-visible
memory, non-blocking transfers and event callbacks that cannot deadlock, and
one platform device per rvkgpu adapter.

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

Guest user mode, `src/icd/opencl/` (namespace `av::clicd`):

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `ClIcdRoot` | system | the injected seams, the dispatch table, the platform, the object table, the connection, the extension policy, the watcher | Answer the ICD entry points the loader reaches |
| `ClIcdBinding` | object | the one pointer to the root | Reach the root from an entry point handed no context |
| `ClDispatchTable` | object | one `cl_icd_dispatch`, this ICD's entries in it | Present one filled dispatch table to the loader |
| `ClPlatform` | object | the dispatch pointer, the suffix, the version, the extension string | Be the platform the loader enumerates |
| `ClObject` | object | the dispatch pointer, its kind, its wire handle, its owning context | Be one object the application holds |
| `ClObjectTable` | system | every live `ClObject`, by kind, with its generation | Judge an object pointer that came from the application |
| `GuestClContext` | object | its link, staging, transport, blob arena, SVM table, mappings, state | Be one `cl_context` |
| ~~`ClConnection`~~ | - | - | Not written: `GlConnection` was already this, and moved to `src/icd/common/` as `icd::IcdConnection` for both ICDs |
| `ClDeviceReport` | object | the adapter's LUID, the version cap, the capabilities this feature tests | Answer the device queries this ICD decides rather than forwards |
| `ClExtensionPolicy` | object | the reportable rows and the test each names | Decide which extensions the application sees |
| `SvmAllocation` | object | host base, guest mirror, bytes, map depth | Be one coarse-grained SVM allocation |
| `SvmTable` | system | the SVM allocations of one context | Translate a mirror address into its host address |
| `MappedRegion` | object | the buffer, the range, the arena lease or the staged block | Be one live mapping of a buffer |
| `ClEventWatcher` | system | the registered callbacks, the service thread, the transport it asks | Run an event's callbacks once the host reaches it |

Shared seam, `src/icd/common/` and `src/um/icd/`:

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `icd::ServiceThread` | object (interface) | nothing; two implementations own the thread | Run one loop until it is asked to stop |
| `WindowsServiceThread` | object | the thread handle and its stop event | Run that loop on a Windows thread |

Device and executor, `src/device/`:

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `HostSvmRanges` | system | the SVM allocations one session made | Say whether an address and extent lie inside one of them |
| `ClEventSignals` | system | the host events registered, and which have completed | Record a host event's completion for the worker to read |
| `ContextSignal` | object | an any-thread flag and the worker's wake event | Wake a context's worker from a thread that is not it |

Changed, not new: `FenceReporter` (a `nudge()` that raises with `progress`
moved and `completed_fence` unchanged), `Worker` (an any-thread wake beside its
main-loop-only `post`), `OpenClBackend` (the ten commands, the host callback
trampoline, the SVM ranges), `ExecutorLibrary`/`ContextExecutor` (a
`names_events()` gate), the executor ABI (version 5, the first device-supplied
callback), `src/proto/icd_stub_report.hpp` (`stub_called`), and the OpenCL
generator (an address return, `Role::SVM_POINTER`, a `svm_for` server hook, a
callback trampoline hook).

Test harness, `tests/harness/icd/`: `FakeClLoader`, `ClIcdRig`, `ClObjectRig`,
`ScriptedThread`, and `ScriptedHost`'s new encoders.

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

- [`docs/kb/07-usermode.md`](../../docs/kb/07-usermode.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## User Scenarios & Testing *(mandatory)*

<!-- Written by /speckit-specify or by hand. The autogen block above is regenerated by tools/gen_specs.py; everything below is preserved. -->

Users: people running OpenCL applications in the guest - compute tools, image
and video processing, scientific code, and runtimes that pick a GPU through the
Khronos ICD loader - who expect the host GPU to appear as an OpenCL device of
the rvkgpu adapter, found without manual registry edits.

Starting point: render-server's OpenCL driver is already an ICD
(`clIcdGetPlatformIDsKHR`) reporting the host's OpenCL 3.0 with allowlisted
extensions and `cl_khr_gl_sharing`, registered under a global Khronos vendors
key. Maps are copies in and out, SVM is coarse-grained over blob ranges with
host mirrors, transfers naming host memory are forced blocking and can
deadlock when gated on a user event from the same guest, and event callbacks
run in the guest.

### User Story 1 - The Khronos loader finds rvkgpu's OpenCL device (Priority: P1)

**Independent Test**: on Linux, the ICD core is driven over MockKmt and the
loopback rig with a fake loader calling `clIcdGetPlatformIDsKHR`,
`clGetPlatformInfo` and `clGetDeviceIDs`; in the guest, `rvkclprobe.exe`
enumerates platforms through the pushed `OpenCL.dll` and runs a kernel.

**Acceptance Scenarios**:

1. **Given** the package installed, **When** an application calls
   `clGetPlatformIDs`, **Then** the Khronos loader finds the ICD through the
   adapter's `OpenCLDriverName` registration, and one device appears per
   rvkgpu adapter.
2. **Given** a device, **When** `CL_DEVICE_NAME`, `CL_DEVICE_VENDOR_ID`,
   `CL_DEVICE_VERSION` and `CL_DEVICE_LUID_KHR` are queried, **Then** they name
   the host GPU the adapter is bound to, the version is the host's capped at
   3.0, and the LUID matches the adapter's.
3. **Given** a program from source or SPIR-V, **When** it is built and a
   kernel is enqueued, **Then** it runs on the host GPU and results read back
   correctly.

### User Story 2 - Memory: buffers, maps and SVM over host-visible memory (Priority: P1)

**Independent Test**: loopback tests allocate buffers with every
`CL_MEM_*` flag combination, map and unmap them, and use SVM allocations
across kernels, with invariants after each step and hostile sizes and offsets.

**Acceptance Scenarios**:

1. **Given** host-visible memory offered by the capset (feature 041),
   **When** a buffer is mapped, **Then** the returned pointer is host GPU memory
   and no copy is made; otherwise the ICD stages a copy in at map and out at
   unmap.
2. **Given** `clSVMAlloc`, **When** the pointer is written in the guest and
   passed to a kernel, **Then** the kernel sees the bytes; SVM capabilities
   report coarse-grained only.
3. **Given** `CL_MEM_USE_HOST_PTR` with guest memory, **Then** the data is
   staged correctly and the pointer's contents reflect kernel results after a
   map.

### User Story 3 - Events and transfers never deadlock (Priority: P1)

**Independent Test**: loopback tests gate blocking and non-blocking transfers
on user events set from the same thread, another thread and a callback, with a
timeout that fails the test instead of hanging.

**Acceptance Scenarios**:

1. **Given** a non-blocking read gated on a user event the same guest thread
   sets later, **When** the event is set, **Then** the read completes; the
   transfer is not forced blocking.
2. **Given** `clSetEventCallback`, **When** the event completes on the host,
   **Then** the callback runs once in the guest, on a thread the ICD owns, and
   never while the transport's lock is held.
3. **Given** `clFinish` with queued work, **Then** it sleeps on the rvk fence
   until the host reaches it; it does not poll.

### Edge Cases

- Two platforms registered (rvkgpu and another vendor): the loader lists both;
  the ICD reports only its own devices.
- A kernel faults or the host device is lost: `CL_OUT_OF_RESOURCES` on the
  queue's operations, contexts on other devices unaffected.
- TDR while a blocking transfer waits: the wait returns an error, not a hang.
- A callback that releases the event or the context it belongs to.
- Allocation sizes and offsets that overflow, or exceed
  `CL_DEVICE_MAX_MEM_ALLOC_SIZE`: rejected before any host command.
- A host device without an optional 3.0 feature (for example SVM or
  `cl_khr_il_program`): not reported.
- The application unloads the ICD with objects alive: host objects are freed.

## Clarifications

### Session 2026-09-18

- Q: The generated wire carries none of `clEnqueueMapBuffer`, `clEnqueueUnmapMemObject` or the SVM family, yet US2 makes maps and coarse-grained SVM P1. How does 052 deliver US2? → A: extend the wire. `tools/codegen/profiles/opencl.toml` gains map/unmap and the coarse-grained SVM commands (`clSVMAlloc`, `clSVMFree`, `clEnqueueSVMMap`, `clEnqueueSVMUnmap`, `clEnqueueSVMMemcpy`, `clEnqueueSVMMemFill`, `clSetKernelArgSVMPointer`), the client, server and stubs are regenerated, and the device's OpenCL backend carries them; a map is host GPU memory where the capset offers host-visible memory (feature 041) and a staged copy in and out otherwise.

- Q: US3 wants `clSetEventCallback`'s callback to run on a thread the ICD owns, but no ICD here owns a thread and `clSetEventCallback` is not on the wire. How does the callback run? → A: the host pushes completion and a guest thread wakes on the fence. `clSetEventCallback` joins the wire; the device's OpenCL backend registers its own host callback and signals the context's fence when the host event completes; the ICD owns one watcher thread per process that sleeps in `Kmt::wait_for_fence` (never polls), asks which events completed, and runs the application's callbacks with no ICD lock held. The thread is a seam (`icd::ServiceThread`) so a test steps it deterministically on Linux.

- Q: A non-blocking transfer's reply is written when the host queues it, not when it completes, so where do its bytes come from without making it blocking (FR-004)? → A: the host writes into a host-visible blob. The destination is a feature-041 host-visible blob the guest has mapped, the host's transfer lands in it directly, and the ICD copies to the application's pointer when it observes the event complete. Where the capset offers no host-visible memory the ICD records the destination against the event and issues the read when the application observes completion; it never forces the enqueue to block.

- Q: SC-001 runs through the system `OpenCL.dll`, but a clean guest may have no Khronos loader and nothing outside this repository may be referenced. How is the ICD proven? → A: guest image preparation installs a loader. `tools/fetch_opencl_loader.py` with `tools/opencl_loader_pins.txt` fetches the Khronos loader by pinned URL and SHA-256 into `third_party/` exactly as `tools/fetch_vulkan_runtime.py` fetches `vulkan-1.dll`, and the runner pushes it beside `rvkclprobe.exe` in the guest; it is never committed and never part of the driver package. The L5 run therefore exercises the real adapter-key discovery path, and SC-001 names this repository's probe rather than any other project's tests.

- Q: The edge cases name 32-bit WoW64 applications and `OpenCLDriverNameWow`, which 050 moved to 061 for OpenGL. What does 052 do? → A: the same. 052 builds the 64-bit `rvkcl.dll` and writes no `OpenCLDriverNameWow`; the 32-bit ICD, its registration and the below-4-GiB constraints on SVM and maps move to 061, where all three ICDs are built for WoW64 once.

## Requirements *(mandatory)*

- **FR-001**: The ICD is registered only through the adapter's
  `OpenCLDriverName` value written by the driver package; the global Khronos
  vendors key is not written, and `OpenCLDriverNameWow` is feature 061's.
- **FR-002**: Commands travel over the guest transport of feature 043; the
  ICD opens no ivshmem device or shared region.
- **FR-003**: Version, optional features and extensions reported are the
  intersection of this ICD's own extension policy - one row per extension, each
  naming the test that proves it - and what the host device reports
  (constitution VI).
- **FR-004**: No transfer is made blocking by the ICD; a non-blocking enqueue
  returns as soon as the host has queued it, and completion is observed through
  rvk fences and event state from the host.
- **FR-005**: Every size, offset, flag and handle from the application is
  validated before arithmetic or a host command.
- **FR-006**: `cl_khr_gl_sharing` is reported only once feature 053 implements
  sharing with the OpenGL ICD of feature 050.

## Success Criteria *(mandatory)*

- **SC-001**: `rvkclprobe.exe`'s scenarios pass in the guest, 64-bit, through
  the Khronos `OpenCL.dll` the runner pushes beside it - so through the loader's
  own discovery of `OpenCLDriverName` on the rvkgpu adapter - and the same
  scenarios pass over the full-stack rig on Linux.
- **SC-002**: The deadlock scenarios complete within their timeout on every
  run of the loopback suite.
- **SC-003**: The ICD core has 100% branch coverage and fuzz targets over its
  parsing of program binaries and property lists.

## Assumptions

- The guest test image has the Khronos OpenCL ICD loader (`OpenCL.dll`),
  fetched by pinned URL and hash and pushed there by the runner. It is not
  committed, and the driver package ships no loader.
- Host OpenCL runs through the host vendor's ICD, or a portable implementation
  where none exists; the device reports whichever the executor bound.

## Out of Scope

- Fine-grained or system SVM.
- The 32-bit (WoW64) ICD and `OpenCLDriverNameWow`: feature 061.
- Sharing with OpenGL, Vulkan or D3D and across processes: feature 053.
- Conformance runs: feature 054.
