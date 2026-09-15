# Research: Vulkan - the ICD on rvkgpu

Every decision below was checked against the tree on 2026-09-15 (branch
`045-vulkan-icd`, from `9520111`), against the Khronos Vulkan-Loader at commit
`bde79ad2dd83` and against `dxgmms2.sys` in `re/ghidra-proj`. Where the spec
asks for something this feature cannot prove, the scope change is recorded
here with its reason and where the work lands.

## 1. How the loader finds the ICD

**Decision.** The INF writes, in the display adapter's software key
(`[Rvkgpu_SoftwareSettings]`, already `AddReg`ed from `[Rvkgpu_Install]`):

```inf
HKR,, VulkanDriverName, 0x00010000, "%13%\rvkvk.json"
```

`rvkvk.json` sits beside `rvkvk.dll` in the driver store:

```json
{"file_format_version": "1.0.1",
 "ICD": {"library_path": ".\\rvkvk.dll", "api_version": "1.3.0", "library_arch": "64"}}
```

The DLL exports `vk_icdNegotiateLoaderICDInterfaceVersion` and
`vk_icdGetInstanceProcAddr`, negotiates interface version 7, and answers
`vk_icdGetPhysicalDeviceProcAddr` and `vk_icdEnumerateAdapterPhysicalDevices`
through `vk_icdGetInstanceProcAddr(NULL, ...)`.

**Rationale (loader source).**
- `loader_windows.c:499` `windows_read_manifest_from_d3d_adapters` enumerates
  with `D3DKMTEnumAdapters2` and reads each adapter's `VulkanDriverName`
  through `D3DKMTQueryAdapterInfo(KMTQAITYPE_QUERYREGISTRY = 48,
  D3DDDI_QUERYREGISTRY_ADAPTERKEY)`, trying `REG_MULTI_SZ` then `REG_SZ`
  (`REG_EXPAND_SZ` is rejected). The value is an absolute manifest path;
  `%13%` expands to the driver store directory at install.
- `loader.c:2460`: a relative `library_path` is joined to the manifest's
  directory only when it contains a backslash, so `.\\rvkvk.dll`, not
  `rvkvk.dll`.
- `loader.c:2112`: with interface version 7 only `vk_icdGetInstanceProcAddr` must
  be exported; `vkCreateInstance` and `vkEnumerateInstanceExtensionProperties`
  are required through it; `vk_icdEnumerateAdapterPhysicalDevices` is looked up
  through it for version 6 and up.
- `loader_windows.c:853`: the loader calls every ICD's
  `vk_icdEnumerateAdapterPhysicalDevices` with each DXGI adapter's LUID; the ICD
  answers 0 for an adapter that is not its own, and must answer the same
  `VkPhysicalDevice` handles `vkEnumeratePhysicalDevices` answers (the loader
  de-duplicates by handle).
- Dispatchable objects are plain structures whose first pointer-sized field the
  loader overwrites; the ICD writes `ICD_LOADER_MAGIC` (`0x01CDC0DE`) there on
  instances, physical devices, devices, queues and command buffers.
- The DLL is loaded, freed and loaded again (`loader_preload_icds`); detach runs
  under the OS loader lock, so it must not block.

**Alternatives rejected.** `HKLM\SOFTWARE\Khronos\Vulkan\Drivers` (FR-001
forbids it, and the loader reads it additively, so it would list the device
twice); `VK_DRIVER_FILES` (per-process).

## 2. Where the ICD's code lives, and how it builds

**Decision.**
- `src/icd/vulkan/` - the ICD's decisions, portable C++23 under the same rules as
  `src/guest`: dispatchable objects, the connection, command-buffer batches,
  memory types, the extension policy, waits. It compiles into `av_icd`, not `av_core` - its strong
  `handle_to_wire` hooks would collide with the executor's - and is
  tested on Linux over `MockKmt` and, end to end, over the full-stack rig with
  the real executor.
