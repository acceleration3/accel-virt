# Research: OpenGL compatibility profile and presentation (051)

Everything here was read out of this repository, out of `dxgkrnl.sys` in
`re/ghidra-proj`, or measured on this host. Nothing is assumed.

## 1. Both host GPUs give a 4.6 compatibility context, and the version string is not how you tell

Measured here (a surfaceless EGL context on each `eglQueryDevicesEXT` device,
the same way `GlDisplay` finds its display):

```
device 0 /dev/dri/card1  core   4.6.0 NVIDIA 580.178.04              mask 0x1
device 0 /dev/dri/card1  compat 4.6.0 NVIDIA 580.178.04              mask 0x2
device 1 /dev/dri/card2  core   4.6 (Core Profile) Mesa 26.2.2       mask 0x1
device 1 /dev/dri/card2  compat 4.6 (Compatibility Profile) Mesa     mask 0x2
```

**Decision.** What profile a context got is read from
`GL_CONTEXT_PROFILE_MASK` (0x9126), never from `GL_VERSION`: NVIDIA's
compatibility string is identical to its core one. The guest learns it the way
it already learns the four strings - one more `glGetIntegerv` round trip in
`GuestGlContext::learn` - so no executor ABI change is needed for it.

**Rationale.** The mask is what GL defines for the question. The string is
vendor prose.

## 2. The host already accepts a compatibility request; nothing asks for one

`GlContextAttributes` (`src/device/executor/gl_context_attributes.cpp:37`)
passes the guest's profile value straight into
`EGL_CONTEXT_OPENGL_PROFILE_MASK`, and `RVK_CONTEXT_PROFILE_COMPATIBILITY` is 2,
which is `EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT`. Its validation accepts
profile 2. So the host half of US1 exists; what is missing is a guest that asks,
a guest that reads back what it got, and a test on both host GPUs (the device
test asserts only `GL_CONTEXT_CORE_PROFILE_BIT` today,
`tests/unit/device/executor/test_opengl_backend_device.cpp:116`).

**Decision.** A legacy `DrvCreateContext`/`DrvCreateLayerContext` asks for
**4.6 compatibility**. If the host refuses it asks for the driver's own default
context (no version, no profile - what an OpenGL 1.x application got before
profiles existed), and if that is refused it asks for 4.6 core, which is 050's
behaviour. Whatever comes back is reported as it is, and the ladder's outcome
is one log line naming the host's answer (US1.4, constitution VI).

## 3. Display lists, and 273 other entry points, are refused in the guest

