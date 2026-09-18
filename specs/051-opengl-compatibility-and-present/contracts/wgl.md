# Contract: what the ICD answers for 051

## `DrvCreateContext` / `DrvCreateLayerContext` (no attributes)

| The host's answer to | The context is | `glGetIntegerv(GL_CONTEXT_PROFILE_MASK)` | Legacy entry points |
|---|---|---|---|
| 4.6 compatibility | compatibility | 2 | forwarded |
| host default (no version, no profile) | whatever it is | what the host says (often 0) | forwarded when the version is pre-3.2 |
| 4.6 core | core | 1 | `GL_INVALID_OPERATION`, as 050 |
| nothing | creation fails | — | — |

The rung that answered is logged once per process, naming the host's version
string.

## `wglCreateContextAttribsARB`

Unchanged from 050 except that `WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT` now
reaches the host. A host that refuses it answers `ERROR_INVALID_PROFILE_ARB`
(0x2096); a version it refuses answers `ERROR_INVALID_VERSION_ARB` (0x2095).

## `wglSwapIntervalEXT` / `wglGetSwapIntervalEXT`

- Answered through `DrvGetProcAddress`, and `WGL_EXT_swap_control` appears in
  `wglGetExtensionsStringARB`.
- `wglSwapIntervalEXT(n)`: `n` in 0..4 sets the interval of the calling
  thread's current context and returns TRUE; anything else returns FALSE with
  `ERROR_INVALID_DATA`; no current context returns FALSE with
  `ERROR_INVALID_OPERATION`.
- `wglGetSwapIntervalEXT()` answers the current context's interval, or 0 when
  there is none.

## `DrvSwapBuffers`

| The window | What happens |
|---|---|
| visible, double-buffered | resolve, read back, blit, then wait for `interval` vertical blanks |
| minimized or empty | nothing is read, nothing is blitted, no blank is waited for, TRUE |
| resized since the last swap | the frame for the old extent is presented, then the buffers are remade at the new one |
| single-buffered format | TRUE without presenting; `glFlush` is what presents |
| destroyed mid-swap | FALSE, and no other window's buffers are touched |

Several windows in one process present independently: each has its own
`BackBufferPresenter`, its own `FrontBuffer` and its own extent, and a refusal
on one does not disturb another.

## `glFlush`, `glReadBuffer(GL_FRONT)`, `glReadPixels`

- `glFlush` on a context whose pixel format is single-buffered presents the
  window, then flushes.
- `glReadBuffer(GL_FRONT)` followed by a read answers the last presented frame,
  from the guest's own copy of it; no host round trip is made for it.
