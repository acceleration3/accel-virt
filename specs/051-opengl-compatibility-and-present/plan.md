# Implementation plan: OpenGL compatibility profile and presentation (051)

**Branch**: `051-opengl-compatibility-and-present` · **Spec**: spec.md · **Research**: research.md

## Summary

Feature 050 shipped an OpenGL ICD whose contexts are core profile and whose
`SwapBuffers` reads the back buffer back and blits it with GDI. 274 of the
336 proc-table slots - immediate mode, matrix stacks, display lists,
`glPushAttrib`, selection and feedback - refuse with `GL_INVALID_OPERATION`,
because there was no compatibility context to run them on.

051 asks the host for that context, reports what it was given, and lets the
legacy slots through when the answer was yes. The wire and the host decoders
for every one of them already exist (research §3), so what this feature adds on
the guest side is the ask, the readback of the granted profile, and the gate.

The presentation half is smaller than the spec assumed and is stated plainly:
Vulkan does not present with D3DKMT either, and cannot until feature 055's UMD
exists (research §4). So 051 gives OpenGL the presentation Vulkan has - the
shared window surface and the adapter's vertical blank - plus what the spec
asks on top of it: `wglSwapIntervalEXT`, reallocation on resize, a swap on a
window nobody can see that costs nothing, several windows in one process, a
single-buffered format that presents on `glFlush`, and a front-buffer read that
answers the last presented frame.

## Technical context

C++23 freestanding, no standard library, `av::Status` everywhere. The work is
almost entirely in `src/icd/opengl/` (the portable ICD) and `src/icd/common/`
(what both ICDs share), with one move out of `src/icd/vulkan/`, two rows in
`src/device/executor/` tests to prove the host grants compatibility on both
GPUs, and the guest probe in `src/guest/suite/`. Tests: the ICD's unit tests
over `ScriptedGl`, the full-stack rig for the real host driver, and the guest
suite for the live desktop.

## Classes this feature introduces

| Class | Kind | State it owns | Responsibility |
|---|---|---|---|
| `GrantedProfile` | object | the version and profile mask the host gave, and the ladder rung it came from | What this context actually is, as opposed to what was asked for |
| `SwapInterval` | object | the interval and the blanks waited for one context | How many vertical blanks a swap waits for |
| `WindowVisibility` | object | a window and the surface that answers for it | Whether a frame put on this window now would be seen |
| `FrontBuffer` | object | the last presented frame's pixels and extent for one window | What a read of the front buffer answers |

`FramePacer` is not introduced - it moves from `src/icd/vulkan/` to
`src/icd/common/` unchanged, and the GL ICD uses it (research §5). A row that
turns out to have no state does not become a class.

## Phasing

1. **The ask and the answer.** `GlContextOptions` for a legacy create becomes
   4.6 compatibility with the fallback ladder of research §2; `GuestGlContext`
   learns `GL_CONTEXT_PROFILE_MASK` and `GL_MAJOR/MINOR_VERSION` beside the four
   strings it already learns; `GrantedProfile` holds the answer; `ManualCalls`
   answers `GL_CONTEXT_PROFILE_MASK` from it; the ladder's outcome is logged
   once. Device-side test: a compatibility context on both host GPUs.
2. **The legacy slots.** `legacy_entry` forwards on a context whose granted
   profile is compatibility and refuses otherwise. Display lists, immediate
   mode, matrix stacks, `glPushAttrib`, client arrays, selection and feedback
   go over the wire the generated client already writes. Loopback tests replay
   a compatibility sequence and judge the host's pixels.
3. **Pacing and the interval.** `FramePacer` moves to `src/icd/common/`;
   `SwapInterval` and the two `wglSwapIntervalEXT` entries appear;
   `WGL_EXT_swap_control` stops being withheld.
4. **What a swap does now.** `WindowVisibility` skips a swap nobody can see;
   resize reallocates before the next frame; several windows present
   independently; a single-buffered format presents on `glFlush`;
   `FrontBuffer` answers a front-buffer read.
5. **Live.** The guest suite's OpenGL probe gains a compatibility step, a
   pacing step measured against the adapter's blanks, and a resize step; the
   reference images follow 048's pattern and stay empty where a pin is missing.

## Constitution check

- III: every row above owns state and has one responsibility. No static
  methods, no stateless class; the ICD's `extern "C"` exports stay one line.
- IV: unit tests first over the scripted host, every branch, invariants after
  every mutation, and the hostile input a swap can carry (a window that
  vanished mid-swap, an extent past the 8192 edge, an interval nobody asked
  for) - plus the existing `fuzz_executor_opengl` corpus, which now reaches the
  legacy decoders.
- VI: the profile reported is the profile granted, read from the mask; no
  extension row without a test.

## Risks

- **A host with no compatibility profile.** Both GPUs here have one; the ladder
  and the log line are what a host without one gets, and the ICD keeps 050's
  behaviour exactly.
- **The legacy slots are 274 entry points.** They are generated and already
  decoded host-side; the risk is in the handful `ManualCalls` must keep
  translating (pixel store, binds, draws), which the gate must not bypass.
- **Nothing in this feature makes the desktop composite differently.** The GDI
  blit stays until 055, and the spec's success criteria say so.
