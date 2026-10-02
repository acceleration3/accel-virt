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
`src/km/avgpu.inf` and `avvk.dll` are written against.

- **Discovery is per adapter.** `windows_read_manifest_from_d3d_adapters`
  (`loader_windows.c:499`) enumerates adapters with `D3DKMTEnumAdapters2` and
  reads each one's `VulkanDriverName` from the adapter's software key through
  `D3DKMTQueryAdapterInfo(KMTQAITYPE_QUERYREGISTRY, D3DDDI_QUERYREGISTRY_ADAPTERKEY)`,
  as `REG_MULTI_SZ` then `REG_SZ` (`REG_EXPAND_SZ` is refused). So the INF writes
  `HKR,, VulkanDriverName, 0x00010000, "%13%\avvk.json"` in the software
  settings and nothing under `HKLM\SOFTWARE\Khronos\Vulkan\Drivers`, which the
  loader reads additively and would list the device twice from.
- **The manifest's library path.** A relative `library_path` is joined to the
  manifest's directory only when it contains a backslash (`loader.c:2460`):
  `.\\avvk.dll`, not `avvk.dll`.
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
`src/km/avgpu.inf` and `avgl.dll` are written against. The adapter's software
key carries `OpenGLDriverName` (`REG_MULTI_SZ`, `%13%\avgl.dll`),
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
`D3DKMTEnumAdapters2` lists, which is avgpu's in these guests - so the ICD
finds its adapter by enumeration too, and no interactive session is needed.
`wglGetProcAddress` answers only a thread that has a context of the ICD
current: asked with none, it answers null and `ERROR_PROC_NOT_FOUND`, which is
how every extension function (`wglCreateContextAttribsARB` first) is reached.
With stack probes off, a program with no C runtime faults on any frame larger
than the 4 KiB its main thread commits; `src/um/stack_probe.cpp` supplies the
`__chkstk` the compiler calls.

### The compatibility profile, and what a swap costs (feature 051)

A context an application makes with `wglCreateContext` and no attributes is
what every OpenGL program written before profiles existed makes, and what it
wants is the fixed-function pipeline. The ICD asks the host for a 4.6
compatibility context, then for the driver's own default, then for 4.6 core,
and reports whichever it got - read from `GL_CONTEXT_PROFILE_MASK`, never from
`GL_VERSION`, because one of this project's two host GPUs names its
compatibility context exactly as it names its core one. Both host GPUs here
grant 4.6 compatibility over EGL.

The proc table's 274 compatibility slots hold their real forwarders, and each
forwarder asks the granted profile before it sends: on a core context it
records the `GL_INVALID_OPERATION` a core context would. The thirteen
fixed-function **client array** entry points stay refused - they read the
application's own memory at every draw, which this transport does not carry,
and it is the same refusal a modern attribute array with no buffer bound gets.

`SwapBuffers` still reads the back buffer and blits it (there is no D3DKMT
present from an ICD - see above), but what surrounds it is a driver's:
`wglSwapIntervalEXT` holds the swap for that many of the adapter's vertical
blanks through the `FramePacer` both ICDs share; a swap on a window that is
gone, minimized or empty reads nothing back at all; a single-buffered pixel
format presents on `glFlush`, as it does on Windows; and a read of `GL_FRONT`
is answered from the frame the swap already put on the window, with no round
trip.

## How the Khronos loader finds an ICD on a WDDM adapter

Read from the Khronos OpenCL ICD loader source pinned by
`tools/opencl_loader_pins.txt` (tag `v2024.10.24`, fetched into
`third_party/opencl-loader/` by `tools/fetch_opencl_loader.py`) for feature
052; it is the record `src/km/avgpu.inf` and `avcl.dll` are written against.
Line numbers are that source's. Unlike Vulkan and OpenGL there is no loader to
pin as a binary - Khronos publishes none - so the guest's `OpenCL.dll` is this
source cross-compiled with the toolchain that builds `avgpu.sys`
(research.md §2).

