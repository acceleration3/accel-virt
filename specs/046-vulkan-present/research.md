# Research: Vulkan presentation (046)

## 1. The present path available before a UMD

**Decision.** The ICD implements `VK_KHR_surface`, `VK_KHR_win32_surface` and
`VK_KHR_swapchain` itself. A swapchain image is a host `VkImage`; presenting
one records a copy of it into a host-visible buffer whose memory is the guest
chunk 045 imports (so the host writes the guest's pages directly), waits for
it, and draws the bytes on the window's DC through `WindowSurface`.

**Rationale.** DWM composes GDI windows; nothing else composes a window of a
process with no D3D UMD. `NtGdiDdDDIPresent` (dxgkrnl.sys @140336990) sends a
windowed present either to a redirection surface or, when
`DXGDEVICE::AllowLegacyPresent` (@14036fb0c) holds - the process owns the VidPn
source - to the primary. The redirection surface belongs to a D3D runtime and
UMD (055).

**Alternatives.** Legacy present to the primary: needs exclusive ownership,
which a windowed application does not take, and session 0 cannot take on the
console. A host window: not the guest desktop (kb 07 option 1).

## 2. Surface properties

`currentExtent` is the window's client size (`GetClientRect`), 0x0 for a
minimized window; image counts 2 to 8; identity transform; opaque
composition; usage colour attachment, transfer source and destination,
sampled; formats `B8G8R8A8_UNORM`, `B8G8R8A8_SRGB`, `R8G8B8A8_UNORM`,
`R8G8B8A8_SRGB`, all `SRGB_NONLINEAR`; modes FIFO and IMMEDIATE.

## 3. Out of date

A present or acquire whose window's client size differs from the swapchain's
extent answers `VK_ERROR_OUT_OF_DATE_KHR` (as Windows drivers do); a window
that is gone answers `VK_ERROR_SURFACE_LOST_KHR`; creating a swapchain for a
0x0 window answers `VK_ERROR_INITIALIZATION_FAILED`.

## 4. Pacing

FIFO waits for the adapter's next vertical blank on source 0 through
`Kmt::wait_for_vertical_blank` after the frame is on the window; IMMEDIATE
does not wait.

## 5. The desktop

`DxgkDdiPresent`'s blit and colour fill are executed by the KMD itself at the
call, between allocations whose pages it knows, after the DMA command is
written; a flip is left to `SetVidPnSourceAddress`, which already moves the
scanout.