- `src/um/vulkan/` - the Windows shim, translation only: the two DLL exports
  (one line each into the ICD root, the ABI boundary CLAUDE.md permits), the
  Windows implementations of the ICD's two seams (below), and the `.def`.
- `rvkvk.dll` builds in the `rvktest` ExternalProject with the same
  freestanding clang-cl flags, a DLL link (`/DLL /ENTRY:rvkvk_entry
  /NODEFAULTLIB`), the generated `vk_icd_entries.cpp`, `vk_dispatch_client.cpp`
  and `vk_serialize` users, and imports only gdi32, kernel32 and ntdll.
  `tools/check_guest_image.py` learns a DLL mode (subsystem `WINDOWS_GUI` is not
  required; exports must be exactly the two entry points).

**Rationale.** 044 made the generated client freestanding for exactly this
(044 research §1). Keeping decisions portable is constitution II; the full-stack
rig (044 T026) runs the guest's whole path on Linux, so the ICD's logic is proven
in seconds before a guest boot, as FR-006 asks.

**Seams (two real implementations each, constitution VIII).**
- `Kmt` (exists) - `GdiKmt` in the guest, `MockKmt`, `DdiKmt` on Linux. It gains
  the calls the ICD needs: the adapter's LUID from the enumeration,
  `query_video_memory` (`D3DKMTQueryVideoMemoryInfo`).
- `IcdMemory` (new) - where the ICD's objects live. `ProcessHeap` in
  `src/um/vulkan` over `HeapAlloc`/`HeapFree` (kernel32), and the tests'
  `MockIcdMemory` that fails any allocation a test names. Constitution
  "Allocation": memory comes from an injected allocator with the caller owning
  release; dispatchable objects, whose count has no bound, live in
  `ObjectPool<T>` over it.

## 3. One connection per instance

**Decision.** `vkCreateInstance` opens a `Connection`: the rvkgpu adapter found
by PCI identity (`AdapterSearch`), a `GuestConnection` on it and one
`KmtTransport` of `SubApi::vulkan` - one rvk context on the host. Every object
under that instance, on every device, goes through it. The connection holds a
lock (`SRWLOCK` in the shim behind a `Lock` seam, the tests' fake recording
order), because applications call from several threads.

**Rationale.** The host executor's handle registry is per session (042, T031):
a device made on one rvk context cannot be named on another, so an instance's
objects must share one. The generated wrappers already call
`acquire_default_connection()` in `vkCreateInstance` and `connection_of(h)`
elsewhere (063 contracts §F).

**Alternatives rejected.** A context per `VkDevice` (host handles of the
instance and physical device would be unknown to it); a process-wide connection
(two instances would share one host instance's lifetime).

## 4. Commands larger than 1 KiB

**Decision.** The ICD's transport uses ring order 20 (1 MiB) and a per-command
limit of 256 KiB, staged in memory from `IcdMemory` rather than a member array;
the reply slot grows to 64 KiB. `KmtTransport` takes both sizes at construction.
`vkCreateShaderModule` and pipeline caches up to that limit cross whole; larger
ones answer `VK_ERROR_OUT_OF_HOST_MEMORY` until 050's blob arena (043 US2) carries
bulk data.

**Rationale.** `KmtTransport::max_command` is 1024 and `reply_bytes` 4096
(`kmt_transport.hpp:35-36`); a compute shader alone is a few KiB of SPIR-V.
Ring orders 12-24 are already accepted end to end (`ring_header.hpp:17-18`,
`capset_info.hpp:56-57`).

## 5. Command buffers record into their own batch

**Decision.** `record_target(VkCommandBuffer)` answers the command buffer's
`CommandBatch`: a `ClientTransport` that appends framed commands into memory
the command buffer owns (`IcdMemory` chunks). `vkEndCommandBuffer` ships the
batch through the connection's transport in one publish; `vkBeginCommandBuffer`
drops a previous one; `vkResetCommandBuffer` goes straight out.

**Rationale.** The emitter's design (`icd_entries.py:42-61`): recorded commands
are externally synchronised per command buffer, so batching needs no lock and
cannot reorder anything, and a frame's hundreds of `vkCmd*` cost one submission.

## 6. Waits stay under TdrDelay

**Decision.** `vkWaitForFences`, `vkWaitSemaphores` and `vkDeviceWaitIdle` /
`vkQueueWaitIdle` are sent in slices of at most 500 ms of host wait each; the
ICD loops against its own performance-counter deadline and answers `VK_TIMEOUT`
when that passes. Status queries (`vkGetFenceStatus`, timeline counters) are
single round trips.

**Rationale.** A host call that blocks its context's worker keeps the batch
from completing, and dxgkrnl resets the adapter after TdrDelay (2 s) - the
failure 044 T038 chased. Sliced waits keep every batch well inside it. The
spec's "without polling" is met in the sense that matters to the guest (the
thread sleeps in `D3DKMT` waits, never spins); a wait with no host round trip
at all is a native fence, feature 049.

