# Feature Specification: Sharing across APIs and processes

**Feature Branch**: `053-cross-api-sharing`

**Created**: 2026-09-11

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 053 · **Status**: `implement` · **Depends on**: 047, 051, 052

Allocations and fences applications share between processes through NT handles
- beyond the presentation paths' own sharing with DWM in 046 and 051 - with
device-global identity on the host, and the interop extensions built on them:
Vulkan Win32 external memory and semaphores, OpenGL external objects, OpenCL
external memory and cl_khr_gl_sharing - the machinery DXGI, DWM and multi-API
applications depend on.

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

Wire, `src/proto/`:

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `SharedObjectRecord` | object | the magic, version, object id, kind, adapter LUID and size of one shared object | Carry a shared object's identity in an allocation's private driver data |
| `SharedObjectRequest` | object | the object id, kind and wire handle one transport command names | Frame the body the three shared-object transport commands share |
| `SharedObjectAnswer` | object | the status and the wire handle a shared-object command answers | Frame what a shared-object command answers |

Guest kernel, `src/core/memory/`:

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `SharedObject` | object | its id, kind, size, the allocation behind it, and how many devices hold it | Be one shared object the adapter knows |
| `SharedObjectTable` | system | the adapter's shared objects and the id it mints next | Give an allocation an identity the whole adapter agrees on |

QEMU device, `src/device/`:

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `HostSharedObject` | object | its id, kind, size, the host descriptor behind it, and how many contexts hold it | Be one host resource several contexts name |
| `HostSharedObjects` | system | the device's shared objects, under the lock its workers reach it through | Keep one host resource per object id for the whole device |
| `ContextSharedObjects` | object | what one context published or opened, and the wire handle each became | Hold one context's share of the device's shared objects |
| `SharedObjectCommand` | object | the header, the body and the tables the command reaches | Answer one SHARE / OPEN / RELEASE transport command |

Guest user mode, `src/icd/`:

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `icd::SharedHandles` | system | the NT handles this process holds, each with the object id behind it | Turn a Win32 handle into the object id the device knows |
| `glicd::GlExternalObjects` | system | the GL memory objects and semaphores this context imported | Hold one GL context's imported external objects |
| `clicd::ClExternalMemory` | system | the `cl_mem` objects this context imported from Win32 handles | Hold one OpenCL context's imported external memory |
| `clicd::ClGlSharing` | system | the GL objects a context has acquired, and the depth of each | Track which GL objects an OpenCL context currently owns |

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

