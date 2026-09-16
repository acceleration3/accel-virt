# Implementation Plan: OpenGL - an installable client driver

**Branch**: `050-opengl-icd` | **Date**: 2026-09-16 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/050-opengl-icd/spec.md`

## Summary

A freestanding `rvkgl.dll` that the system `opengl32.dll` loads through the
adapter's `OpenGLDriverName` (with `OpenGLVersion` 2 and `OpenGLFlags` 1),
answering the `Drv*` interface: pixel formats from a fixed table, contexts made
lazily on the host with the attributes and share target the application gave,
a `GLCLTPROCTABLE` in Microsoft's order from the generated OpenGL client, WGL
extensions answered through `DrvGetProcAddress`, and `DrvSwapBuffers` reading
the host back buffer into guest memory and blitting it through GDI until 051.
Each GL context is its own rvk OpenGL context on the process's one device; bulk
data crosses through a blob arena that is a 045 host-visible chunk imported by
the GL backend. It is built on 045's branch and reuses its ICD pieces, moved
where both ICDs share them. Research moved the 32-bit ICD to 061 and
`WGL_EXT_swap_control` to 051 (research.md §10), and puts a live experiment on
session 0 first (§9).

## Technical Context

**Language/Version**: C++23 freestanding (ICD core, KMD, device, executor);
Python 3 (tools, generator, runner).

**Primary Dependencies**: 045's ICD infrastructure (`BlockSource`,
`ThreadLock`, `LockedTransport`/`SyncCallScope`, `Connection`'s adapter search,
`HostVisibleChunk`, `BlobImport`, `ExtensionPolicy`, `ProcessHeap`, `SrwLock`,
`FileLog`); 043's `KmtTransport`, `GuestConnection`, `ClientBlobArena`; 063's
generated OpenGL client and entries (vendored generator); 042/044's
`OpenGlBackend`, `GlDisplay`, `GlContext` over EGL; the system `opengl32.dll`.

**Storage**: N/A.

**Testing**: L1 over `MockKmt` with a scripted host and a scripted
`WindowSurface` for every ICD class, including a fake `opengl32` driving the
`Drv*` exports in Microsoft's order (research §1); emitter tests in
`tests/tools/`; KMD, device and executor unit tests for the context record's
options and share contexts (executor tests on real GPUs); fuzz targets
for WGL attribute lists, pixel-format queries and the record; L4 full-stack
probe through the ICD's `Drv*` entries with the real executor's EGL; L5
`run_guest_suite.py --opengl` with `rvkglprobe.exe`.

**Target Platform**: Windows 11 guest (x64 user mode, no CRT); Linux host.

**Project Type**: driver components (user-mode DLL, KMD, QEMU device, host library, generator).

**Performance Goals**: a swap costs one read-back into mapped guest memory and
one GDI blit; no command above `kPixelBlobFloor` copies through the ring.

**Constraints**: no STL/CRT; imports only gdi32, kernel32, ntdll, user32; the
DLL loads, unloads and reloads cleanly; commands up to 256 KiB, replies up to
64 KiB; nothing reported that is not tested (constitution VI); the executor
ABI stays loadable by version 1-3 libraries.

**Scale/Scope**: one ICD DLL (~14 portable classes), an emitter change and a
committed table-order file, context options in the record's reserved tail through KMD,
device and executor, the GL backend's import, a probe, runner and INF changes.

## Constitution Check

| Principle | How this feature satisfies it |
|---|---|
| I. Specification precedes implementation | spec clarified (2026-09-16), research.md answers every question from binaries and code, scope moves recorded |
| II. Core portable, shim translation only | every decision in `src/icd/opengl` and `src/icd/common`; `src/um/opengl` holds the one-line exports and the `WindowSurface`/`ThreadSlot` implementations |
| III. Object-oriented, with state | every class in the spec's table owns state; the ICD root is one `ObjectStorage` in the exports' translation unit and the generated hooks reach it through `GlIcdBinding` - the Vulkan ICD's exception, taken a second time (PATCH amendment) |
| IV. Test-first and exhaustive | each class over MockKmt at 100% branches; hostile attribute lists, pixel-format queries, host replies and the record's options, each with a fuzz target; invariants after every mutation; the allocation-failure sweep |
| V. Everything from outside is hostile | the ICD validates `DHGLRC`, `HDC`, format indices and attribute lists; the KMD judges the options and that a share context is the same process's; the executor makes a context only from judged options |
| VI. Feature levels are phases | extensions only with the policy's test; legacy contexts reported as the core contexts they are; swap control withheld until 051 |
| VII. IRQL and locking | the KMD's record handling at PASSIVE as 043's; ICD locks user mode, order recorded and asserted (`RecordingThreadLock`) |
| VIII. No speculative structure | `WindowSurface` and `ThreadSlot` each have the Windows implementation and the test one; the shared ICD pieces move only because a second ICD uses them |
| IX. Reproducible and observable | `opengl32.dll` is pinned in `tools/re/binaries.txt`; the ICD's log (the mapping path, every refusal) reaches `diag/` |

**Amendment carried (PATCH):** the process-lifetime list gains `rvkgl.dll`'s
root behind its `Drv*` exports and `GlIcdBinding`, for the reason 045's
entry gives.

## Classes this feature introduces

The spec's table (`spec.md`, "Classes this feature introduces") is the list.
Changed, not new: `RvkContextRecord` (the options in its reserved tail),
`ContextSetup` (options), `RvkContextTable` (judges a share context),
`OpenGlBackend` (`initialize` with options and a share context, `adopt` takes
the region), `GlDisplay::create_context` (attributes, share), the executor ABI
(version 4, `create_with_options`), the device's context table (resolves a share context), `ClientBlobArena` (the lease the
generated client names), the OpenGL emitter.

## Project Structure

### Documentation (this feature)

```text
specs/050-opengl-icd/
├── plan.md, research.md, data-model.md, quickstart.md
├── contracts/icd_interface.md      # INF values, exports, Drv* behaviour, proc table
├── contracts/context_options.md    # record v2, ContextSetup options, executor ABI v4
└── tasks.md
```

### Source Code

```text
src/icd/common/                   # MOVED from src/icd/vulkan: ExtensionPolicy, ThreadLock, LockedTransport, LogSink, adapter connection, chunk binding
src/icd/opengl/                   # NEW: GlIcdRoot, PixelFormatTable, PixelFormatQuery, ContextAttributes, WindowTable,
                                  #      GuestGlContext, CurrentContexts, ClientArrayState, BufferMapping,
                                  #      BackBufferPresenter, GlExtensionPolicy, gl_hooks.cpp, gl_helpers.h, seams
