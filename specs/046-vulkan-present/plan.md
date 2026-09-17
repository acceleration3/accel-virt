# Implementation plan: Vulkan presentation (046)

**Branch**: `046-vulkan-present` · **Spec**: spec.md · **Research**: research.md

## Summary

WSI in rvkvk.dll over host images and a host-visible read-back buffer, put on
the window through `WindowSurface` (GDI) until a UMD exists (055); FIFO paced on
the adapter's vertical blank; the probe proves a frame on the window, its rate
and a resize; the KMD executes `DxgkDdiPresent`'s blits so the desktop reaches
the scanout.

## Technical context

C++23 freestanding (constitution); `src/icd/vulkan` (portable, gates apply),
`src/um` shims, `src/core/present` for the KMD blit; tests over IcdRig,
ScriptedHost, MockPlatform/fake dxgkrnl, FullStackRig on the host GPU; live in
the gate's real-executor boot.

## Constitution check

- III: every class in the spec's table owns state; WSI state lives in the
  root's pools, no new process-lifetime state.
- IV: tests first, hostile tables for the caller's create info (the surface and
  swapchain create structures come from the application), fuzz target
  `fuzz_swapchain_create`.
- VI: only FIFO and IMMEDIATE, four formats; FR-001 deviation recorded
  (Clarifications) - GDI until 055.

## Complexity tracking

| Deviation | Why | Rejected alternative |
|---|---|---|
| GDI blit for windowed present (FR-001) | no redirection surface without a UMD | D3DKMT legacy present - needs VidPn ownership |
