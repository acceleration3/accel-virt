# Data model: OpenGL compatibility profile and presentation (051)

## GrantedProfile (object, one per `GuestGlContext`)

| Field | Type | Meaning |
|---|---|---|
| `asked_` | `GlContextOptions` | what the ICD asked the host for |
| `major_`, `minor_` | `uint32_t` | `GL_MAJOR_VERSION`, `GL_MINOR_VERSION` as the host answered |
| `mask_` | `uint32_t` | `GL_CONTEXT_PROFILE_MASK`: 1 core, 2 compatibility, 0 neither (a pre-3.2 context) |
| `rung_` | `Rung` | which ask succeeded: `compatibility_46`, `host_default`, `core_46` |

Rules:

- `mask_` is read from the host, never inferred from `GL_VERSION` (research §1).
- `legacy_calls_live()` is `mask_ == compatibility_bit || (mask_ == 0 && major_ < 3)`:
  a driver's default context predates profiles and has the legacy commands.
- The version reported to the application is `major_.minor_` capped at 4.6, as
  050 caps it.
- A profile the ICD did not ask for is still reported as it is.

## SwapInterval (object, one per `GuestGlContext`)

| Field | Type | Meaning |
|---|---|---|
| `interval_` | `uint32_t` | 0 to `max_interval` (4); `wglSwapIntervalEXT` |
| `blanks_` | `uint64_t` | how many blanks this context has waited |

Rules: a negative or out-of-range interval is refused with
`ERROR_INVALID_DATA` and leaves the interval unchanged (`WGL_EXT_swap_control`
defines no tear control here, and the extension that does is not reported).
Interval 0 waits for nothing.

## WindowVisibility (object, built per swap)

| Field | Type | Meaning |
|---|---|---|
| `surface_` | `WindowSurface&` | what answers for the window |
| `window_` | `void *` | the HWND |

Rules: `showing()` is false when the window is iconic or its client area is
empty in either dimension; a swap on such a window reads nothing back, blits
nothing, waits for no blank, and succeeds.

## FrontBuffer (object, one per `BackBufferPresenter`)

| Field | Type | Meaning |
|---|---|---|
| `pixels_` | `uint8_t *` | the last frame presented on this window, BGRA, bottom-up |
| `width_`, `height_` | `uint32_t` | its extent |
| `presented_` | `bool` | whether anything has been presented yet |

Rules: a read of the front buffer before the first present answers the
window-sized black frame GL would have; a read after a resize answers the last
presented extent, not the window's new one.

## What changes shape elsewhere

- `GlContextOptions` gains nothing: the compatibility ask is the existing
  profile field set to `RVK_CONTEXT_PROFILE_COMPATIBILITY`.
- `FramePacer` moves to `src/icd/common/frame_pacer.{hpp,cpp}` unchanged.
- `GlExtensionPolicy` loses `WGL_EXT_swap_control` from `never_reported` and
  gains a `GL_ARB_compatibility` row, both intersected with the host's list.
