---
description: "Tasks: Vulkan presentation (046)"
---

# Tasks: Vulkan presentation through rvkgpu

**Input**: plan.md, spec.md (Clarifications), research.md, data-model.md, contracts/wsi.md, quickstart.md

**Tests**: written first (constitution IV); invariants after every mutation; hostile tables and a fuzz target for the application's create structures. Commit at every checkpoint once `ctest --test-dir build -LE tools -j6` passes; the coverage build before a new class is committed.

## Phase 1: Setup

- [X] T001 Move `WindowSurface` from `src/icd/opengl/` to `src/icd/common/window_surface.hpp` (namespace `av::icd`), adding `blit_window(window, width, height, bgra_bottom_up)` (draw on the window's own DC) and `read_window(window, width, height, bgra_bottom_up)`; `ScriptedWindowSurface` and `GdiWindowSurface` follow (`src/um/icd/gdi_window_surface.*`); the GL ICD's users and tests unchanged in behaviour
- [X] T002 The instance extension table gains `VK_KHR_surface` (25) and `VK_KHR_win32_surface` (6); `VK_KHR_surface`, `VK_KHR_win32_surface` and `VK_KHR_swapchain` leave `ExtensionPolicy`'s withheld list; `VK_KHR_swapchain` (70) becomes a policy row naming its test

**Checkpoint**: commit.

## Phase 2: US1 - A swapchain presents (P1)

- [X] T003 [US1] `SurfaceTable` (system) and the surface entries (`vkCreateWin32SurfaceKHR`, `vkDestroySurfaceKHR`, surface support queries) in `src/icd/vulkan/`; tests in `tests/unit/icd/vulkan/test_surfaces.cpp`
- [X] T004 [US1] `SurfaceProperties` (object): capabilities (1 and 2), formats (1 and 2), present modes per research.md §2-3, count/array protocols, a gone window; tests `test_surface_properties.cpp`
- [X] T005 [US1] `SwapchainImages` (object) and `Swapchain` (object): create (every refusal of contracts/wsi.md, a hostile table over `VkSwapchainCreateInfoKHR`), images, acquire, present (the host copy through a host-visible buffer, RGBA to BGRA, bottom-up, `blit_window`), destroy; `SwapchainTable` (system) and the entries; tests `test_swapchain.cpp` over IcdRig and the scripted host (the commands a present sends, the bytes blitted, the allocation-failure sweep); `tests/fuzz/fuzz_swapchain_create.cpp` with a corpus
- [X] T006 [US1] `FramePacer` (object): FIFO waits for the next vertical blank after the frame is drawn; IMMEDIATE does not; a failed wait is logged once and the present still succeeds; tests `test_frame_pacer.cpp`
- [ ] T007 [US1] `test_fullstack_vulkan_icd.cpp`: a swapchain on the host GPU presents a drawn frame through the ICD; the blitted bytes are the render's; `build/images/vulkan-present.bmp`

**Checkpoint**: commit.

## Phase 3: US2 - Windows that change (P1)

- [X] T008 [US2] Resize: acquire and present answer `VK_ERROR_OUT_OF_DATE_KHR` after the client size changes; a swapchain recreated with `oldSwapchain` presents at the new extent; a minimized (0x0) window refuses creation; a gone window is `VK_ERROR_SURFACE_LOST_KHR`; tests in `test_swapchain.cpp`

**Checkpoint**: commit.

## Phase 4: The probe and the guest

- [ ] T009 `VulkanEntry` gains the WSI calls; `ScriptedVulkan` answers them; `VulkanProbe` gains `surface`, `present` (60 FIFO frames of the draw's triangle; the window read back through `read_window`, every pixel the colour; saved as `vulkan-present`), `pacing` (the 60 frames' rate within 2% of the vertical blank rate the adapter measures) and `resized` (a resize, out of date, recreated, presented); tests `test_vulkan_probe.cpp`; `rvkvkprobe.exe` makes a 64x64 `WS_POPUP` window (the GL probe's `ProbeWindow`, moved to `src/um/probe/`)
- [ ] T010 `run_guest_suite.py --vulkan` requires the new steps and pulls `vulkan-present`; tests in `test_run_guest_suite.py`

**Checkpoint**: commit.

## Phase 5: The desktop reaches the scanout (FR-004)

- [ ] T011 `PresentBlit` (object) in `src/core/present/`: a blit or colour fill between allocations whose pages the KMD knows (aperture pages), per sub-rectangle, clipped to both; `Adapter::present` runs it after writing the DMA command; tests `test_present_blit.cpp` (every rectangle edge, a source without pages, overlapping rectangles, a rectangle past either allocation)
- [ ] T012 Live: the suite boot's screendump shows the desktop (not all black); record what DWM presents from

**Checkpoint**: commit.

## Phase 6: Polish and merge

- [ ] T013 docs: `docs/testing.md` (WSI layers, the present step, the desktop), `docs/kb/07-usermode.md` (the present path before a UMD, citing the RE)
- [ ] T014 Coverage and fuzz smoke; `AV_GATE_JOBS=6 AV_GUEST_IMAGE=... tools/gate.sh`; merge; push
