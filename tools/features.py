"""The feature decomposition: WDDM levels as implementation phases, plus the
QEMU device and the foundations. Consumed by gen_roadmap.py.

status:
  implement  build it, test it, report the capability
  stub       provide the DDI, return the correct "not supported" status, test that
  skip       leave the slot NULL; dxgkrnl handles absence
  partial    implement the listed subset only
"""

# (id, short-name, level-key-or-None, status, title, summary, extra)
FEATURES = [
 # ---- foundations ---------------------------------------------------------
 ("001","build-and-test-foundation",None,"implement",
  "Build and test foundation",
  "CMake/Ninja build producing three targets from one source tree: the native "
  "test binary, the Windows driver, and the QEMU device object. ctest as the "
  "single entry point, with ASan/UBSan and llvm-cov wired in and gating.",
  {"deps":[],"kb":["12-re-cookbook.md"]}),

 ("002","platform-abstraction-and-mock",None,"implement",
  "Platform abstraction and adversarial mock",
  "src/core/platform.h and its two implementations: the Windows kernel shim and "
  "the test mock. The mock simulates IRQL, fails allocations on demand, tracks "
  "leaks and lock order, and controls time.",
  {"deps":["001"],"kb":["02-kmd-contract.md"]}),

 ("003","ddi-conformance-harness",None,"implement",
  "DDI conformance harness (fake dxgkrnl)",
  "A fake dxgkrnl that builds DRIVER_INITIALIZATION_DATA, calls DriverEntry, and "
  "drives the DDI table through sequences derived from reverse engineering the "
  "real dxgkrnl.sys, including the abusive-but-legal sequences a naive driver "
  "forgets.",
  {"deps":["002"],"kb":["02-kmd-contract.md","10-warp-kmd.md"]}),

 ("004","wire-protocol-core",None,"implement",
  "Wire protocol: shared definitions and codecs",
  "src/proto: the structures shared by driver and device — submission "
  "descriptors, fence pages, ring headers, capset payload, version negotiation. "
  "One header, two consumers, no divergence.",
  {"deps":["001"],"kb":["11-implementation-plan.md"]}),

 ("005","virtio-pci-transport",None,"implement",
  "virtio-pci transport inside the miniport",
  "Capability walk, feature negotiation, device status FSM, split virtqueues and "
  "MSI-X, written from the virtio 1.2 spec because no virtio library exists "
  "under a WDDM miniport.",
  {"deps":["002","004"],"kb":["11-implementation-plan.md"]}),

 # ---- WDDM 1.0 (BASE), split by functional area ---------------------------
 ("006","wddm10-pnp-and-adapter","BASE","implement",
  "WDDM 1.0: PnP, power and adapter lifetime",
  "The adapter comes up and goes down cleanly: AddDevice through RemoveDevice, "
  "power transitions, child enumeration, and the interrupt/DPC pair.",
  {"deps":["003","005"],"slots":[
   "Version","DxgkDdiAddDevice","DxgkDdiStartDevice","DxgkDdiStopDevice",
   "DxgkDdiRemoveDevice","DxgkDdiDispatchIoRequest","DxgkDdiInterruptRoutine",
   "DxgkDdiDpcRoutine","DxgkDdiQueryChildRelations","DxgkDdiQueryChildStatus",
   "DxgkDdiQueryDeviceDescriptor","DxgkDdiSetPowerState","DxgkDdiNotifyAcpiEvent",
   "DxgkDdiResetDevice","DxgkDdiUnload","DxgkDdiQueryInterface",
   "DxgkDdiControlEtwLogging","DxgkDdiLinkDevice"],
   "kb":["02-kmd-contract.md","06-display.md"]}),

 ("007","wddm10-capability-reporting","BASE","implement",
  "WDDM 1.0: capability reporting",
  "DxgkDdiQueryAdapterInfo, the 48-way switch that decides what kind of driver "
  "this is. Wrong answers here fail silently much later, so this feature owns "
  "the rule that a capability is never reported before it is implemented.",
  {"deps":["006"],"slots":["DxgkDdiQueryAdapterInfo","DxgkDdiEscape",
   "DxgkDdiCollectDbgInfo"],
   "kb":["02-kmd-contract.md","04-memory.md"]}),

 ("008","wddm10-device-and-allocation","BASE","implement",
  "WDDM 1.0: devices, allocations and segments",
  "The memory object model: segments reported to VidMm, allocations created, "
  "described, opened and closed, and the standard allocations the runtime makes "
  "without the UMD.",
  {"deps":["007"],"slots":["DxgkDdiCreateDevice","DxgkDdiDestroyDevice",
   "DxgkDdiCreateAllocation","DxgkDdiDestroyAllocation","DxgkDdiDescribeAllocation",
   "DxgkDdiGetStandardAllocationDriverData","DxgkDdiOpenAllocation",
   "DxgkDdiCloseAllocation","DxgkDdiSetDisplayPrivateDriverFormat"],
   "kb":["03-object-model.md","04-memory.md"]}),

 ("009","wddm10-context-and-submission","BASE","partial",
  "WDDM 1.0: contexts and packet submission",
  "The first vertical slice of execution: record, patch, submit, complete. Uses "
  "physical addressing and DxgkDdiPatch, which WDDM 2.0 replaces with GPU "
  "virtual addressing. Deliberately throwaway: it proves the Render → Submit → "
  "fence pipeline with the least machinery.",
  {"deps":["008"],"slots":["DxgkDdiCreateContext","DxgkDdiDestroyContext",
   "DxgkDdiRender","DxgkDdiPatch","DxgkDdiSubmitCommand","DxgkDdiPreemptCommand",
   "DxgkDdiQueryCurrentFence","DxgkDdiBuildPagingBuffer"],
   "kb":["05-scheduling.md","10-warp-kmd.md"],
   "note":"Patch and physical addressing are retired by feature 017. Keep the "
          "implementation small and do not build on it."}),

 ("010","wddm10-tdr-baseline","BASE","implement",
  "WDDM 1.0: timeout detection and recovery",
  "The watchdog does not know there is no hardware. Progress tracking, reset and "
  "restart must be real from the first submission, not added later.",
  {"deps":["009"],"slots":["DxgkDdiResetFromTimeout","DxgkDdiRestartFromTimeout"],
   "kb":["05-scheduling.md"]}),

 ("011","wddm10-vidpn-and-modes","BASE","implement",
  "WDDM 1.0: VidPn topology and mode enumeration",
  "The display graph: sources, targets, paths, mode sets, and the cofunctional "
  "modality enumeration that silently breaks mode changes when under-populated.",
  {"deps":["006"],"slots":["DxgkDdiIsSupportedVidPn",
   "DxgkDdiRecommendFunctionalVidPn","DxgkDdiEnumVidPnCofuncModality",
   "DxgkDdiCommitVidPn","DxgkDdiUpdateActiveVidPnPresentPath",
   "DxgkDdiRecommendMonitorModes","DxgkDdiRecommendVidPnTopology",
   "DxgkDdiControlInterrupt","DxgkDdiGetScanLine"],
   "kb":["06-display.md"]}),

 ("012","wddm10-present-and-scanout","BASE","implement",
  "WDDM 1.0: present and scanout",
  "Getting pixels on screen, and the synthetic vsync DWM's frame pacing depends "
  "on.",
  {"deps":["011","008"],"slots":["DxgkDdiPresent","DxgkDdiSetVidPnSourceAddress",
   "DxgkDdiSetVidPnSourceVisibility"],
   "kb":["06-display.md"]}),

 ("013","wddm10-cursor-and-legacy","BASE","partial",
  "WDDM 1.0: cursor, and declining the legacy surface",
  "Hardware cursor is real and maps onto the virtio-gpu cursor queue. Palette, "
  "swizzling ranges, overlays and capture are legacy: decline them correctly and "
  "prove the decline.",
  {"deps":["012"],"slots":["DxgkDdiSetPointerPosition","DxgkDdiSetPointerShape",
   "DxgkDdiSetPalette","DxgkDdiAcquireSwizzlingRange","DxgkDdiReleaseSwizzlingRange",
   "DxgkDdiCreateOverlay","DxgkDdiUpdateOverlay","DxgkDdiFlipOverlay",
   "DxgkDdiDestroyOverlay","DxgkDdiStopCapture"],
   "kb":["06-display.md"],
   "note":"Only the two pointer DDIs are implement; the rest are stub."}),

 # ---- later WDDM levels, generated from the DDI table ---------------------
 ("014","wddm11-win7-additions","WIN7","partial",
  "WDDM 1.1 (Windows 7) additions", None, {"deps":["013"],"kb":["08-wddm3x-delta.md"]}),
 ("015","wddm12-win8-additions","WIN8","implement",
  "WDDM 1.2 (Windows 8) additions", None, {"deps":["014"],"kb":["05-scheduling.md"]}),
 ("016","wddm13-additions","WDDM1_3","partial",
  "WDDM 1.3 additions", None, {"deps":["015"],"kb":["06-display.md"]}),
 ("017","wddm20-gpu-va-and-paging","WDDM2_0","implement",
  "WDDM 2.0: GPU virtual addressing, paging queues, monitored fences",
  "The pivot of the whole project. GPU VA replaces physical addressing and "
  "DxgkDdiPatch is never called again; residency becomes asynchronous; monitored "
  "fences arrive. Everything after this depends on it.",
  {"deps":["016"],"kb":["04-memory.md","03-object-model.md"]}),
 ("018","wddm21-additions","WDDM2_1","partial",
  "WDDM 2.1 additions", None, {"deps":["017"],"kb":["06-display.md"]}),
 ("019","wddm22-hardware-queues","WDDM2_2","implement",
  "WDDM 2.2: hardware contexts and queues",
  "Hardware scheduling: the driver owns a queue the device drains, instead of "
  "VidSch arbitrating one packet at a time.",
  {"deps":["018"],"kb":["05-scheduling.md"]}),
 ("020","wddm23-27-incremental","WDDM2_3+","partial",
  "WDDM 2.3 – 2.7 incremental additions",
  "Four sparse levels taken together. Each is a handful of slots and none "
  "restructures anything.",
  {"deps":["019"],"merge":["WDDM2_3","WDDM2_5","WDDM2_6","WDDM2_7"],
   "kb":["08-wddm3x-delta.md"]}),
 ("021","wddm24-additions","WDDM2_4","partial",
  "WDDM 2.4 additions", None, {"deps":["019"],"kb":["08-wddm3x-delta.md"]}),
 ("022","wddm29-flip-queue-logs","WDDM2_9","partial",
  "WDDM 2.9: flip queue log buffers", None, {"deps":["021","020"],"kb":["08-wddm3x-delta.md"]}),
 ("023","wddm30-additions","WDDM3_0","partial",
  "WDDM 3.0: backing stores and CPU events", None, {"deps":["022"],"kb":["08-wddm3x-delta.md"]}),
 ("024","wddm31-doorbells-native-fences","WDDM3_1","implement",
  "WDDM 3.1: doorbells and native fences",
  "User-mode submission. The performance story: the guest ICD writes its ring "
  "and stores to a doorbell with no kernel transition, and the KMD leaves the "
  "fast path.",
  {"deps":["023"],"kb":["05-scheduling.md","08-wddm3x-delta.md"]}),
 ("025","wddm32-additions","WDDM3_2","partial",
  "WDDM 3.2: native fence sharing, and declining GPU-P and live migration",
  "Implement native fence sharing, ResetDisplayEngine and CollectDbgInfo2. "
  "Decline the SR-IOV/partitioning family outright — it describes mediated "
  "hardware passthrough, which this design is not. Live migration and dirty "
  "tracking are deferred, not declined.",
  {"deps":["024"],"kb":["08-wddm3x-delta.md","09-gpupv.md"]}),

 # ---- the QEMU device ------------------------------------------------------
 ("026","device-skeleton-and-threading",None,"implement",
  "QEMU device: skeleton, object model and threading",
  "virtio-gpu-rvk-pci as a VirtIOGPU subclass in a patched QEMU package, one "
  "instance per host GPU bound by Vulkan device UUID (host-gpu property), and "
  "tools/host/list_gpus to discover host GPUs and generate the per-machine QEMU "
  "or libvirt configuration. The threading contract: no decode or execution on "
  "the main loop, one worker per context, virtqueue state only under the BQL.",
  {"deps":["004"],"kb":["11-implementation-plan.md"]}),
 ("027","device-2d-and-display",None,"implement",
  "QEMU device: 2D resources, scanout and EDID",
  "The base class's 2D protocol, which feature 012 talks to. Testable against "
  "stock virtio-gpu-pci before the custom device exists.",
  {"deps":["026"],"kb":["06-display.md"]}),
 ("028","device-blob-memory",None,"implement",
  "QEMU device: blob resources and host-visible memory",
  "Guest-backed blobs for rings and fence pages; host-visible blobs mapped into "
  "the hostmem BAR so guest vkMapMemory is zero-copy onto host GPU memory.",
  {"deps":["027"],"kb":["04-memory.md","11-implementation-plan.md"]}),
 ("029","device-capset-and-contexts",None,"implement",
  "QEMU device: rvk capset and context lifetime",
  "CTX_CREATE with the private capset id, protocol version negotiation, sub-API "
  "selection, per-context state and teardown.",
  {"deps":["028"],"kb":["11-implementation-plan.md"]}),
 ("030","device-ring-and-doorbell",None,"implement",
  "QEMU device: submission ring and doorbell",
  "The hot path, deliberately outside the virtio spec: a per-context ring in "
  "guest memory and a doorbell register backed by a KVM ioeventfd.",
  {"deps":["029"],"kb":["05-scheduling.md","11-implementation-plan.md"]}),
 ("031","device-fences-and-interrupts",None,"implement",
  "QEMU device: fence pages and interrupt injection",
  "Fence page layout and monotonicity, per-context fence rings, MSI-X injected "
  "from a worker thread through a KVM irqfd with vector masking honoured.",
  {"deps":["030"],"kb":["05-scheduling.md"]}),
 ("032","device-execution-and-codec",None,"implement",
  "QEMU device: decode and execute",
  "Bind the generated decoders to a host dispatch table and execute "
  "on host Vulkan/GL/CL. The decoders are the primary attack surface of the "
  "whole system.",
  {"deps":["031"],"kb":["11-implementation-plan.md"]}),
 ("033","device-fault-and-reset",None,"implement",
  "QEMU device: fault, reset and recovery",
  "A faulted context is torn down without disturbing others, reported to the "
  "guest, and reconciled with the guest's TDR.",
  {"deps":["032"],"kb":["05-scheduling.md"]}),

 # ---- cross-cutting suites -------------------------------------------------
 ("034","loopback-protocol-suite",None,"implement",
  "Loopback protocol test suite",
  "Guest core and device core in one Linux process over an in-memory transport. "
  "Where protocol design errors get found before either side is written.",
  {"deps":["030","031"],"kb":["11-implementation-plan.md"]}),
 ("035","guest-integration-suite",None,"implement",
  "In-guest integration suite",
  "Driver Verifier, D3DKMT-level tests mirroring the loopback suite, and the "
  "application suite. Driven from Linux over the QEMU guest agent with a "
  "snapshot before every install.",
  {"deps":["034"],"kb":["11-implementation-plan.md"]}),

 # ---- live bring-up: M0-M2 in a real guest ---------------------------------
 ("036","device-interrupt-lifetime",None,"implement",
  "QEMU device: fence interrupts that follow the driver's lifetime",
  "The device's fence interrupts are connected while the guest driver holds "
  "DRIVER_OK and released when it clears it or resets the device, and a guest "
  "that disables and re-enables MSI-X - as Windows does whenever it restarts a "
  "display driver - can no longer abort QEMU with a duplicate irqfd. Found by "
  "the first live run of the in-guest suite.",
  {"deps":["031","035"],"kb":["05-scheduling.md","11-implementation-plan.md"]}),
 ("037","guest-bring-up",None,"implement",
  "Real-guest bring-up: the in-guest suite passes",
  "rvkgpu.sys starts on the real dxgkrnl under Driver Verifier and rvktest.exe "
  "passes. Every disagreement between the fake dxgkrnl and Windows that L5 "
  "exposes is first reproduced as a harness scenario, then fixed in core; the "
  "driver logs every refusal it makes at the point of decision.",
  {"deps":["036"],"kb":["02-kmd-contract.md","10-warp-kmd.md","12-re-cookbook.md"]}),
 ("038","guest-desktop",None,"implement",
  "Real-guest desktop: DWM on rvkgpu at a chosen mode",
  "M1 in a real guest: the Windows desktop runs through rvkgpu, composited by "
  "DWM on WARP, with modes from the EDID, synthetic vsync, VidPn commit and "
  "scanout through the 2D path; mode changes, display power and several outputs "
  "work, and the host sees every frame.",
  {"deps":["037"],"kb":["06-display.md","11-implementation-plan.md"]}),
 ("039","host-memory-feasibility",None,"implement",
  "Host memory and interrupt feasibility on real host GPUs",
  "M2's spikes, run on every host GPU class and recorded: exported host GPU "
  "memory mapped into a KVM memslot through the hostmem BAR, guest-backed pages "
  "imported with VK_EXT_external_memory_host through a memfd window, and irqfd "
  "injection from workers under vector-masking stress. Each outcome becomes a "
  "capset bit with a tested fallback, so no guest component guesses.",
  {"deps":["028","031","036"],"kb":["04-memory.md","11-implementation-plan.md"]}),
 ("040","guest-power-and-recovery",None,"implement",
  "Real-guest power, restart and recovery",
  "The adapter survives what Windows does to a GPU: sleep and hibernate with "
  "resume, device disable and enable, a driver upgrade without reboot, TDR on a "
  "stalled host context that recovers only that device, and a host executor "
  "that faults - each exercised by the in-guest runner, not only the harness.",
  {"deps":["038","033"],"kb":["05-scheduling.md","02-kmd-contract.md"]}),

 # ---- the seam: the guest clients over rvkgpu (M3) --------------------------
 ("041","kmd-host-visible-memory",None,"implement",
  "KMD: host-visible memory a process can map",
  "Host-visible blobs become allocations in a CPU-visible memory segment backed "
  "by the hostmem BAR, so D3DKMTLock2 and a GPU mapping hand a process real host "
  "GPU memory with no copy; guest-backed rings and fence pages map the same way; "
  "32-bit processes get views below 4 GiB; budgets, residency and eviction are "
  "reported truthfully from the capset bits feature 039 defines.",
  {"deps":["037","039"],"kb":["04-memory.md","11-implementation-plan.md"]}),
 ("042","host-executor-library",None,"implement",
  "Host executor: this project's backends behind rvk_executor.h",
  "A C-ABI executor library, built here from this project's Vulkan, OpenGL and "
  "OpenCL host backends, that the device loads by its executor property: one "
  "rvk context is one session on one worker, commands decode into the backend "
  "for the context's sub-API, host-visible and guest-backed blobs are imported "
  "rather than copied, and every decode or device-loss failure becomes a fault "
  "on the wire. An executor conformance suite in this repository holds any "
  "executor library to the contract.",
  {"deps":["032","039","063"],"kb":["11-implementation-plan.md"],
   "note":"This repository owns all of it: the ABI (src/device/abi/rvk_executor.h), "
          "the conformance suite, the device side, and the backends themselves. "
          "The wire it decodes is generated by feature 063."}),
 ("043","guest-kmt-transport",None,"implement",
  "Guest transport: the generated encoders over D3DKMT and the rvk ring",
  "The guest half of the seam: a user-mode transport carrying this project's "
  "generated Vulkan, OpenGL and OpenCL encoders - a per-process link shared by "
  "all three APIs, synchronous and "
  "asynchronous commands, command streams, a reply area, a blob arena of "
  "host-visible memory, fences and completed sequence numbers - built from an "
  "rvk context per sub-API, its ring and doorbell, fence waits that sleep "
  "instead of polling, and allocations from feature 041. Portable logic tested "
  "on Linux over MockKmt and the loopback rig.",
  {"deps":["041","024","034","063"],"kb":["05-scheduling.md","07-usermode.md","11-implementation-plan.md"],
   "note":"The transport interface is this repository's, defined here; the "
          "generated client dispatch is handed one. There is no external "
          "interface to implement."}),
 ("044","rvk-context-live",None,"implement",
  "End to end: rvk contexts in a real guest",
  "M3 done in a real guest: a test program opens the adapter, creates a context "
  "for each sub-API through the transport, allocates and maps host-visible "
  "memory, submits command streams that the real executor runs on the host GPU, "
  "waits on fences, and has a deliberately faulted context recover without "
  "taking the adapter or the other contexts down.",
  {"deps":["037","042","043"],"kb":["05-scheduling.md","11-implementation-plan.md"]}),

 # ---- Vulkan, and D3D through it (M4, M5) ------------------------------------
 ("045","vulkan-icd",None,"implement",
  "Vulkan: the ICD on rvkgpu",
  "This project's generated Vulkan ICD over the guest transport, found by the Vulkan "
  "loader through the adapter's driver registration rather than a global key: "
  "each physical device is one rvkgpu adapter with its LUID, memory types come "
  "from the adapter's segments and budgets, timeline semaphores and fences wait "
  "on rvk fences, and a TDR or fault reports VK_ERROR_DEVICE_LOST. Vulkan 1.3, "
  "or the host's version if lower.",
  {"deps":["044"],"kb":["07-usermode.md","11-implementation-plan.md"]}),
 ("046","vulkan-present",None,"implement",
  "Vulkan: presentation through the KMD",
  "VK_KHR_win32_surface and VK_KHR_swapchain present through rvkgpu instead of "
  "GDI: swapchain images are allocations the host renders into, presented with "
  "D3DKMT and composed by DWM, paced by the adapter's vertical blank; FIFO, "
  "mailbox and immediate modes, resize, occlusion and fullscreen behave as on "
  "a hardware driver.",
  {"deps":["045","038","041"],"kb":["06-display.md","07-usermode.md"]}),
 ("047","vulkan-for-d3d-translation",None,"implement",
  "Vulkan: the feature set DXVK and vkd3d-proton need",
  "Every extension and feature DXVK (D3D8 to D3D11) and vkd3d-proton (D3D12) "
  "require, forwarded when the host GPU has it and absent when it does not, a "
  "32-bit ICD for WoW64 games, and Win32 external memory and semaphores for DXGI "
  "interop - so D3D applications run on rvkgpu with the translation layers "
  "placed beside them.",
  {"deps":["046"],"kb":["07-usermode.md"]}),
 ("048","vulkan-conformance",None,"implement",
  "Vulkan: conformance and the application suite in the guest",
  "The L5 application suite: a Vulkan CTS mustpass subset, vkcube, and DXVK and "
  "vkd3d-proton samples run in the guest by the in-guest runner, with rendered "
  "frames compared against references and pass rates recorded as a ratchet.",
  {"deps":["047"],"kb":["11-implementation-plan.md"]}),

 # ---- the hot path leaves the kernel (M6) ------------------------------------
 ("049","user-mode-submission-live",None,"implement",
  "User-mode submission in the guest: doorbells and native fences",
  "The transport submits by writing its ring and storing to a WDDM doorbell "
  "mapped into the process, and waits on native fences, so the KMD is out of "
  "the submission path; doorbells disconnected by suspend or TDR are handled; "
  "per-submit cost is measured against D3DKMTSubmitCommand and held by a "
  "benchmark ratchet.",
  {"deps":["045","024"],"kb":["05-scheduling.md","08-wddm3x-delta.md"]}),

 # ---- OpenGL and OpenCL (M7) ---------------------------------------------------
 ("050","opengl-icd",None,"implement",
  "OpenGL: an installable client driver",
  "An OpenGL ICD the system opengl32.dll loads for the adapter - the Drv* "
  "interface, pixel formats, contexts and share lists, the WGL_ARB extensions - "
  "over the generated OpenGL encoders and the guest transport, so every OpenGL "
  "application uses the host GPU without a DLL placed beside it. OpenGL 4.6 "
  "core, executed on the host through EGL.",
  {"deps":["044"],"kb":["07-usermode.md","11-implementation-plan.md"]}),
 ("051","opengl-compatibility-and-present",None,"implement",
  "OpenGL: compatibility profile and presentation",
  "The compatibility profile Windows OpenGL applications still use - fixed "
  "function, immediate mode, legacy state - and SwapBuffers presented through "
  "rvkgpu like Vulkan's swapchain, with swap intervals paced by the vertical "
  "blank and no read-back through GDI.",
  {"deps":["050","046"],"kb":["06-display.md"]}),
 ("052","opencl-icd",None,"implement",
  "OpenCL: the ICD on rvkgpu",
  "This project's generated OpenCL ICD over the guest transport, registered through the "
  "adapter's driver registration: OpenCL 3.0 with the host device's optional "
  "features, coarse-grained shared virtual memory over host-visible memory, "
  "non-blocking transfers and event callbacks that cannot deadlock, and one "
  "platform device per rvkgpu adapter.",
  {"deps":["044"],"kb":["07-usermode.md"]}),
 ("053","cross-api-sharing",None,"implement",
  "Sharing across APIs and processes",
  "Allocations and fences applications share between processes through NT "
  "handles - beyond the presentation paths' own sharing with DWM in 046 and 051 - "
  "with device-global identity on the host, and the interop extensions built on "
  "them: Vulkan Win32 external memory and semaphores, OpenGL external objects, "
  "OpenCL external memory and cl_khr_gl_sharing - the machinery DXGI, DWM and "
  "multi-API applications depend on.",
  {"deps":["047","051","052"],"kb":["03-object-model.md","04-memory.md"]}),
 ("054","opengl-opencl-conformance",None,"implement",
  "OpenGL and OpenCL: conformance in the guest",
  "Khronos OpenGL conformance and piglit subsets and an OpenCL CTS subset, run "
  "in the guest by the in-guest runner with rendered output compared against "
  "references, pass rates recorded as a ratchet, and every claimed version and "
  "extension backed by a passing test.",
  {"deps":["051","052","053"],"kb":["11-implementation-plan.md"]}),

 # ---- Direct3D through the Microsoft runtime, and the desktop (M8) -----------
 ("055","umd-foundation",None,"implement",
  "rvkumd.dll: the user-mode driver and DXGI",
  "The user-mode driver the D3D runtimes load for the adapter: adapter open, "
  "the DXGI DDI (present, blit, resource rotation), adapter identity, memory "
  "and feature levels reported truthfully so DXGI and DXCore enumerate rvkgpu "
  "as a real GPU, and the allocation private data the KMD validates - built on "
  "the same transport as the ICDs.",
  {"deps":["049","053"],"kb":["07-usermode.md","03-object-model.md"]}),
 ("056","d3d11-umd",None,"implement",
  "Direct3D 10 and 11: the user-mode driver",
  "The D3D10/11 UMD DDI over Vulkan remoting - resources, DXBC shaders "
  "translated to SPIR-V, state, draws and compute, queries and maps - so DWM, "
  "Direct2D, DirectWrite, browsers and applications shipping their own D3D11 "
  "run on rvkgpu through the Microsoft runtime; feature level 11_1 reported only "
  "when it is complete.",
  {"deps":["055"],"kb":["07-usermode.md"]}),
 ("057","d3d12-umd",None,"implement",
  "Direct3D 12: the user-mode driver",
  "The D3D12 UMD DDI over Vulkan remoting - command queues and lists, "
  "descriptor heaps, pipeline states with DXIL translated to SPIR-V, residency "
  "through MakeResident and Evict, and fences - so applications using the "
  "system d3d12.dll run on rvkgpu with no DLL replaced, at the feature level the "
  "host GPU supports.",
  {"deps":["056"],"kb":["07-usermode.md","04-memory.md","05-scheduling.md"]}),
 ("058","host-composition-scanout",None,"implement",
  "The desktop on the host GPU: zero-copy composition and scanout",
  "DWM composites on rvkgpu instead of WARP, primary surfaces are host GPU "
  "images, flips go through SetVidPnSourceAddress with vertical-blank "
  "completion, and the device hands the final image to QEMU's display as a "
  "dmabuf - no frame crosses back into guest memory.",
  {"deps":["056","046","038"],"kb":["06-display.md","07-usermode.md","11-implementation-plan.md"]}),
 ("059","d3d-conformance",None,"implement",
  "Direct3D: conformance and the application suite",
  "The HLK graphics tests as a correctness oracle (unsigned), DXVK and "
  "vkd3d-proton test suites, and games and benchmarks through both the "
  "Microsoft runtime and the translation layers, run in the guest with frames "
  "compared against references and every reported feature level backed by a "
  "passing test.",
  {"deps":["057","058","064"],"kb":["07-usermode.md"]}),

 # ---- a product ----------------------------------------------------------------
 ("060","multi-gpu-live",None,"implement",
  "One adapter per host GPU in the guest",
  "Several virtio-gpu-rvk-pci devices, one per host GPU, each an rvkgpu adapter "
  "the guest sees as its own GPU: LUIDs that match across Vulkan, OpenGL, "
  "OpenCL, DXGI and DXCore, per-adapter memory budgets and Task Manager engines, "
  "and D3D12 and Vulkan applications choosing between them.",
  {"deps":["048"],"kb":["11-implementation-plan.md"]}),
 ("061","driver-package",None,"implement",
  "One installable driver package",
  "A single driver package that installs the KMD, the UMD and the Vulkan, "
  "OpenGL and OpenCL drivers in 64-bit and WoW64 32-bit forms, with the Khronos "
  "Vulkan and OpenCL loaders where Windows has none, registered through the "
  "adapter as a DCH-compliant package does, upgraded and removed cleanly, and "
  "refusing a device whose protocol version it cannot speak.",
  {"deps":["047","054","055","057","060","064"],"kb":["07-usermode.md"]}),
 ("062","performance-and-soak",None,"implement",
  "Performance, stability and soak",
  "Per-API benchmarks against the same host GPU natively with recorded "
  "thresholds, hours-long soak runs under host stalls, memory pressure and TDR "
  "injection, and leak accounting in the guest and the device, all run by the "
  "in-guest runner and held as ratchets.",
  {"deps":["040","042","059","061"],"kb":["05-scheduling.md","11-implementation-plan.md"]}),

 # ---- the wire, generated here ----------------------------------------------
 ("063","render-server-integration",None,"implement",
  "The generators, vendored",
  "render-server's Khronos-registry generators, and nothing else of it: "
  "vendored into tools/codegen/, owned and edited here, pointed at this "
  "project's own wire types, and run against vk.xml, gl.xml and cl.xml pinned "
  "by revision and hash. What they emit is committed under src/proto/generated/, "
  "compiled and fuzzed like any other source, and a gate re-runs the generators "
  "to prove the committed tree is what they would write.",
  {"deps":["001","032"],"kb":["11-implementation-plan.md","07-usermode.md"],
   "note":"Numbered last because it was identified last; 042 and 043 depend on "
          "it, so the roadmap schedules it before them. Nothing fetches, links "
          "or pkg-configs render-server: it contributes no runtime."}),
 ("064","d3d9-umd",None,"implement",
  "Direct3D 9: the user-mode driver",
  "The D3D9 UMD DDI in rvkumd.dll over Vulkan remoting - resources, fixed "
  "function and shader model 1-3 translated to SPIR-V, state blocks, draws, "
  "locks and presents - so Direct3D 9 applications, and DirectDraw applications "
  "Windows runs on top of it, use the host GPU through the Microsoft runtime "
  "without DXVK placed beside them.",
  {"deps":["055"],"kb":["07-usermode.md"]}),

 # ---- debugging the wire ------------------------------------------------------
 ("065","wire-recording",None,"implement",
  "Wire recordings: capture, inspect and replay",
  "The device records what crosses the wire - each context's setup, every "
  "batch as the executor received it, replies, fences, faults and blob "
  "lifetimes - to a file on the host, flushed so a crash keeps what led to it, "
  "switched on per run and bounded in size. A host tool prints a recording "
  "with command names and decoded arguments, and replays it into an executor "
  "library, with the Khronos validation layers or another GPU, so a rendering "
  "mistake, a crash or a validation error is reproduced without a guest.",
  {"deps":["042","063"],"kb":["11-implementation-plan.md"],
   "note":"Requested 2026-09-14 for debug analysis of rendering mistakes, crashes "
          "and validation. Numbered last because it was identified last; it depends "
          "only on the executor and the generated wire, so the roadmap can schedule "
          "it right after 044."}),
]
