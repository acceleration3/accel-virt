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
- **Dispatchable objects** are plain structures whose first pointer-sized field
  the ICD sets to `ICD_LOADER_MAGIC` (`0x01CDC0DE`) and the loader overwrites
  with its dispatch table.
- **Load and unload.** The DLL is loaded, freed and loaded again while drivers
  are scanned, and `DllMain` runs under the OS loader lock, so attach and detach
  do nothing; the loader holds its own `loader_lock` around the scan and around
  `vkCreateInstance`, which is what lets an ICD construct its state on the first
  call without racing.

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