## 7. Memory: types from the adapter, host-visible memory with no copy

**Decision (design A).**
- `vkGetPhysicalDeviceMemoryProperties` answers the host's types, filtered:
  a type keeps `HOST_VISIBLE` only where the host GPU can import a host pointer
  into it (Intel: every type; NVIDIA: the host-RAM types, not the BAR type), and
  heap sizes are the adapter's segment sizes from `D3DKMTQueryVideoMemoryInfo`.
- Device-local allocations forward `vkAllocateMemory` as today.
- Host-visible allocations are carved from **chunks**: a chunk is an rvkgpu
  allocation of a new kind, `host_visible_memory`, pinned in the aperture segment
  and locked for the process (the pattern of the transport's ring,
  `kmt_transport.cpp:29-60`). The KMD publishes the chunk's guest pages as a GUEST
  blob (`guest_blob_pages` exists) when the ICD binds it to its context through a
  private-data record. A transport command `IMPORT_BLOB` (API 0, beside NOP) makes
  the device build a contiguous host view of the blob's pages and hand it to the
  executor's `import_blob` as a host pointer; the executor imports it with
  `VK_EXT_external_memory_host` into the guest device's `VkDeviceMemory` of the
  type the guest asked for, registers the handle and answers it.
- `vkMapMemory` answers the chunk's lock view plus the offset; `vkUnmapMemory`
  does nothing. The host GPU and the guest process then read and write the same
  guest RAM.