**Discovery runs once per process** - `khrIcdOsVendorsEnumerateOnce` wraps it
in `InitOnceExecuteOnce` (`icd_windows.c:409`) - and
`khrIcdOsVendorsEnumerate` (`:186`) walks `OCL_ICD_FILENAMES` from the
environment first (`icd.c:369`), then three discovery paths in order, then the
global vendors key. Each path's failure is traced and the next runs anyway.

1. **`khrIcdOsVendorsEnumerateDXGK`** (`icd_windows_dxgk.c:32`). `gdi32.dll` is
   loaded and `D3DKMTSubmitPresentBltToHwQueue`'s presence is the OS check
   (`:42`); `D3DKMTEnumAdapters2` lists every adapter (`:49`) and each is asked
   `D3DKMTQueryAdapterInfo` (`:55`) with a registry query -
   `LOADER_QUERY_REGISTRY_ADAPTER_KEY`, `translate_path = TRUE`, **`value_type
   = REG_SZ`**, `value_name` from `getOpenCLRegKeyName` (`:97-99`). The value
   read is handed to `khrIcdVendorAdd` with the adapter's LUID.
2. **`khrIcdOsVendorsEnumerateHKR`** (`icd_windows_hkr.c:206`): SetupAPI over
   the display device class and then the software-component class (`:225`,
   `:356`), `CM_Open_DevNode_Key(..., CM_REGISTRY_SOFTWARE)` (`:83`), and
   `RegQueryValueExA` of the same value name.
3. **`khrIcdOsVendorsEnumerateAppPackage`** (`icd_windows_apppackage.c:36`),
   for an ICD shipped inside an MSIX package, and then
   `HKLM\SOFTWARE\Khronos\OpenCL\Vendors` (`icd_windows.c:194`). This driver
   uses neither.