src/guest/transport/kmt_blob_arena.*   # NEW: the arena over a bound chunk
src/proto/context/                # RvkContextRecord v2, ContextSetup options, GlContextOptions
src/proto/client_blob_arena.*     # the lease the generated client names
src/core/rvk/                     # the record's options judged, share context resolved
src/device/execution/, src/device/context/  # options into the session
src/device/abi/rvk_executor.h     # version 4: create_with_options
src/device/executor/              # OpenGlBackend options, adopt; ExecutorRoot::create_with_options
src/device/shim/gl_display.*, gl_context.*  # EGL attributes and share context
src/um/icd/                       # MOVED from src/um/vulkan: ProcessHeap, SrwLock, FileLog
src/um/opengl/                    # NEW: rvkgl_exports.cpp, rvkgl.def, GdiWindowSurface, TlsThreadSlot
src/um/probe/rvkglprobe*          # NEW: the probe's shim; imports opengl32.dll
src/guest/suite/gl_probe.*        # NEW: the probe's scenarios, portable over a GlEntry seam
src/km/rvkgpu.inf                 # OpenGLDriverName, OpenGLVersion, OpenGLFlags, rvkgl.dll
tools/codegen/rvk_codegen/opengl/emit.py, tools/codegen/profiles/opengl_proc_table.txt
tools/re/gl_proc_table.py, tools/re/binaries.txt (opengl32.dll)
tools/check_guest_image.py (--allow-import user32.dll), tools/guest/run_guest_suite.py (--opengl)
tests/unit/icd/opengl/, tests/harness/icd/ (fake opengl32, scripted window surface),
tests/loopback/test_fullstack_opengl_icd.cpp, tests/fuzz/fuzz_{context_attributes,pixel_format_query,rvk_context_record}.cpp
```

**Structure Decision**: `src/icd/opengl/` beside `src/icd/vulkan/`, with the
pieces both use in `src/icd/common/` (the plan of 045 §2 foresaw this).

## Complexity Tracking

| Violation | Why needed | Simpler alternative rejected because |
|---|---|---|
| A fifth piece of process-lifetime state (the GL ICD root and its binding) | `opengl32` calls `DrvValidateVersion`, `DrvCreateContext`, ... with no ICD context, and the generated hooks take none | one root for both ICDs would make `rvkvk.dll` link OpenGL and `rvkgl.dll` link Vulkan |
| A user32 import | a window's client size and the window behind a DC are user32's (`WindowFromDC`, `GetClientRect`) | gdi32's clip box answers the visible area, not the client size, and is wrong for an occluded window |
| A committed table-order file derived from a Microsoft binary | the `GLCLTPROCTABLE` order is ABI and written down nowhere in the WDK | hard-coding it in the emitter hides where it came from; the file is checked against the pinned binary |
