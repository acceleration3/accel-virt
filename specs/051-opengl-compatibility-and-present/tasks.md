---
description: "Tasks: OpenGL compatibility profile and presentation (051)"
---

# Tasks: OpenGL compatibility profile and presentation

**Input**: plan.md, spec.md, research.md, data-model.md, contracts/wgl.md

**Tests**: written first (constitution IV). Every branch, invariants after every
mutation, and a hostile table for everything an application hands a swap - a
window destroyed between the read-back and the blit, an extent past the 8192
edge, an interval nobody defined. Commit at every **Checkpoint** once
`ctest --test-dir build -LE tools -j6` passes.

**What this feature does not build**: the D3DKMT present. Feature 046 has none
either, and research.md §4 says why - `DXGDEVICE::AllowLegacyPresent` refuses a
device that does not own the VidPn source, and the redirected present needs a
UMD's redirection surface. It arrives with feature 055, for both APIs at once.

## Phase 1: Foundational - what the host actually gave us

- [X] T001 `GrantedProfile` (object) in `src/icd/opengl/granted_profile.{hpp,cpp}`: the asked options, the host's `GL_MAJOR_VERSION`/`GL_MINOR_VERSION`/`GL_CONTEXT_PROFILE_MASK` and which rung answered (`compatibility_46`, `host_default`, `core_46`); `legacy_calls_live()` is `mask == 2 || (mask == 0 && major < 3)` (data-model.md). Tests `tests/unit/icd/opengl/test_granted_profile.cpp`: each rung, a mask the host never defines, a pre-3.2 default context, and the invariant
- [X] T002 `GuestGlContext::learn` reads the three integers beside the four strings it already learns, in one batch, and holds the answer in a `GrantedProfile`; `ScriptedGl` answers them so a test can make the host say anything. Tests in `tests/unit/icd/opengl/test_guest_gl_context.cpp`: a host that answers nothing leaves the profile unknown and the context still usable

**Checkpoint**: commit. Nothing behaves differently yet; the ICD simply knows what it has.

## Phase 2: US1 - legacy OpenGL applications run (P1)

- [X] T003 [US1] `GlIcdRoot::make_chain` asks the ladder of research.md §2 for a context created without attributes: 4.6 compatibility, then the host's default (no version, no profile), then 4.6 core. Tests: each refusal moves to the next rung, a host that refuses all three fails the make with `ERROR_INVALID_PROFILE_ARB`, and a `wglCreateContextAttribsARB` context is never laddered
- [X] T004 [US1] The rung that answered is logged once per process, naming the host's `GL_VERSION` string (US1.4). Tests: exactly one line however many contexts are made, and the line names the host's answer
- [X] T005 [US1] `ManualCalls::integer` answers `GL_CONTEXT_PROFILE_MASK` (0x9126), `GL_MAJOR_VERSION` and `GL_MINOR_VERSION` from the context's `GrantedProfile` instead of asking the host again. Tests in `tests/unit/icd/opengl/test_manual_calls.cpp`
- [X] T006 [US1] `GlIcdRoot::record_legacy` forwards to the generated entry when the current context's `legacy_calls_live()` and records `GL_INVALID_OPERATION` when it does not (`src/icd/opengl/gl_hooks.cpp`, `gl_icd_root.cpp`). Tests: the same call on a core context and on a compatibility one, and that a context with no current thread still refuses
- [X] T007 [US1] Display lists reach the host: `glNewList`, `glEndList`, `glGenLists`, `glDeleteLists`, `glListBase`, `glCallList` and `glCallLists` are forwarded, and a list called each frame is one command, not a replay. Tests in `tests/unit/icd/opengl/` over `ScriptedGl`, and a loopback test that compiles a list and calls it
- [X] T008 [US1] Immediate mode, matrix stacks, fixed-function lighting and texturing, `glPushAttrib`/`glPopAttrib`, client arrays, selection and feedback run over the wire on a compatibility context. Loopback test `tests/loopback/test_fullstack_opengl_compat.cpp` replays the sequence on the real host driver and judges the pixels, writing `build/images/opengl-compat.bmp`
- [X] T009 [US1] `tests/unit/device/executor/test_opengl_backend_device.cpp` gains a compatibility case: a 4.6 compatibility context on **both** host GPUs reports `GL_CONTEXT_PROFILE_MASK == 2`, and a host that refuses is a skip with its reason, not a failure
- [X] T010 [US1] `GlExtensionPolicy` gains a `GL_ARB_compatibility` row, intersected with the host's `glGetStringi` list as every row is (NVIDIA does not report it, Mesa does). Tests: the row is reported only on a compatibility context whose host lists it

**Checkpoint**: commit. A legacy application's calls reach the host, and a core context is unchanged from 050.

