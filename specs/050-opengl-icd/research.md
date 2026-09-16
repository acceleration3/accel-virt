# Research: OpenGL - an installable client driver

Sources: `opengl32.dll` and `gdi32full.dll` 10.0.26100.5074 (Windows 11 24H2 and
25H2, the guest image's family) and `dxgkrnl.sys` from `tools/re/binaries.txt`,
symbolized in `re/ghidra-proj`; the SDK's `d3dkmthk.h`; this repository's
generated OpenGL tree and executor. Addresses are `opengl32.dll`'s unless named.

## 1. How the system opengl32.dll finds the ICD

**Decision.** The INF writes, in each rvkgpu adapter's software key:

| Value | Type | Data |
|---|---|---|
| `OpenGLDriverName` | `REG_MULTI_SZ` | `%13%\rvkgl.dll` |
| `OpenGLVersion` | `REG_DWORD` | 2 |
| `OpenGLFlags` | `REG_DWORD` | 1 |

Nothing under `HKLM\...\OpenGLDrivers`, nothing beside applications.

**What opengl32 does** (`LoadAvailableDrivers` @18006d410, once per process,
under `semLocal`, at the first pixel-format or context call on a DC):

1. `bGetLDDMDriverInfoFromHdc` @180046648: `D3DKMTOpenAdapterFromHdc(hdc)`. If
   that fails it tries `CreateDCA` on each `EnumDisplayDevicesA` entry with the
   primary flag, and failing that `bGetLDDMDriverInfoNoHdc` @18006dac8 takes the
   **first** adapter `D3DKMTEnumAdapters2` lists.
2. `bGetLDDMDriverInfo` @1800466b0 (hybrid and preferred-GPU logic aside, which
   an adapter without `KMTQAITYPE_...` hybrid flags skips) calls
   `D3DKMTQueryAdapterInfo(KMTQAITYPE_UMOPENGLINFO = 2, 0x210 bytes)` into
   `D3DKMT_OPENGLINFO {WCHAR UmdOpenGlIcdFileName[260]; ULONG Version; ULONG Flags}`
   and copies name, `Version` and `Flags` into its driver record, with its own
   record version 2.
3. `pgldrvLoadAndAllocDriverInfo` @18003f568: `LoadLibraryExW(name,
   LOAD_WITH_ALTERED_SEARCH_PATH)`, then `GetProcAddress` of
   `DrvValidateVersion`, `DrvSetCallbackProcs` and the 18 names of the table at
   @1800cdf20, in this order: `DrvCreateContext`, `DrvDeleteContext`,
   `DrvSetContext`, `DrvReleaseContext`, `DrvCopyContext`,
   `DrvCreateLayerContext`, `DrvShareLists`, `DrvGetProcAddress`,
   `DrvDescribeLayerPlane`, `DrvSetLayerPaletteEntries`,
   `DrvGetLayerPaletteEntries`, `DrvRealizeLayerPalette`, `DrvSwapLayerBuffers`,
   `DrvSwapMultipleBuffers`, `DrvDescribePixelFormat`, `DrvSetPixelFormat`,
   `DrvSwapBuffers`, `DrvPresentBuffers`.
   The driver is **refused** unless (`DrvCreateContext` or
   `DrvCreateLayerContext`), `DrvDeleteContext`, `DrvSetContext` and
   `DrvReleaseContext` exist; when `Flags & 1`, also `DrvDescribePixelFormat`,
   `DrvSetPixelFormat` and `DrvSwapBuffers`; and `DrvValidateVersion(Version)`
   must answer non-zero. It then calls `DrvSetCallbackProcs(9, procs)` with
   `wglCbSetCurrentValue`, `wglCbGetCurrentValue`, `wglCbGetDhglrc`,
   `wglCbGetDdHandle`, `wglCbPresentBuffers`, `wglCbGetAdapterLuid`,
   `wglCbCheckFullscreenSupport`, `wglCbPresentToRedirectionSurface`,
   `wglCbSubmitPresentToRedirectionSurface` (table @1800e1630).

**What dxgkrnl does** (`DpiGetAdapterInfo` @dxgkrnl, the strings at line 0x548
of its source): opens the adapter's `PLUGPLAY_REGKEY_DRIVER` key
(`DpiOpenPnpRegistryKey(..., 2, ...)`) - the same software key 045's
`VulkanDriverName` lives in - reads `OpenGLDriverName` and
`OpenGLDriverNameWow` as strings, and `OpenGLVersion`, `OpenGLFlags`,
`OpenGLVersionWow`, `OpenGLFlagsWow` as type-checked `REG_DWORD`s. A name
beginning `#` is redirected to an `OpenGL\DriverName` subkey (not used here).
`PostProcessUMDFileName` translates the name for a process in a VM container
and requires the caller's buffer to be at least 0x210 bytes.

**Consequences.** `OpenGLFlags` must be 1: without it opengl32 describes pixel
formats itself (GDI's generic formats) and never asks the ICD. The ICD exports
the three `Flags & 1` entries, the four context entries, `DrvShareLists`,
`DrvGetProcAddress`, `DrvCopyContext`, `DrvCreateLayerContext` (layer 0 only),
`DrvDescribeLayerPlane` (answers no planes), `DrvSwapLayerBuffers` (main plane
only), `DrvValidateVersion` and `DrvSetCallbackProcs` - 14 exports, each one
line. The palette entries, `DrvSwapMultipleBuffers` and `DrvPresentBuffers` are
not exported: opengl32 tolerates their absence. The choice is made once per
process, so a probe that fails to load the ICD has to be a new process.

**Alternatives rejected.** The global `OpenGLDrivers\<name>` key
(`GetDrvRegInfo` @18006d1ec, the pre-WDDM path): machine-wide, and FR-001 forbids
it.

## 2. Where the ICD's code lives, and how it builds

**Decision.** Portable decisions in `src/icd/opengl/`; what both ICDs share
(`ExtensionPolicy`, `ThreadLock`, `LockedTransport`, `LogSink`, the connection's
adapter search, the chunk binding) moves to `src/icd/common/`. The Windows shim
is `src/um/opengl/` (`rvkgl_exports.cpp`, `rvkgl.def`, `GdiWindowSurface`,
`TlsThreadSlot`), and the Windows pieces 045 wrote for `rvkvk.dll`
(`ProcessHeap`, `SrwLock`, `FileLog`) move to `src/um/icd/`. `rvkgl.dll` links
`src/icd/common`, `src/icd/opengl`, the guest transport and the generated
OpenGL client and entries; it imports gdi32, kernel32, ntdll and **user32**
(`WindowFromDC`, `GetClientRect`), so `check_guest_image.py --dll` gains
`--allow-import user32.dll` for it.

**The generated tree needs three emitter changes** (063's generator, vendored):

- The ICD is handed no GL symbol by name: `opengl32` calls through a
  `GLCLTPROCTABLE` the ICD returns from `DrvSetContext` (`__wglSetProcTable`
  @18000c080 accepts `cEntries` 0x150 = 336 or 0x132 = 306 and copies the
  entries into the thread's dispatch), and asks `DrvGetProcAddress` for every
  other name (`wglGetProcAddress` @1800303f0 calls it through the driver record
  at +0x48). The table's order is Microsoft's ABI and is read from
  `glCltRGBAProcTable` (336 entries, `glcltNewList` first, `glcltVertex3f` at
  136, `glcltEnable` at 215): a tool (`tools/re/gl_proc_table.py`) writes it to
  `tools/codegen/profiles/opengl_proc_table.txt`, which is committed data the
  emitter reads, and a test holds the file to the binary when the workspace
  exists. The emitter writes `gl_icd_table.h` as that table plus the
  by-name rows for `DrvGetProcAddress`, and stops writing `gl_icd_exports.def`
  (an ICD exports no `gl*`).
- The entries' `extern "C"` names become internal (`Wrap_gl*`, as Vulkan's
  are), so no GL symbol can collide with `opengl32`'s.
- Every synchronous call opens `SyncCallScope` (045 T046), as the Vulkan and
  OpenCL emitters already do.

The hooks the generated code calls (`::av::glicd::wire`, `pixel_bytes`,
`pixel_buffer_bound`, `framebuffer_name`, `flush_persistent_mappings`,
`forget_bindings`, `compressed_bytes`, `level_bytes`, `compressed_level_bytes`,
`query_buffer_bound`, `offsets_of`, the three `skip_*`, and
`::av::icd::payload_dropped`) are one-line forwarders in
`src/icd/opengl/gl_hooks.cpp` to the root behind `GlIcdBinding`, the pattern
045's `icd_hooks.cpp` set. 43 entry points are declared and not generated
(client arrays and their draws, buffer object creation and data, program
linking and location queries, `glGetString(i)`, `glPixelStore`, `glFlush`,
`glClientWaitSync`, `glDebugMessageCallback`): the ICD writes them, because
each reads state the host does not have.

**Constitution.** A second ICD root is a second piece of process-lifetime state
behind a C entry point with no context (`DrvValidateVersion` and friends take
no ICD pointer). The constitution's list gains `rvkgl.dll`'s root and
`GlIcdBinding` (PATCH), as 045 added Vulkan's.

## 3. Contexts, threads and the current context

**Decision.** `DrvCreateContext(hdc)` answers a `DHGLRC` naming a
`GuestGlContext` in the root's pool: nothing on the host yet. `DrvSetContext(hdc,
dhglrc, pfnSetProcTable)` makes the host context on first use, records the
thread as owner, binds the window's back buffer, and answers the proc table.
A context current on another thread is refused (`NULL`, `ERROR_BUSY`), leaving
that thread's state alone (US3.2). `DrvReleaseContext` clears the thread's
current context. The current context is thread state: `CurrentContexts` over a
`ThreadSlot` seam (`TlsAlloc`/`TlsGetValue` on Windows - a freestanding DLL has
no implicit TLS without the CRT's `_tls_used` - and a thread-keyed table in
tests).

**One transport per context.** Each `GuestGlContext` owns an rvk OpenGL context
(`KmtTransport` of `SubApi::opengl`) on the process's one `GuestConnection`
device, so a process's contexts share the per-process link of 043 and each maps
to its own host context (US3.1). Commands and replies use 045's sizes (ring
order 20, 256 KiB commands, 64 KiB replies).

## 4. Context options and share lists

**Decision.** `wglCreateContextAttribsARB` (answered through `DrvGetProcAddress`)
and `DrvCreateContext` both record `GlContextOptions` (major, minor, profile
mask, flags, share context). The host context is made at the first
`DrvSetContext` with those options: the `RvkContextRecord` carries them in its
reserved tail (still 32 bytes; all zero keeps 043's meaning), the KMD judges them (a share context
must be a live rvk OpenGL context of the same process and device) and passes
them in `ContextSetup` with the share context's device id, and the executor
ABI gains `create_with_options` (version 4: `create`'s arguments plus the
options and the share context's `RvkExecutor`, which the device resolves from
its own context table and refuses when it is not an OpenGL context of the same
GPU) so `OpenGlBackend::initialize` makes the EGL context with
`EGL_CONTEXT_MAJOR/MINOR_VERSION`, `EGL_CONTEXT_OPENGL_PROFILE_MASK`, the
debug/forward-compatible/robust flags and the share context's EGL context. A
version 3 library is never handed options: the device makes a default context
and refuses a context whose options are not the defaults. A version the host cannot give is the host's refusal, answered as
`ERROR_INVALID_VERSION_ARB` (0x2095) and `ERROR_INVALID_PROFILE_ARB` (0x2096)
as `WGL_ARB_create_context` specifies (US1.4). `DrvShareLists(a, b)` records
the share target while neither context has been made current; after that it
fails, as Microsoft's implementation does once objects may exist (US2.3).

**Rationale.** EGL can share only at creation; a lazily-made host context is
how an ICD honours `wglShareLists` after `wglCreateContext`.

**Risk.** Two contexts of one share group live on two device workers: EGL allows
a share context current on another thread at creation on Mesa and NVIDIA
(checked in the executor's test on both host GPUs before anything relies on it).

**Legacy creation before 051.** A `DrvCreateContext` context is core profile at
the host's version capped at 4.6. The 336-entry table's GL 1.1 entries that the
core command set lacks (`glBegin`, ...) record `GL_INVALID_OPERATION` in the
context's local error, which `glGetError` returns before the host's.

## 5. Bulk data and buffer mappings

**Decision.** The blob arena 043 moved here is a 045 host-visible chunk:
`KmtBlobArena` allocates, locks and binds a `host_visible_memory` chunk
(16 MiB, or the lease's size) to the GL context and sends `IMPORT_BLOB`;
`OpenGlBackend::adopt` takes the imported pointer as the region its
bulk-data commands read and write (today it refuses every import). Leases are
bump-allocated and retired once the command that uses them has been answered.
Arrays of `kPixelBlobFloor` bytes or more cross as leases; smaller ones stay
inline. The generated client and `src/proto/client_blob_arena.hpp` disagree
today (the client names `ClientBlobArena::Lease` with `ok()`, `data`, `offset`
and a `kPixelBlobFloor` constant from `gl_helpers.h`; the class has
`ClientBlobLease` and `taken()`), which is part of why the client does not link:
the emitter follows the class, and `gl_helpers.h` is the ICD's hook header
(`src/icd/opengl/gl_helpers.h`, found on the include path the generated tree
uses) defining the floor.

`glMapBuffer(Range)` and persistent mappings: GL has no host-pointer import on
both host drivers (`GL_EXT_external_objects_fd` takes dma-bufs Mesa exports and
NVIDIA refuses; `GL_AMD_pinned_memory` is Mesa-only), so a `BufferMapping` is a
lease filled by a `glGetBufferSubData` into the arena at map (skipped for
`GL_MAP_INVALIDATE_*` and write-only maps) and written back by a
`glBufferSubData` from the arena at unmap or at the next command for a
persistent map (`flush_persistent_mappings`). The host reads and writes the
arena in place, so the only copy is the driver's own. The path is logged once
per context (FR-006).

## 6. Presentation before 051

**Decision.** Each `(window, context)` pair gets a host framebuffer object with
renderbuffers matching the window's pixel format and client size; the
generated `framebuffer_name` hook maps the application's framebuffer 0 to it.
`DrvSwapBuffers(hdc)` resolves a multisampled buffer, reads the colour buffer
with `glReadPixels(GL_BGRA, GL_UNSIGNED_BYTE)` into an arena lease, and
`SetDIBitsToDevice`s it to the DC bottom-up; a client size that changed since
the last frame reallocates the renderbuffers before the next one.
`DrvSwapLayerBuffers` does the same for the main plane and refuses others.

**Why not `wglCbPresentBuffers`.** It is opengl32's path into DWM redirection
for a driver presenting through its own allocations - 051's work, with the
KMD's present.

## 7. Pixel formats and WGL extensions

**Decision.** `PixelFormatTable` is fixed at build time: RGBA8 with depth/stencil
{none, D24S8, D32} × {single, double} × {sRGB off, on} × {samples 0, 4}, all
`PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_SUPPORT_COMPOSITION`, indices
stable for the ICD's lifetime (US2.1). No palette formats, no layer planes, no
`PFD_DRAW_TO_BITMAP`: a memory DC is declined (`pwndGetFromMemDC` finds no
window, and `DrvSetPixelFormat` refuses). `SetPixelFormat` on a window that
already has one fails without changing the table. `WGL_ARB_pixel_format`'s
three queries and `wglChoosePixelFormatARB` read attribute lists the
application wrote - hostile input with a fuzz target. Reported WGL extensions:
`WGL_ARB_extensions_string`, `WGL_ARB_pixel_format`, `WGL_ARB_create_context`,
`WGL_ARB_create_context_profile`, `WGL_ARB_create_context_robustness`,
`WGL_ARB_multisample`, `WGL_ARB_framebuffer_sRGB` - each only with the test the
policy names (constitution VI).

## 8. Device loss

**Decision.** A faulted transport latches the context lost: with
`WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB` and `GL_LOSE_CONTEXT_ON_RESET_ARB`,
`glGetGraphicsResetStatus` answers `GL_UNKNOWN_CONTEXT_RESET` locally and every
other call is a no-op; without it, `glGetError` answers `GL_OUT_OF_MEMORY` once
and calls are no-ops. Every context on the same device is lost with it; a new
context opens a new rvk context (045's `Connection` behaviour, per context).

## 9. The guest proof

**Decision.** `rvkglprobe.exe` (imports `opengl32.dll`, gdi32, user32, kernel32,
ntdll) creates a window, and steps: `pixel_format`, `context`, `version`
(`GL_VERSION` and `GL_RENDERER` printed), `draw` (a triangle into the default
framebuffer, swapped, read back with `glReadPixels`), `share` (a texture made
in one context read in another after `wglShareLists`), `attribs` (4.7 refused
with `ERROR_INVALID_VERSION_ARB`), `busy` (a context current on one thread
refused on another), `lost` (a forced fault through a probe-only
`DrvGetProcAddress` name, as 045's), `recreated`. The same scenarios run through
the ICD's `Drv*` entry points over `FullStackRig` with a scripted
`GdiWindowSurface`, and `run_guest_suite.py --opengl` runs the probe.

**Session 0.** The runner's processes are session 0 services. `opengl32`'s
fallback (§1) takes the first enumerated adapter when a DC's adapter cannot be
opened, and a session-0 window DC may belong to no display adapter at all. This
is inference, so Phase 2 of the tasks is a live experiment - a minimal probe
calling `SetPixelFormat` and `wglCreateContext` on a session-0 window with the
INF values set and an ICD that only logs - before the ICD is built. If it shows
another ICD (or none), the runner gains an interactive-session launcher:
`--prepare-image` configures autologon and the probe runs as a scheduled task
in the console session (`schtasks /IT`), which 046 and 051 need as well.

## 10. Scope moved

- The 32-bit ICD (`OpenGLDriverNameWow`, US1.2): 061, with 045's.
- `WGL_EXT_swap_control`: 051, which paces presentation.
- The compatibility profile for legacy creation: 051 (already its scope).
- Zero-copy buffer mapping: not reachable on both host drivers (§5); 053
  revisits it with the cross-API sharing work.
