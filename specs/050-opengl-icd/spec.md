# Feature Specification: OpenGL: an installable client driver

**Feature Branch**: `050-opengl-icd`

**Created**: 2026-09-11

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 050 · **Status**: `implement` · **Depends on**: 044

An OpenGL ICD the system opengl32.dll loads for the adapter - the Drv*
interface, pixel formats, contexts and share lists, the WGL_ARB extensions -
over the generated OpenGL encoders and the guest transport, so every OpenGL
application uses the host GPU without a DLL placed beside it. OpenGL 4.6 core,
executed on the host through EGL.

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
| `GlIcdRoot` | system (guest UM) | the context pool, the window table, the pixel formats, the injected seams | Answer the `Drv*` interface |
| `PixelFormatTable` | object | the fixed list of formats this ICD serves | Describe a pixel format by its index |
| `PixelFormatQuery` | object | the table it answers from, the attribute list being judged | Answer `WGL_ARB_pixel_format`'s queries |
| `ContextAttributes` | object | the decoded `wglCreateContextAttribsARB` list | Judge one attribute list |
| `WindowTable` | system | per window: its pixel format, client size, back buffers | Track the windows a process drew on |
| `GuestGlContext` | object | one `DHGLRC`: its options, share target, transport, owner thread, local error | Be one application GL context |
| `CurrentContexts` | system | the `ThreadSlot`, the contexts current on threads | Say which context a thread draws with |
| `ClientArrayState` | object | the bindings and client arrays the manual entry points read | Size the bytes a command sends |
| `BufferMapping` | object | one mapped range, its arena lease | Mirror a mapped buffer range in guest memory |
| `BackBufferPresenter` | object | a window's host framebuffer, its read-back lease | Put a finished frame on the window's DC |
| `GlExtensionPolicy` | object | the rows of reportable GL and WGL extensions and their tests | Decide which extensions the application sees |
| `KmtBlobArena` | object (guest transport) | one `host_visible_memory` chunk bound to the context, its bump offset | Lend room for bulk data |
| `GlContextOptions` | object (proto) | version, profile, flags, share context | Carry what a host context is made with |
| `ProcTableOrder` | object (tools, generated data) | Microsoft's `GLCLTPROCTABLE` order | Place a GL 1.1 entry at its table index |