**Why the value is `REG_SZ`.** The INF writes `HKR,, OpenCLDriverName,
0x00000000, "%13%\avcl.dll"` - a plain string, not the `REG_MULTI_SZ` that
`VulkanDriverName` and `OpenGLDriverName` beside it use. Path 1 asks for
`REG_SZ` and only retries the query as `REG_MULTI_SZ` when the call itself
returns a failure `NTSTATUS` (`icd_windows_dxgk.c:118-121`, "Accepting
multi-string registry key type"); path 2 accepts `REG_SZ` and, after the read,
tolerates `REG_MULTI_SZ` (`icd_windows_hkr.c:132`). `REG_SZ` is the one type
both take on their first attempt, and it is what makes the two paths agree
about an adapter they both see. `translate_path` is the flag that has the
`%13%` driver-store path resolved.

The value name is chosen at runtime by `getOpenCLRegKeyName`
(`icd_windows_hkr.c:55`): a 64-bit loader always reads `OpenCLDriverName`,
and a 32-bit one reads `OpenCLDriverNameWow` when `IsWow64Process` says so.
Feature 061 writes that second value, as it does for the other two ICDs.

**One export.** `khrIcdVendorAdd` (`icd.c:70`) resolves exactly one symbol by
name from the DLL: **`clGetExtensionFunctionAddress`** (`:92`). Nothing else is
looked up as an export - `clIcdGetPlatformIDsKHR` is obtained *through* it
(`:100`) and the ICD is refused if that answers null. So `avcl.dll` exports
that function and `clIcdGetPlatformIDsKHR`, which older loaders looked up
directly and which costs a line, and nothing else: the rest of the API is
reached through the dispatch table, and an export the loader never looks up
would only invite direct linking that bypasses it.

**What the loader does with the platforms.** `clIcdGetPlatformIDs(0, NULL,
&count)` then the fill call (`:108`, `:121`); a null `cl_platform_id` in the
array is skipped (`:136`), but a **non-null platform's `dispatch` is
dereferenced without a check** (`:140`), so every object this ICD hands out is
a struct whose first member is a `cl_icd_dispatch *` the ICD has already
filled in - a null there is a crash inside the loader, not a rejection.

**The two queries an ICD cannot decline** are
`clGetPlatformInfo(CL_PLATFORM_ICD_SUFFIX_KHR, ...)`, asked twice: once for the
size (`:140`) and once for the bytes (`:155`). Either one answering anything
but `CL_SUCCESS` drops that platform silently, and a platform with no suffix is
a platform the loader will not name. Both are answered from the platform alone,
before any device exists and without opening an adapter, because the loader
asks them while enumerating - an ICD that opened a device here would open one
in every process that ever calls `clGetPlatformIDs`. `avcl.dll` answers `AV`.

**Load and unload.** The DLL is loaded for the probe, loaded a second time per
platform accepted (`:178`), and the probe's own reference is freed at the end
of `khrIcdVendorAdd` whatever happened - so an ICD that offers no platform is
loaded and freed with nothing kept, and `DllMain` runs under the OS loader
lock. This ICD's entry point therefore returns `TRUE` and does nothing
(research.md §3): the first real call is where its state is built.

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
anything that is not an av context as a loopback, so nothing ever executed the
blits. `PresentBlit` (`src/miniport/present/`) now moves those pixels in the
miniport, between allocations whose guest pages the aperture mapping recorded,
and `adapter/0 present_rows` in a state dump says how many rows it moved.

### What the desktop actually presents from, measured (feature 046, T012)

`PresentBlit` moves what a present names. The live guest says a present names
nothing. Measured on the 046 image (driver 0.35, Windows 11 26100, session 1
active with `dwm.exe`, `explorer.exe` and `winlogon.exe` running):

- In a whole boot `DxgkDdiPresent` is called **twice**, both with
  `Flags = Blt` and `NumSrcAllocations = NumDstAllocations = 0` - no allocation
  list at all, so there is nothing to move. The driver says so once per present:
  `avgpu: present unmoved list flags 0x00000001 sources 0 destinations 0`.
- `DxgkDdiSetVidPnSourceAddress` only ever names `0xC0000000`, the shared
  primary (`kind=1`); the adapter holds exactly two allocations, that primary
  and a shadow (`kind=2`) of the same 1280x800x4 bytes, both CPU-visible.
- QMP `pmemsave` of the primary's 4096000 bytes is **all zeros** - at the
  desktop, and while a full-screen red window ran on the console desktop
  (a scheduled task with `/it`, so session 1, not the runner's session 0).
  The screendump is black in both cases and no further present arrives.
- The display class key has no `UserModeDriverName`: there is no D3D UMD, so
  DWM composes with WARP and its output never becomes bytes in an allocation
  this miniport has pages for. The 35 DMA buffers the two non-av contexts
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

## What the two layers require of the ICD (feature 047)

Both layers enumerate the driver's extensions and features and refuse the
device that is missing one, so what they require is a gate here rather than a
discovery in a game: `tools/layer_requirements.py` reads each layer's own
required list out of its pinned source, and `tools/check_layer_requirements.py`
fails the build naming any name the ICD's policy table does not report. The
lists are never copied into this repository - a release that starts requiring
one more extension fails the gate instead of failing at runtime.

Three things about that gate are worth knowing:

- **What the loader answers is not the driver's to report.** Both layers
  require `VK_KHR_surface` and `VK_EXT_debug_utils`, which are the loader's;
  `NOT_A_DRIVER_S` in the gate lists them, and an absent row for one of them
  is not a gap.
- **A row may be answered over a different host extension.** No Linux driver
  has a Win32 handle to give, so `VK_KHR_external_memory_win32` is reported
  over the host's `VK_KHR_external_memory_fd`, and the policy row names that
  mechanism. The gate holds the named host extension to the generated list
  too, so a row cannot promise a translation this build never generated.
- **A feature is answered from the host unchanged.** Dynamic rendering,
  synchronization2, timeline semaphores, buffer device address, descriptor
  indexing, mutable descriptor types and the rest arrive as
  `VkPhysicalDeviceFeatures2` chain nodes and are carried whole. An extension
  the host lacks is one the ICD reports absent; nothing is answered
  optimistically.

### DXGI interop: what a Win32 handle is here

A `HANDLE` is not a thing the wire carries, and it is not a type the host
build's headers declare, so all four commands of the two Win32
external-object extensions are the ICD's own
(`src/icd/vulkan/external_objects.cpp`), over records that write out the
Windows structure layouts. A handle the ICD answers names a row of its own
table: the memory or the semaphore of one device.

The sharing behind it is the host's. A guest device is a host device, so two
guest devices sharing an image share it between two host devices - which the
host's own fd extensions do, because both host devices live in the one process
the device library runs in. So an export remembers the object and answers a
handle, and an import asks the host for a **fresh** `vkGetMemoryFdKHR` of that
object and forwards `VkImportMemoryFdInfoKHR` in the Win32 structure's place.
Fresh per import, because a host driver takes ownership of an fd it imports.

The device side owns those fds (`HostExportedFds`): an fd an import names must
be one that context exported, since an fd a guest made up names something else
in the QEMU process entirely, and an fd exported and never imported is closed
with its device rather than held until QEMU exits.

Two consequences to keep in mind:

- **Sharing is within one instance.** The fd table is a context's, and each
  guest instance is its own context. DXVK and vkd3d-proton each make one
  instance, so this is not a limit either of them meets; two processes sharing
  is feature 053's, which gives objects a device-global identity.
- **The KMT and Direct3D handle types are absent, and say so.**
  `vkGetPhysicalDeviceExternalBufferProperties` answers the one opaque Win32
  type out of the host's opaque fd and answers every other Win32 type as
  absent, because nothing here turns a Direct3D object into a host one.
  `vkGetMemoryWin32HandlePropertiesKHR` refuses every handle: it answers for a
  handle from outside, and a handle this ICD did not export names nothing.

### The 32-bit ICD is feature 061's

DXVK and vkd3d-proton both ship 32-bit builds, and a 32-bit title loads a
32-bit `vulkan-1.dll` which loads a 32-bit ICD. There is none here: the
driver's user-mode side is built x64 only, and a second architecture is a
second build of the guest tree, its own D3DKMT thunk width and its own
registry key. Feature 061 is where that belongs, and until then a 32-bit title
finds no driver rather than a broken one.

## The Khronos loader dispatches an unknown entry point on its first argument

A driver-private Vulkan entry point - one the registry has never heard of, that
an ICD answers from `vk_icdGetInstanceProcAddr` - does not reach the ICD the
way an application calls it. The loader has no dispatch table row for a name it
does not know, so it hands out a **generic trampoline**, and every one of those
trampolines does the same thing: it reads the call's **first argument**, treats
it as a dispatchable handle, follows the loader dispatch table at the front of
it, and jumps to the driver's function through that.

So a driver-private entry point must take a dispatchable Vulkan handle as its
first parameter - `VkInstance`, `VkPhysicalDevice`, `VkDevice`, `VkQueue` or
`VkCommandBuffer`. Anything else and the trampoline dereferences whatever was
in that register.

Feature 053 learned this from a guest, twice:

- `vkAvLastKernelStatus()` took no arguments at all. The probe crashed with
  `0xC0000005` inside `vulkan-1.dll+0x80dda` on the first call - the trampoline
  reading a dispatch table out of an uninitialised register.
- Respelled `vkAvLastKernelStatus(VkPhysicalDevice, uint32_t *)` it works, and
  has been read back from a live guest since.

`vkAvForceContextFault(VkInstance, VkPhysicalDevice)` from feature 044 had the
shape right from the start, which is why nothing had caught the rule: both of
the project's private entry points now begin with a dispatchable handle because
the loader requires it, not because they need one.

Two things follow for anyone adding another:

- **The handle is the dispatch, so it must be one the ICD made.** A private
  entry point cannot be called before an instance exists, and cannot be called
  with a handle from another ICD.
- **A unit test cannot catch this.** The tests call the ICD's function pointer
  directly and never go through a loader, so the wrong shape passes every test
  in the tree and fails on the first real guest. The rule is the review, not the
  suite.
