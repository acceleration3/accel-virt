# 07 — User mode: the UMD DDI, and where vkd3d/DXVK actually sit

## The normal picture

A vendor ships a user-mode driver DLL named in the INF. The D3D12 runtime
(`D3D12Core.dll`) loads it and calls `OpenAdapter10_2` / the
`D3D12DDI_*` function tables in `d3d12umddi.h`. The UMD:

- turns D3D12 command list recording into vendor command-buffer bytes,
- compiles DXIL to the vendor ISA,
- calls `D3DKMT*` thunks in `gdi32.dll` to create allocations, contexts and
  fences, and to submit.

The DMA buffer contents that reach your KMD's `DxgkDdiRender` come from here.

## Where vkd3d-proton and DXVK actually sit

**They do not implement the UMD DDI.** `vkd3d-proton` ships a replacement
`d3d12.dll`; DXVK ships replacement `d3d11.dll` / `dxgi.dll`. They sit *above*
the runtime, not below it, and they call Vulkan.

```
    normal:                        with vkd3d-proton:
  app                            app
   └ d3d12.dll (MS)               └ d3d12.dll  (vkd3d-proton)
      └ D3D12Core.dll (MS)            └ vulkan-1.dll
         └ vendor UMD                     └ your Vulkan ICD
            └ D3DKMT → dxgkrnl               └ ring → host
               └ your KMD
```

This has a direct consequence for the plan: **for the vkd3d/DXVK path you do
not need to write a D3D12 UMD at all.** The commands never enter the Microsoft
runtime, so no `d3d12umddi.h` table is ever called.

## But you still need a UMD, for a different reason

Three things break with no UMD present:

1. **DWM.** The desktop compositor creates a real D3D device on the real
   adapter. If your adapter has no working UMD, DWM falls back to WARP or
   fails, and everything composites on the CPU.
2. **DXGI enumeration.** `IDXGIFactory::EnumAdapters` and DXCore report your
   adapter's capabilities from the driver. Applications — including ones you
   care about — pick adapters based on this.
3. **Anything that is not vkd3d.** Media Foundation, Direct2D, the browser's
   compositor, and any app shipping its own D3D11 all go through the Microsoft
   runtime.

So the realistic shape is:

- **`d3d12.dll` / `d3d11.dll` replaced by vkd3d-proton / DXVK** for the
  applications you control. Fast path, no UMD DDI involved.
- **A minimal but real UMD** for everything else, so the adapter is a
  first-class citizen. That UMD can itself be a thin layer over the same
  Vulkan remoting ring — a second consumer of the same wire protocol.

Doing the UMD second, after the KMD and the Vulkan ICD work, is the right
order. The KMD plus the ICD plus vkd3d gets you a working accelerated D3D12
application; the UMD gets you a working *desktop*.

## The D3DKMT boundary, which you do need either way

Even the vkd3d path reaches the kernel, because a Windows Vulkan ICD must:

- enumerate the adapter (`D3DKMTEnumAdapters2`, `D3DKMTOpenAdapterFromLuid`),
- create a device and context (`D3DKMTCreateDevice`, `D3DKMTCreateContextVirtual`),
- allocate memory (`D3DKMTCreateAllocation2`) and map GPU VAs
  (`D3DKMTMapGpuVirtualAddress`),
- create fences (`D3DKMTCreateSynchronizationObject2`) and submit
  (`D3DKMTSubmitCommand`), or ring a doorbell,
- and present (`D3DKMTPresent`, or the DXGI swapchain path).

`d3dkmthk.h` in the SDK is the complete list. Every one of those lands in your
KMD as a DDI call. That is the contract that actually matters for this project.

## How the Vulkan loader finds an ICD on a WDDM adapter

Read from Khronos Vulkan-Loader `bde79ad2dd83` for feature 045; it is the record
`src/km/rvkgpu.inf` and `rvkvk.dll` are written against.

- **Discovery is per adapter.** `windows_read_manifest_from_d3d_adapters`
  (`loader_windows.c:499`) enumerates adapters with `D3DKMTEnumAdapters2` and
  reads each one's `VulkanDriverName` from the adapter's software key through
  `D3DKMTQueryAdapterInfo(KMTQAITYPE_QUERYREGISTRY, D3DDDI_QUERYREGISTRY_ADAPTERKEY)`,
  as `REG_MULTI_SZ` then `REG_SZ` (`REG_EXPAND_SZ` is refused). So the INF writes
  `HKR,, VulkanDriverName, 0x00010000, "%13%\rvkvk.json"` in the software
  settings and nothing under `HKLM\SOFTWARE\Khronos\Vulkan\Drivers`, which the
  loader reads additively and would list the device twice from.