`ExtensionPolicy`, `ProcessHeap`, `SrwLock`, `FileLog`, `ThreadLock`,
`LockedTransport`, `Connection`'s adapter search and 045's chunk binding are
reused, moved to `src/icd/common/` and `src/um/icd/` where both ICDs need them.

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
- [`docs/kb/11-implementation-plan.md`](../../docs/kb/11-implementation-plan.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## User Scenarios & Testing *(mandatory)*

<!-- Written by /speckit-specify or by hand. The autogen block above is regenerated by tools/gen_specs.py; everything below is preserved. -->

Users: people running ordinary Windows OpenGL applications in the guest (CAD
tools, emulators, games, anything linked against `opengl32.lib`), who expect
them to use the host GPU the moment the rvkgpu package is installed, without
copying a DLL beside each executable. Also the driver's developers, who need the
ICD interface and pixel-format logic tested on Linux before a guest runs it.

Starting point: the generated OpenGL client (063) encodes OpenGL 4.6 core
(645 commands) for execution on the host through surfaceless EGL (044), but it
was shaped as a replacement `opengl32.dll` exporting the GL entry points by
name, and it does not link: its ICD hooks do not exist. The system
`opengl32.dll` never loads such a DLL. This feature makes it an installable
client driver.

### User Story 1 - The system opengl32.dll loads the rvkgpu ICD (Priority: P1)

**Independent Test**: on Linux, a fake `opengl32` loader (tests) drives the
ICD's exported `Drv*` interface through the call order Microsoft's
`opengl32.dll` uses - `DrvValidateVersion`, `DrvSetCallbackProcs`,
`DrvDescribePixelFormat`, `DrvSetPixelFormat`, `DrvCreateContext`,
`DrvSetContext`, `DrvReleaseContext`, `DrvDeleteContext` - over MockKmt and the
loopback rig, including every refusal; in the guest, `rvktest`'s GL scenario
creates a context through the system `opengl32.dll` with no DLL beside it.

**Acceptance Scenarios**:

1. **Given** the package installed on an rvkgpu adapter, **When** an
   application calls `SetPixelFormat` and `wglCreateContext` on a window on that
   adapter's display, **Then** the system `opengl32.dll` loads the ICD named by
   the adapter's `OpenGLDriverName` registration and the context runs on the
   host GPU.
2. *(moved to 061)* **Given** a 32-bit application under WoW64, **When** it
   does the same, **Then** the 32-bit ICD named by `OpenGLDriverNameWow` loads.
3. **Given** `DrvSetContext`, **When** a context becomes current on a thread,
   **Then** the ICD returns a complete dispatch table for that context's version
   and profile, and `glGetString(GL_RENDERER)` names the host GPU the adapter is
   bound to.
4. **Given** a context whose version the host cannot provide, **When**
   `wglCreateContextAttribsARB` asks for it, **Then** creation fails with the
   WGL error the ARB extension specifies, not a context of another version.

### User Story 2 - Pixel formats and WGL extensions behave as on a hardware driver (Priority: P1)

**Independent Test**: the pixel-format table and WGL extension answers are
table-tested on Linux against what `DrvDescribePixelFormat` must report for
each format the ICD offers; `wgl_smoke`-style scenarios run in the guest.

**Acceptance Scenarios**:

1. **Given** a window, **When** the application enumerates pixel formats,
   **Then** every format described is one the presentation path can serve
   (color, depth, stencil, double buffer, sRGB, multisample) and its index is
   stable for the ICD's lifetime.
2. **Given** `WGL_ARB_create_context`, `WGL_ARB_create_context_profile`,
   `WGL_ARB_pixel_format`, `WGL_ARB_multisample` and `WGL_ARB_framebuffer_sRGB`
   (`WGL_EXT_swap_control` moved to 051), **When** an application queries
   `wglGetExtensionsStringARB`, **Then** each is listed only when implemented
   and tested (constitution VI).
3. **Given** two contexts on one pixel format, **When** `wglShareLists` is
   called before either has objects, **Then** textures, buffers and programs
   are shared; after, **Then** it fails as Microsoft's implementation does.

### User Story 3 - Many contexts, many threads, one transport (Priority: P2)

**Independent Test**: loopback tests create contexts on several threads, make
them current alternately, share objects and delete contexts in every order,
with invariants after each step and no leak in the guest or the device.

**Acceptance Scenarios**:

1. **Given** one process with several GL contexts and a Vulkan or OpenCL
   driver loaded, **When** all issue commands, **Then** they share the
   per-process link of feature 043 and each GL context maps to its own rvk
   OpenGL context on the host.
2. **Given** a context current on thread A, **When** thread B makes it current
   without A releasing it, **Then** `DrvSetContext` fails and A's state is
   untouched.

### Edge Cases

- A 32-bit WoW64 application loads `opengl32`: the 32-bit ICD, 32-bit views of
  host-visible memory below 4 GiB (feature 041), same host behaviour.
- `SetPixelFormat` on a window that already has a pixel format: fails, as on
  Windows, without changing the ICD's per-window state.
- A window moves to another adapter's output, or the DC belongs to a memory
  bitmap: the ICD declines the format rather than rendering to the wrong GPU.
- TDR or a faulted rvk context: every context of that device reports
  `GL_CONTEXT_LOST` through `GL_ARB_robustness` when requested, otherwise
  `glGetError` returns `GL_OUT_OF_MEMORY` and calls become no-ops; no guest
  process crashes.
- The application deletes the current context, or exits with contexts current
  on several threads: resources on the host are released exactly once.
- The host GPU's EGL lacks an extension the guest would forward: it is not
  advertised.
- The adapter is disabled while contexts exist: calls fail, the link closes,
  the process survives.
- A hostile process passes garbage `DHGLRC`, `HDC` or pixel-format indices:
  every entry point validates and fails.

## Clarifications

### Session 2026-09-16 (resolved from the code, the WDK and `opengl32.dll`; research.md)

- Q: How does the system `opengl32.dll` find the ICD? → A: `D3DKMTOpenAdapterFromHdc` on the application's DC (or, with no usable DC, the first `D3DKMTEnumAdapters2` adapter), then `D3DKMTQueryAdapterInfo(KMTQAITYPE_UMOPENGLINFO)`, which dxgkrnl answers from `OpenGLDriverName`, `OpenGLVersion` and `OpenGLFlags` in the adapter's software key (`DpiGetAdapterInfo`). One ICD per process, chosen at the first pixel-format call (research §1).
- Q: What does `OpenGLVersion`/`OpenGLFlags` have to be? → A: `OpenGLVersion` = 2 (passed to `DrvValidateVersion`), `OpenGLFlags` = 1, which makes `opengl32` require and call `DrvDescribePixelFormat`, `DrvSetPixelFormat` and `DrvSwapBuffers` (research §1).
- Q: 32-bit applications (US1.2, SC-001's 32-bit half)? → A: moved to 061 with 045's 32-bit Vulkan ICD; 050 writes no `...Wow` value.
- Q: render-server's t00-t55 programs (SC-001)? → A: render-server contributes no runtime (063), and nothing outside this repository is referenced. SC-001 is proven by `rvkglprobe.exe` through the system `opengl32.dll` and the same probe over the full-stack rig.
- Q: What does a legacy `wglCreateContext` get before 051? → A: a core-profile context at the host's version (capped at 4.6), reported truthfully as core; GL 1.1 entries the core profile lacks raise `GL_INVALID_OPERATION` locally. 051 makes legacy creation compatibility.
- Q: `WGL_EXT_swap_control`? → A: moved to 051, which paces presentation; 050 presents by read-back, where an interval means nothing testable.
- Q: `DrvSwapBuffers` before 051? → A: read the back buffer into the context's blob arena and `SetDIBitsToDevice` it to the DC (research §6).
- Q: Buffer mappings (FR-006)? → A: GL cannot import a host pointer on both host drivers, so every mapping is fill-at-map, write-back-at-unmap through the blob arena, logged once per context; the arena itself is a 045 host-visible chunk the host reads with no copy (research §5).
- Q: Share lists (US2.3)? → A: the host context is made lazily at the first `DrvSetContext`, with the share target `wglShareLists` named; after that `DrvShareLists` fails (research §4).
- Q: Which windows can the guest proof draw on? → A: the runner's processes run in session 0; a live experiment (tasks Phase 2) proves `opengl32` picks the rvkgpu ICD there before anything is built on it, with an interactive-session launcher as the recorded fallback.

## Requirements *(mandatory)*

- **FR-001**: The ICD is registered only through the adapter's
  `OpenGLDriverName` / `OpenGLDriverNameWow` values written by the driver
  package; nothing is written under a global Khronos or `OpenGLDrivers` key and
  nothing is copied beside applications.
- **FR-002**: The ICD exports the `Drv*` interface Microsoft's `opengl32.dll`
  calls, and each export is one line forwarding to a class in the portable ICD
  core (constitution III; the export boundary is an ABI boundary).
- **FR-003**: The ICD reports OpenGL 4.6 core only when the host GPU provides
  it through EGL, otherwise the host's version; extensions are the intersection
  of the generated tier list (`gl_tier.h`), what this feature tests, and the host.
- **FR-004**: The ICD's pixel-format decisions, context table, current-context
  tracking and WGL extension answers are portable logic tested on Linux to the
  constitution's standard; the Windows shim is translation only.
- **FR-005**: GL commands travel over the guest transport of feature 043; the
  ICD opens no ivshmem device, shared region or named mapping.
- **FR-006**: Buffer mappings are filled at map and written back at unmap
  through the context's blob arena, whose memory the host reads without a copy;
  the path is visible in the driver's log.
- **FR-007**: Every value from the application or the host that the ICD reads
  is validated; malformed input returns a GL or WGL error, never an assert.

## Success Criteria *(mandatory)*

- **SC-001**: With no DLL beside it, `rvkglprobe.exe` passes every step in the
  guest through the system `opengl32.dll` (64-bit; 32-bit is 061's), and the
  same probe passes through the ICD over the full-stack rig.
- **SC-002**: The ICD core has 100% branch coverage on Linux and a fuzz target
  over its WGL attribute-list parsing.
- **SC-003**: `glGetString(GL_VERSION)` in the guest matches the version the
  host EGL context provides, capped at 4.6.

## Assumptions

- Feature 043's transport and feature 044's live rvk contexts exist.
- Feature 045's ICD infrastructure (process heap, locks, log, connection,
  host-visible chunks) exists; this feature is built on its branch.
- The generated OpenGL client (063) links below the ICD's entry layer once its
  hooks exist and the emitter writes the `GLCLTPROCTABLE` in Microsoft's order;
  it exports no GL symbol.

## Out of Scope

- The compatibility profile, presentation through rvkgpu and
  `WGL_EXT_swap_control`: feature 051. Until then SwapBuffers reads back and
  blits through GDI.
- The 32-bit ICD (`OpenGLDriverNameWow`): feature 061.
- OpenGL ES through WGL, GLX-style overlays and layer planes
  (`DrvCreateLayerContext` accepts only layer 0).
- Interop with Vulkan, OpenCL and other processes: feature 053.
- Conformance runs: feature 054.