## Phase 3: US2 - the interval and the blank (P1)

- [X] T011 [US2] `FramePacer` moves from `src/icd/vulkan/frame_pacer.{hpp,cpp}` to `src/icd/common/frame_pacer.{hpp,cpp}` unchanged, with its test; every Vulkan include follows it. `ctest` proves the Vulkan side is untouched
- [X] T012 [US2] `SwapInterval` (object) in `src/icd/opengl/swap_interval.{hpp,cpp}`: the interval (0..4) and the blanks this context has waited; an out-of-range interval is refused and leaves the old one (data-model.md). Tests `tests/unit/icd/opengl/test_swap_interval.cpp`
- [X] T013 [US2] `wglSwapIntervalEXT` and `wglGetSwapIntervalEXT` are answered through `DrvGetProcAddress`, and `WGL_EXT_swap_control` leaves `never_reported` in `src/icd/opengl/gl_extension_policy.cpp`. Tests: the contract's three refusals (out of range, no current context) and the string containing the extension
- [X] T014 [US2] `DrvSwapBuffers` waits the context's interval in vertical blanks through the shared `FramePacer` after the blit, and waits for nothing at interval 0. Tests over `MockKmt`: the blanks waited per swap, and a blank wait that fails logging once and leaving the swap successful

**Checkpoint**: commit. A GL swap is paced like a FIFO Vulkan present.

## Phase 4: US2 - what a swap does now (P1)

- [X] T015 [US2] `WindowVisibility` (object) in `src/icd/opengl/window_visibility.{hpp,cpp}`: a window and the `WindowSurface` that answers for it; `showing()` is false for an iconic window or an empty client area. A swap on such a window reads nothing back, blits nothing, waits for no blank and succeeds (SC-003). Tests: the device's read accounting is untouched across such a swap
- [X] T016 [US2] A window resized between swaps presents the frame it had, then remakes its buffers at the new extent before the next one; an extent past the 8192 edge is refused without touching the old buffers. Tests in `tests/unit/icd/opengl/test_window_framebuffers.cpp`
- [X] T017 [US2] Several windows in one process present independently: each keeps its own extent and its own last frame, and a refusal on one disturbs no other. Covered by `tests/unit/icd/opengl/test_window_framebuffers.cpp` (a later window moves only the bindings left on it; a window whose remake is refused keeps presenting what it had), which is where the per-window state lives - a second copy at the root level tested the scripted host's answer order, not the behaviour
- [X] T018 [US2] `ManualCalls::flush` presents the window when the current context's pixel format is single-buffered and otherwise only flushes; `DrvSwapBuffers` on a single-buffered format succeeds without presenting (contracts/wgl.md). Tests both ways
- [X] T019 [US2] `FrontBuffer` (object) in `src/icd/opengl/front_buffer.{hpp,cpp}`: the last presented frame's pixels and extent for one window, answered to `glReadBuffer(GL_FRONT)` reads with no host round trip; a read before the first present answers a black frame of the window's size. Tests `tests/unit/icd/opengl/test_front_buffer.cpp`
- [X] T020 [US2] Hostile table for the swap path in `tests/unit/icd/opengl/test_gl_icd_root.cpp`: a window destroyed between the read-back and the blit, a DC whose pixel format belongs to another context, a swap with no current context, and an interval set from a thread with none

**Checkpoint**: commit. Every behaviour contracts/wgl.md names is tested.

## Phase 5: Live, and the record

- [ ] T021 The guest suite's OpenGL probe (`src/guest/suite/opengl_probe.*`) gains a `compat` step: a legacy `wglCreateContext`, a display list compiled and called, an immediate-mode triangle, and the profile mask it was granted, reported as a suite result
- [ ] T022 The probe gains `interval` and `resized` steps: 60 swaps at interval 1 take at least 59 of the adapter's blanks (the tolerance 046 set), interval 0 takes fewer, and a resized window keeps presenting at its new extent
- [ ] T023 `tools/guest/run_guest_suite.py` reports the new steps, and `docs/kb/05-scheduling.md`'s OpenGL notes and `docs/roadmap.md`'s capability column record what 051 enables - and that the GDI blit and the read-back stay until 055
- [ ] T024 Coverage and fuzz smoke; `AV_GATE_JOBS=6 AV_GUEST_IMAGE=... tools/gate.sh`; merge; push

## Dependencies

- Phase 1 blocks everything: nothing can be gated on a profile nobody read.
- US1 (Phase 2) and US2 (Phases 3-4) are independent after that; US2's T011 touches the Vulkan ICD and is best done alone.
- Phase 5 needs both.

## MVP

Phases 1-2: a legacy OpenGL application runs. The presentation half is a
separate increment and ships behind it.