- **The manifest's library path.** A relative `library_path` is joined to the
  manifest's directory only when it contains a backslash (`loader.c:2460`):
  `.\\rvkvk.dll`, not `rvkvk.dll`.
- **Interface version 7.** Only `vk_icdGetInstanceProcAddr` must be exported;
  `vk_icdNegotiateLoaderICDInterfaceVersion` is exported too, and
  `vkCreateInstance`, `vkEnumerateInstanceExtensionProperties`,
  `vk_icdGetPhysicalDeviceProcAddr` and `vk_icdEnumerateAdapterPhysicalDevices`
  are answered through `vk_icdGetInstanceProcAddr(NULL, name)` (`loader.c:2112`).
- **Adapters by LUID.** The loader calls `vk_icdEnumerateAdapterPhysicalDevices`
  with each DXGI adapter's LUID (`loader_windows.c:853`); the ICD answers its
  physical device only for its own adapter's LUID, and the same handle
  `vkEnumeratePhysicalDevices` answers, since the loader de-duplicates by handle.
  `VkPhysicalDeviceIDProperties.deviceLUID` is that LUID.
- **Entry points the loader requires.** Through an instance the ICD must answer
  every name `loader_icd_init_entries` looks up with `LOOKUP_REQUIRED_GIPA` -
  `vkDestroyInstance`, `vkEnumeratePhysicalDevices`, the seven 1.0
  `vkGetPhysicalDevice*` queries, `vkGetDeviceProcAddr`, `vkCreateDevice` and
  `vkEnumerateDeviceExtensionProperties` - or the loader skips the driver after
  its `vkCreateInstance` succeeded (found live in 045: a null
  `vkGetDeviceProcAddr` left `vulkaninfo` with no device).
- **Dispatchable objects** are plain structures whose first pointer-sized field
  the ICD sets to `ICD_LOADER_MAGIC` (`0x01CDC0DE`) and the loader overwrites
  with its dispatch table.
- **Load and unload.** The DLL is loaded, freed and loaded again while drivers
  are scanned, and `DllMain` runs under the OS loader lock, so attach and detach
  do nothing; the loader holds its own `loader_lock` around the scan and around
  `vkCreateInstance`, which is what lets an ICD construct its state on the first
  call without racing.

## How opengl32 finds an ICD on a WDDM adapter

Read from the system `opengl32.dll` (10.0.26100.5074, pinned in
`tools/re/binaries.txt`) and `dxgkrnl.sys` for feature 050; it is the record
`src/km/rvkgpu.inf` and `rvkgl.dll` are written against. The adapter's software
key carries `OpenGLDriverName` (`REG_MULTI_SZ`, `%13%\rvkgl.dll`),
`OpenGLVersion` (`REG_DWORD` 2) and `OpenGLFlags` (`REG_DWORD` 1); nothing is
written under `HKLM\...\OpenGLDrivers`.

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

**What the live guest showed** (feature 050, T009 and T043, Windows 11 build
26200). The count above is 15, not 14: `DrvCreateLayerContext` is exported too,
and it is the one opengl32 calls - `wglCreateContext` became
`DrvCreateLayerContext(hdc, 0)` and never `DrvCreateContext`. A window a
session-0 service makes has a DC for which `D3DKMTOpenAdapterFromHdc` fails
(`STATUS_INVALID_PARAMETER`), and opengl32 then takes the first adapter
`D3DKMTEnumAdapters2` lists, which is rvkgpu's in these guests - so the ICD
finds its adapter by enumeration too, and no interactive session is needed.
`wglGetProcAddress` answers only a thread that has a context of the ICD
current: asked with none, it answers null and `ERROR_PROC_NOT_FOUND`, which is
how every extension function (`wglCreateContextAttribsARB` first) is reached.
With stack probes off, a program with no C runtime faults on any frame larger
than the 4 KiB its main thread commits; `src/um/stack_probe.cpp` supplies the
`__chkstk` the compiler calls.

## Presenting from Vulkan

`VK_KHR_win32_surface` + `VK_KHR_swapchain` on Windows are implemented by the
ICD on top of DXGI or on top of `D3DKMTPresent`. For a remoting ICD the
choices are:

1. **Render on host, scan out on host.** Guest swapchain images are host
   images; present is a host-side present. The guest sees nothing. Only works
   for exclusive fullscreen or a separate host window — it does not composite
   into the guest desktop.
2. **Render on host, copy back to a guest allocation, flip in the guest.**
   Guest swapchain image is backed by a shared allocation the host writes; the
   ICD calls `D3DKMTPresent` and your KMD flips it. Costs a copy across the
   shared region, integrates properly with DWM.
3. **Host-side composition, no copy.** Once DWM itself composites on the
   paravirtual adapter (a real UMD), the final desktop image is a host VRAM
   image and the QEMU device hands it to QEMU's display as a dmabuf
   (`dpy_gl_scanout_dmabuf`). Nothing crosses back into guest memory.

Option 2 is the correct first implementation: it works with DWM running on
WARP and with any host driver. Option 3 is the destination, and it depends on
the UMD.

### What feature 046 built, and why it is not option 2 yet

`D3DKMTPresent` is not reachable from an ICD that is not a D3D user-mode
driver. Reversing `dxgkrnl.sys` (26100.1150) shows both doors shut:
`DXGDEVICE::AllowLegacyPresent` (@14036fb0c) refuses a legacy present unless
the device owns the VidPn source, which only the session's display owner does;
and a redirected present - the one DWM composites - is presented from the
user-mode driver's redirection surface, which a process without a UMD never
creates. A window's present therefore has no kernel path from the ICD alone.

So 046's swapchain presents like this: the swapchain's images are host images,
`vkQueuePresentKHR` copies the acquired one through a host-visible buffer into
guest memory (the chunks of feature 045), turns it bottom-up into the window's
channel order, and puts it on the window with `SetDIBitsToDevice` on the
window's own DC. FIFO then waits for the adapter's next vertical blank through
`D3DKMTWaitForVerticalBlankEvent`. It composites correctly - the desktop owns
the window, so the frame is where the application put it - at the cost of one
copy back and a GDI blit. FR-001 ("never GDI") is deferred to feature 055 with
the UMD, which is what makes option 2 reachable.

The desktop's own presents are a separate matter: dxgkrnl writes them as DMA
buffers for the miniport's node, and this driver's `ExecutionNode` completes
anything that is not an rvk context as a loopback, so nothing ever executed the
blits. `PresentBlit` (`src/core/present/`) now moves those pixels in the
miniport, between allocations whose guest pages the aperture mapping recorded,
and `adapter/0 present_rows` in a state dump says how many rows it moved.

### What the desktop actually presents from, measured (feature 046, T012)

`PresentBlit` moves what a present names. The live guest says a present names
nothing. Measured on the 046 image (driver 0.35, Windows 11 26100, session 1
active with `dwm.exe`, `explorer.exe` and `winlogon.exe` running):

- In a whole boot `DxgkDdiPresent` is called **twice**, both with
  `Flags = Blt` and `NumSrcAllocations = NumDstAllocations = 0` - no allocation
  list at all, so there is nothing to move. The driver says so once per present:
  `rvkgpu: present unmoved list flags 0x00000001 sources 0 destinations 0`.
- `DxgkDdiSetVidPnSourceAddress` only ever names `0xC0000000`, the shared
  primary (`kind=1`); the adapter holds exactly two allocations, that primary
  and a shadow (`kind=2`) of the same 1280x800x4 bytes, both CPU-visible.
- QMP `pmemsave` of the primary's 4096000 bytes is **all zeros** - at the
  desktop, and while a full-screen red window ran on the console desktop
  (a scheduled task with `/it`, so session 1, not the runner's session 0).
  The screendump is black in both cases and no further present arrives.
- The display class key has no `UserModeDriverName`: there is no D3D UMD, so
  DWM composes with WARP and its output never becomes bytes in an allocation
  this miniport has pages for. The 35 DMA buffers the two non-rvk contexts
  submit are loopback-completed by `ExecutionNode` as before.

So the clarification this feature planned against - "every source DWM presents
from on this adapter is CPU-backed" - is wrong: on a miniport with no UMD, DWM
does not present through `DxgkDdiPresent` at all. The desktop reaching the
scanout waits for guest GPU work to be executed on the host (user-mode
submission, feature 049, and the D3D UMD of features 055-057), not for anything
else in the present path. `PresentBlit` stays, proven at L1 and L2 and ready
for the presents that will name allocations; `adapter/0 present_rows` and
`presents_unmoved` in a state dump are how a later feature will see it start to
carry frames.
