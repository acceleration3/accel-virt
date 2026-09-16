# Data model: OpenGL - an installable client driver

## Guest user mode (`src/icd/opengl`)

**GlIcdRoot** - one per process, in `ObjectStorage` in `rvkgl_exports.cpp`;
bound to `GlIcdBinding` for the generated hooks.
- Owns: the injected `BlockSource`, `Kmt`, `ThreadLock`s, `LogSink`,
  `WindowSurface`, `ThreadSlot`; the `PixelFormatTable`, `WindowTable`,
  `CurrentContexts`, `GlExtensionPolicy`; the `ObjectPool<GuestGlContext>`; the
  process's one `GuestConnection` (opened at the first host context, closed with
  the last); the nine callback procs `DrvSetCallbackProcs` gave.
- Rules: nothing is opened before the first `DrvSetContext`;
  `DrvValidateVersion` answers true only for 2; every `DHGLRC` and `HDC` is
  judged before use (a `DHGLRC` is an index into the pool with a generation, so
  a stale or forged one is refused).

**PixelFormatTable** - one, constant.
- Rows: colour bits, alpha, depth, stencil, double buffer, sRGB, samples, flags.
- Rules: index 1..N stable; `describe(index, bytes, pfd)` answers N for any
  index, fills the `PIXELFORMATDESCRIPTOR` only for an index in range and a
  size at least its own.

**PixelFormatQuery** - one per call.
- Fields: the table, the attribute list (pointer, count or terminator).
- Rules: `get_attribs(index, names, count, values)`,
  `choose(int_attribs, float_attribs, max, formats, count)`; an unknown
  attribute, a list past 128 pairs, an unterminated list and a format out of
  range answer `ERROR_INVALID_PARAMETER`/`ERROR_INVALID_PIXEL_FORMAT`.

**ContextAttributes** - one per `wglCreateContextAttribsARB` call.
- Fields: major, minor, profile mask, flags, layer plane, reset strategy.
- Rules: pairs until 0, at most 64; a repeated name, an unknown name, a
  version below 1.0 or with a minor past what that major has, both profile
  bits, a flag outside the four defined and a layer other than 0 answer the
  ARB errors (`ERROR_INVALID_VERSION_ARB`, `ERROR_INVALID_PROFILE_ARB`,
  `ERROR_INVALID_PARAMETER`); produces `GlContextOptions`.

**WindowTable** - one per process.
- Rows (at most 256): `HWND`, pixel format index, client size, and per context
  that drew on it a `BackBufferPresenter`.
- Rules: a window's format is set once; a DC with no window is declined; a row
  goes when the last context that drew on it is deleted or the window is gone
  at the next swap.

**GuestGlContext** - one per `DHGLRC`.
- Fields: `GlContextOptions`, share target, pixel format, owner thread (0 when
  not current), `KmtTransport` of `SubApi::opengl` and its `LockedTransport`,
  `KmtBlobArena`, `ClientArrayState`, the `BufferMapping`s (at most 64),
  local error, `lost`, `made` (host context exists).
- States: `created` -> (`DrvSetContext`) `made`/`current` <-> `released` ->
  `deleted`; `made` -> `lost` on a transport fault. `DrvShareLists` only in
  `created`.

**CurrentContexts** - one per process.
- Owns: the `ThreadSlot` (the calling thread's context), the count of
  contexts current anywhere.
- Rules: a context is current on at most one thread; `wire()` answers the
  calling thread's context's transport, or null (a GL call with nothing current
  is a no-op, as on Windows).

**ClientArrayState** - one per context.
- Fields: array/element/pixel-pack/pixel-unpack/query buffer bindings, the
  client-side vertex arrays (pointer, size, type, stride, enabled) of the
  current VAO, pixel store state, the program's attribute/uniform names asked.
- Rules: what the generated `pixel_bytes`, `pixel_buffer_bound`,
  `query_buffer_bound`, `offsets_of` hooks answer; a client array's bytes for
  a draw are sized from the draw's range and never read past the count.

**BufferMapping** - one per mapped buffer range.
- Fields: buffer name, offset, length, access, the arena lease, persistent.
- Rules: filled at map unless invalidating or write-only; written back at unmap
  (or before the next command for a persistent, writable map); an unmap of a
  range not mapped is `GL_INVALID_OPERATION`.

**BackBufferPresenter** - one per (window, context).
- Fields: host framebuffer name, colour/depth renderbuffers, their size and
  samples, the read-back lease.
- Rules: resized before the first frame after the window's client size
  changes; a swap resolves, reads back and blits; a failed blit is logged, not
  fatal.

**GlExtensionPolicy** - `ExtensionPolicy` rows for GL and WGL names (each with
its test), intersected with `gl_tier.h` and the host's `glGetStringi` list.

**Seams**: `WindowSurface` (`window_of(hdc)`, `client_size(hwnd)`,
`blit(hdc, width, height, bgra_bottom_up)`) - `GdiWindowSurface` and
`ScriptedWindowSurface`; `ThreadSlot` (`get()`, `set(value)`, `thread_id()`) -
`TlsThreadSlot` and `ScriptedThreadSlot` (the test names the thread).

## Guest transport (`src/guest/transport`)

**KmtBlobArena** - one per GL context: a 045 `HostVisibleChunk` (16 MiB, or a
lease's size up to 256 MiB) bound to the context and imported with
`IMPORT_BLOB`; the `ClientBlobArena` the generated client leases from.
- Rules: `take` bumps, `give_back` rewinds only the top lease, all leases retire
  after the command that used them has been answered; no arena answers
  not-ready, and the client sends the bytes inline.

## Protocol (`src/proto/context`)

**GlContextOptions** - major, minor, profile, flags, share context id (0 none).

**RvkContextRecord** - still 32 bytes; its reserved tail carries the options
for `SubApi::opengl` (all zero keeps 043's meaning and is required for every
other sub-API). See contracts/context_options.md.

## KMD (`src/core/rvk`)

`RvkContextTable` resolves a record's share context to a live rvk OpenGL
context of the same process and device, else `STATUS_INVALID_PARAMETER`, and
puts the host context id into `ContextSetup`.

## Device and executor

The device resolves `ContextSetup`'s share context id to that context's
`RvkExecutor` (same GPU, OpenGL, live) and passes it to `create_with_options`;
anything else refuses the context create.

`OpenGlBackend::initialize(display, options, share)` makes the EGL context with
the options; `adopt(blob)` takes a host-pointer import as its region (one at a
time; a second import is refused).