`src/proto/generated/opengl/gl_icd_table.h:35` fills the proc table's legacy
slots - `glNewList`, `glEndList`, `glCallList`, `glCallLists`, `glDeleteLists`,
`glGenLists`, `glListBase` and 267 more - with `legacy_entry(...)`, which reaches
`GlIcdRoot::record_legacy` and records `GL_INVALID_OPERATION` without sending
anything. The wire commands and the host decoders for all of them exist
(`rvk_glCallLists` is `kCmd_glCallLists`, decoded at
`gl_dispatch_server.cpp:24877` straight onto the host's `glCallLists`).

**Decision.** A legacy slot forwards when the context it is called on was
granted a compatibility profile, and keeps recording `GL_INVALID_OPERATION`
when it was not. The gate is the granted mask of §1, so a core context behaves
exactly as 050 tested it. A display list therefore lives on the host: the guest
sends `glNewList`, the commands between it and `glEndList`, and later one
`glCallLists` - never a replay (US1.3).

**Rationale.** The generated client already encodes these; the refusal was 050's
honest report that no compatibility context existed to run them on.

## 4. There is no D3DKMT present to share, because feature 046 has none either

The spec asks SwapBuffers to present "through the same presentation path as
Vulkan's swapchain (feature 046): presented with D3DKMT and composed by DWM,
with no read-back and no GDI blit". **That path does not exist.** 046 presents
by copying the swapchain image into guest-visible memory, orienting it on the
CPU and calling `SetDIBitsToDevice` on the window's own DC
(`src/icd/vulkan/swapchain.cpp:208`, `src/um/icd/gdi_window_surface.cpp:26`),
and it recorded why (`docs/kb/07-usermode.md:220`): reversing `dxgkrnl.sys`
shows `DXGDEVICE::AllowLegacyPresent` (@14036fb0c) refuses a legacy present
unless the device owns the VidPn source, and the redirected present DWM
composites comes from a user-mode driver's redirection surface, which a process
with no UMD never creates. 046 deferred its own FR-001 to feature 055 for that
reason.

The other half is just as hard: GL cannot render into guest memory at all.
`OpenGlBackend::adopt` takes `RVK_BLOB_HOST_POINTER` as a raw region and
deliberately makes it no GL object, because neither host driver imports a host
pointer into GL (`specs/050-opengl-icd/research.md`: Mesa's
`GL_EXT_external_objects_fd` takes dma-bufs, NVIDIA refuses them;
`GL_AMD_pinned_memory` is Mesa-only). Vulkan renders into guest pages only
because `VK_EXT_external_memory_host` exists and GL has no equivalent.

**Decision.** 051 gives OpenGL **the presentation Vulkan has**, not a
presentation Vulkan lacks: the same window surface, the same vertical-blank
pacing, and the interval, resize, occlusion, multi-window and single-buffer
behaviour the spec asks for on top of it. The D3DKMT present and the last copy
go with 055's UMD, for both APIs at once, and this is written into the spec's
success criteria rather than left as a silent miss.

**Alternative considered and rejected.** Crossing the frame GL → Vulkan on the
host (`GL_EXT_memory_object_fd` is present on both host GPUs, measured here)
so the Vulkan backend could write the guest's pages. It moves the copy from the
host GPU to the host GPU, keeps the GDI blit that is the actual cost, and puts
an interop path under the present of every GL application for a benefit only
055 can collect.

## 5. Pacing already exists, in the wrong directory

`FramePacer` (`src/icd/vulkan/frame_pacer.hpp`) holds a present until the
adapter's next vertical blank through `Kmt::wait_for_vertical_blank`, counts the
blanks it waited and logs a failure once. Its includes are `Kmt`, `LogSink` and
`Status` - nothing Vulkan.

**Decision.** It moves to `src/icd/common/frame_pacer.*` unchanged, and both
ICDs use it. `wglSwapIntervalEXT(n)` waits `n` blanks before returning from
`SwapBuffers`; `0` waits none. The Vulkan tests move with it.

## 6. What "occluded" can honestly mean here

There is no DWM occlusion query in an ICD, and 046 has none either. What a
window can be asked for is `IsIconic` and its client rectangle.

**Decision.** A swap on a minimized window, or one whose client area is empty,
skips the read-back and the blit and returns success - the render thread is
never blocked and no host GPU time is spent on pixels nobody sees. A window
merely covered by another is not detectable from here, and the spec says so
rather than pretending.

## 7. Single-buffered formats, and the front buffer

`PixelFormatTable` offers both single- and double-buffered formats
(`src/icd/opengl/pixel_format_table.cpp:47`). Windows presents a
single-buffered format on `glFlush`; `SwapBuffers` on one is a no-op that
succeeds.

**Decision.** `ManualCalls` gains `flush`, which presents the window when the
current context's pixel format is single-buffered and otherwise forwards.
`glReadBuffer(GL_FRONT)` and a read from it answer the last presented image,
which `BackBufferPresenter` already holds in its guest pixel buffer - so the
front buffer is a read of that buffer, not another round trip to the host.

## 8. Extensions this feature may report

`GlIcdRoot` reports no GL extension at all today (`reported_extensions` is
null), by policy: a row arrives with a test that proves it.

**Decision.** 051 adds exactly two rows, both with tests: `GL_ARB_compatibility`
where the granted mask says compatibility (Mesa reports it; NVIDIA does not, so
the row is intersected with the host's list like every other), and
`WGL_EXT_swap_control` in the WGL string, which stops being withheld
(`src/icd/opengl/gl_extension_policy.cpp:14`) once there is an interval to
honour.

## 9. The thirteen entry points that stay refused

Thirteen compatibility entry points are not generated forwarders but the ICD's
own (`src/icd/opengl/manual_entries.cpp`): `glVertexPointer`, `glColorPointer`,
`glNormalPointer`, `glTexCoordPointer`, `glIndexPointer`, `glEdgeFlagPointer`,
`glFogCoordPointer`, `glSecondaryColorPointer`, `glInterleavedArrays`,
`glEnableClientState`, `glDisableClientState`, `glClientActiveTexture` and
`glArrayElement`. They are the fixed-function **client arrays**: a pointer into
the application's own memory that GL reads at every draw.

**Decision.** They keep recording `GL_INVALID_OPERATION`, as they did before
051, and the gate above does not open them.

**Rationale.** This is the same refusal 050 already makes for a modern vertex
attribute with no buffer bound (`test_manual_calls`: "a client-side array is
invalid operation"), and for the same reason: the transport sends commands, not
the guest address space, so an array read at draw time would have to be copied
across on every draw, at a size nobody states. A display list, immediate mode,
the matrix stacks, `glPushAttrib`, selection and feedback need none of that and
all work; an application that binds a buffer gets the modern path.

Closing this means sizing a client array from the draw that reads it and
copying that range per draw, which is a feature's work and is written down here
rather than half-done.