**Rationale.**
- 041 found VidMm refuses every allocation preferring the CPU-visible host-memory
  segment. `dxgmms2.sys` `VIDMM_GLOBAL::CreateOneAllocation` @1400f9574 refuses a
  `CpuVisible` allocation with no aperture-type segment in its supported set on a
  WDDM 3.0+ driver ("CPUVisible allocations must include an aperture segment in
  the supported segment set"); and even accepted, that region is QEMU RAM, not
  GPU memory, so it would still not be zero-copy.
- The host side already exists in parts: the runner starts QEMU with a shared
  memfd RAM backend (`run_guest_suite.py:209,218`); 039 imported guest RAM into
  both host GPUs with `VK_EXT_external_memory_host`; ABI v2 has `import_blob` with
  `RVK_BLOB_HOST_POINTER` and `VulkanBackend::adopt` (042), which nothing calls yet.
- The contiguous host view: for each run of contiguous guest-physical pages, the
  device maps the same memfd pages again at the right place in a reserved range
  (`MAP_SHARED | MAP_FIXED`), from `qemu_ram_block_from_host` and the region's fd.
  That is a second mapping of the pages 039 imported, which is inference until the
  first live experiment (below).

**First experiment, before the rest is built (Phase 2 of tasks):** a 64 MiB
chunk in a real guest process, deliberately scattered, imported on each host GPU;
the host GPU writes a pattern the guest view reads, and the reverse.

**Alternatives rejected.** Retesting 041's segment preference with an aperture
segment in the supported set (cheap, recorded as a follow-up, but not zero-copy
for the reason above); a copy on map and unmap (not what US2.1 asks, and every
`vkMapMemory` becomes a round trip); udmabuf (`VM_PFNMAP` mappings cannot be
imported as host pointers).

**Risks carried into tasks.** VidMm trimming a pinned aperture allocation while
the host holds the import (043's open risk: log every aperture unmap of the
kind); optimal-tiling images on imported memory (query
`vkGetPhysicalDeviceImageFormatProperties2` with `HOST_ALLOCATION` before
answering `memoryTypeBits`); limits (`GuestPageMapping` 16384 entries,
`ImportTable` 256 per context, `BlobTable` 1024).

## 8. Properties, identity and version

**Decision.** Physical-device queries are forwarded to the host, which shows the
context only its bound GPU (044 T034). The ICD rewrites what the guest's view
owns: `VkPhysicalDeviceIDProperties.deviceLUID` becomes the adapter's LUID with
`deviceLUIDValid` true, `apiVersion` is `min(1.3, host)`, and the heaps come from
section 7.

**Rationale.** The capset carries the GPU's UUID, not its name or version, and
the host's answers are the host GPU's own - which is what US1.2 asks to see. The
LUID is the guest's alone.

## 9. What is reported

**Decision.** Device extensions are the intersection of three lists: what the
host device reports, the generated allowlist (`vk_supported_extensions.h`), and
an `ExtensionPolicy` table this feature owns in which each entry names the test
that exercises it. 045's table holds the extensions its scenarios use -
`VK_EXT_memory_budget` (answered locally from `D3DKMTQueryVideoMemoryInfo`) and
`VK_EXT_external_memory_host` is **not** reported to the guest (it is the ICD's
own mechanism). Instance extensions: none beyond the loader's. WSI
(`VK_KHR_surface`, `VK_KHR_win32_surface`, `VK_KHR_swapchain`) is **not** reported
until 046 implements presentation; 045 US3.2 is satisfied as "never forwarded".

**Rationale.** Constitution VI: never report a capability that is not implemented
and tested. The DXVK / vkd3d-proton set is 047's.

## 10. The guest proof needs a loader

**Decision.** `tools/fetch_vulkan_runtime.py` fetches the Khronos Vulkan runtime
(`vulkan-1.dll`, `vulkaninfo.exe`) pinned by SHA-256, the way the Windows kit and
the Khronos registries are fetched; the runner pushes both beside `rvkvkprobe.exe`,
a freestanding probe that imports `vulkan-1.dll` through an import library made
from a `.def`, and runs `vulkaninfo --summary`. Shaders are GLSL committed beside
their SPIR-V as byte arrays generated by `tools/gen_spirv.py` (`glslangValidator`),
with a `--check` ctest like `check_generated`.

**Rationale.** Windows installs no Vulkan loader without a vendor driver; SC-001
names `vulkaninfo`. Fetching by pin keeps the `self_contained` gate's rule
(nothing referenced outside the repository at build time without a pinned fetch).

## 11. Scope moved

- **The 32-bit ICD (`VulkanDriverNameWow`)** moves to 061 (the driver package, whose
  spec already names every `*DriverNameWow`): it needs an x86 toolchain, decorated
  `__stdcall` exports and host-visible chunks below 4 GiB, which VidMm places
  (041 T023). `specs/061-driver-package/spec.md` carries it.
- **Graphics workload readback without presentation**: the probe renders into an
  offscreen image and reads it back through host-visible memory; presentation is
  046.
- **Bulk data above 256 KiB** (large pipeline caches, big shader modules): 050's
  blob arena.