- [`docs/kb/03-object-model.md`](../../docs/kb/03-object-model.md)
- [`docs/kb/04-memory.md`](../../docs/kb/04-memory.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## User Scenarios & Testing *(mandatory)*

<!-- Written by /speckit-specify or by hand. The autogen block above is regenerated by tools/gen_specs.py; everything below is preserved. -->

Users: applications that hand GPU objects between APIs or processes - a
Vulkan renderer displaying through DXGI, a compute stage in OpenCL feeding an
OpenGL viewport, video pipelines, browsers with a GPU process, and DWM, which
composites surfaces other processes rendered. Also the driver's developers, who
must prove that a shared handle cannot become a way for one process to reach
another's memory.

Starting point: render-server shares images between its own APIs inside one
session with `kProtoCreate/Import/DestroySharedImage`, backed by dma_bufs on
the host. Nothing crosses a process boundary, and nothing is expressed as a
WDDM shared allocation or an NT handle. kb 11 risk 6 applies: device object
identity must be global, not per open.

### User Story 1 - Shared allocations and fences across processes (Priority: P1)

**Independent Test**: the fake dxgkrnl drives shared-allocation and
shared-fence creation, open from a second device, and close in every order;
loopback tests open the same host object from two guest contexts; in the
guest, two `rvktest` processes exchange an NT handle and read each other's
rendered pixels.

**Acceptance Scenarios**:

1. **Given** an allocation created shareable, **When** its owner shares it with
   `D3DKMTShareObjects` and another process opens it with
   `D3DKMTOpenResourceFromNtHandle`, **Then** both refer to one host object and
   writes by one are visible to the other.
2. **Given** a shared monitored or native fence, **When** one process signals
   it, **Then** the other's wait completes.
3. **Given** the owner exits, **When** the other process still holds the
   handle, **Then** the host object lives until the last reference closes.
4. **Given** a handle the process did not receive, or a forged one, **When** it
   is opened, **Then** the open fails and no host object is touched.

### User Story 2 - Interop extensions on shared objects (Priority: P1)

**Independent Test**: guest programs run each pair - Vulkan to OpenGL, OpenGL
to OpenCL, OpenCL to Vulkan - in one process and across two, checking pixel
and buffer contents and synchronization order.

**Acceptance Scenarios**:

1. **Given** `VK_KHR_external_memory_win32` and
   `VK_KHR_external_semaphore_win32`, **When** a Vulkan image is exported and
   imported by another Vulkan device or process, **Then** contents and
   semaphore ordering are preserved.
2. **Given** `GL_EXT_memory_object_win32` and `GL_EXT_semaphore_win32`, **When**
   an OpenGL context imports that handle, **Then** it renders into the same
   host image the Vulkan device reads.
3. **Given** `cl_khr_gl_sharing`, **When** an OpenCL kernel acquires a GL
   buffer or texture, **Then** it reads and writes the GL object without a
   copy through the guest, and `clEnqueueReleaseGLObjects` returns it.
4. **Given** `cl_khr_external_memory_win32`, **When** OpenCL imports a Vulkan
   or D3D-shared allocation, **Then** kernels see its contents.

### User Story 3 - Object identity is global on the host (Priority: P1)

**Independent Test**: device-core tests open one shared object from contexts
of different guest devices and assert one host object, one refcount, one
teardown; fuzz targets feed hostile share and open requests.

**Acceptance Scenarios**:

1. **Given** two rvk contexts from different processes on one adapter,
   **When** both import the same shared allocation, **Then** the device holds
   one host resource with two references.
2. **Given** a context that faults or is destroyed, **Then** shared objects
   others hold survive it.

### Edge Cases

- A hostile process opens a handle after the owner exited, duplicates it
  thousands of times, or closes it twice: bounded, refcounted, no host leak,
  no use after free.
- A handle from another adapter (multi-GPU): the open fails; cross-adapter
  sharing is not claimed.
- Formats or sizes that one API cannot represent: import fails with that API's
  documented error.
- The host GPU lacks dma_buf export or the external-memory extension: the
  guest extensions are not reported.
- TDR of the owner's device while others read: they see device loss for that
  object, not corruption.
- 32-bit and 64-bit processes sharing one handle.
- Sync objects signalled after every importer closed.

## Requirements *(mandatory)*

- **FR-001**: Cross-process sharing uses WDDM shared allocations and NT
  handles only; no named shared memory or out-of-band channel.
- **FR-002**: The KMD validates every open against dxgkrnl's shared-object
  records and the allocation private data it wrote; the device validates every
  import against its global object table.
- **FR-003**: Host object identity is global per adapter, refcounted across
  contexts and processes (kb 11 risk 6).
- **FR-004**: Each interop extension is reported only when implemented, tested
  and supported by the host (constitution VI).
- **FR-005**: All share and import parsing on both sides has hostile-input
  tests and fuzz targets (constitution IV.3, V).

## Success Criteria *(mandatory)*

- **SC-001**: The interop matrix (Vulkan, OpenGL, OpenCL; same process and
  cross-process) passes in the guest.
- **SC-002**: A stress run opening and closing shared handles from hostile
  processes ends with zero host objects and no guest pool leak.
- **SC-003**: DWM composites a window rendered by another process's Vulkan
  swapchain through a shared allocation (checked again by feature 058).

## Assumptions

- Features 047, 051 and 052 provide the per-API drivers.
- Host sharing is built on dma_bufs and host API external-memory import, as
  render-server's shared images already are.

## Out of Scope

- Sharing between adapters or with other vendors' GPUs.
- D3D11/D3D12 shared resources through the Microsoft runtime: features
  055-057 build on this feature's machinery.
