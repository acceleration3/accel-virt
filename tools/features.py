"""The feature decomposition: WDDM levels as implementation phases, plus the
QEMU device and the foundations. Consumed by gen_specs.py.

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
  "virtio-gpu-rvk-pci as a VirtIOGPU subclass in a patched QEMU package. The "
  "threading contract: no decode or execution on the main loop, one worker per "
  "context, virtqueue state only under the BQL.",
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
  "Bind render-server's generated decoders to a host dispatch table and execute "
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
]
