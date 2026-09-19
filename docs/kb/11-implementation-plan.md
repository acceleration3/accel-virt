# 11 — Implementation plan: a paravirtual WDDM 3.2 GPU on a custom QEMU device

## Decisions this plan is built on

1. **Host is QEMU/KVM on Linux.** No Hyper-V, no Windows host.
2. **No ivshmem.** The transport is a virtio-gpu device.
3. **The host side lives inside QEMU.** A new in-process device,
   `virtio-gpu-rvk-pci`, owns transport, memory mapping, fences, scanout and
   host execution. No separate backend process, no vhost-user.
4. **Serialization is generated, and generated here.** `render-server`
   contributed its Khronos-registry generators and nothing else; they are
   vendored in `tools/codegen/` and own the encoders (linked into the guest
   ICDs/UMD) and the decoders (linked into the QEMU device). The generated
   code is not a transport, not a server, not an executor (feature 063).
5. **D3D is translated in the guest** by vkd3d-proton and DXVK, which are
   ordinary Vulkan clients. Vulkan, OpenGL and OpenCL are the three serialized
   APIs.
6. **Target is full render + present** on WDDM 3.2 (`0x11007`).

## Where the cut line goes, and why

| Cut at | Guest ships | Host must be | Verdict |
|---|---|---|---|
| **D3DKMT** (GPU-PV, see `09-gpupv.md`) | nothing; host driver files projected in | Windows, same GPU vendor | Unavailable on a Linux host. |
| **The DDI** (forward `DxgkDdi*`) | a KMD | something that executes vendor DMA buffers | Nothing to forward to. The DMA format is the vendor's. |
| **The graphics API** (Vulkan/GL/CL) | ICDs + a KMD | Linux with any Vulkan/GL/CL driver | **Chosen.** |

The KMD's job is therefore **not** to execute graphics. Its job is to be a
credible WDDM GPU: own allocations, GPU virtual address space, fences and
scanout, and move serialized command streams to the device.

## Components

```
 GUEST (Windows 11 24H2)                           HOST (Linux)
 ┌──────────────────────────────────────────┐     ┌─────────────────────────────────────────────┐
 │ app                                      │     │ qemu-system-x86_64  (patched package)       │
 │  ├ d3d12.dll   = vkd3d-proton  prebuilt  │     │  └ virtio-gpu-rvk-pci           NEW         │
 │  ├ d3d11/dxgi  = DXVK          prebuilt  │     │     ├ VirtIOGPU base: 2D, scanout, EDID,    │
 │  ├ vulkan-1.dll (Khronos loader)         │     │     │  cursor, control queue   (stock QEMU) │
 │  │  └ rvk Vulkan ICD   ─┐                │     │     ├ capset + 3D commands                  │
 │  ├ opengl32 → rvk GL ICD ├ generated     │     │     ├ hostmem BAR  (mappable host memory)   │
 │  └ OpenCL   → rvk CL ICD ┘ encoders      │     │     ├ doorbell BAR (ioeventfd per context)  │
 │        │ D3DKMT (gdi32)                  │     │     ├ fence pages + irqfd MSI-X             │
 │        ▼                                 │     │     ├ worker thread per context             │
 │ dxgkrnl.sys / dxgmms2.sys                │     │     ├ generated decoders                    │
 │        │ DDI (187 slots)                 │     │     └ executor: host libvulkan / GL / CL    │
 │        ▼                                 │     │                                             │
 │ rvkgpu.sys   WDDM 3.2 KMD       NEW      │◄───►│   PCI: virtio-gpu, MSI-X                    │
 │  └ virtio-pci modern transport           │     └─────────────────────────────────────────────┘
 │ DWM / D2D / MF → D3D12Core               │
 │  └ rvkumd.dll  (last milestone)  NEW     │
 └──────────────────────────────────────────┘
```

Four things get built:

| Component | Language | Lives in |
|---|---|---|
| `rvkgpu.sys` — WDDM 3.2 miniport speaking virtio-gpu | C (kernel) | new driver project |
| `virtio-gpu-rvk-pci` — QEMU device | C (QEMU style) | a patch on the QEMU package |
| the serialization — encoders + decoders | C++23, generated | this repository, `src/proto/generated/` |
| Guest ICDs moved onto D3DKMT, later `rvkumd.dll` | C++23 | guest driver project |

## The QEMU device: `virtio-gpu-rvk-pci`

### Shape

Modeled directly on `virtio-gpu-rutabaga`, which QEMU 11.1.1 already ships as
the loadable module `hw-display-virtio-gpu-rutabaga.so`: a subclass of
`VirtIOGPU` that lets the base class handle the 2D protocol and overrides the
3D command set. New files, and nothing else in QEMU changes:

```
hw/display/virtio-gpu-rvk.c          the device (virtio-gpu-rvk-device)
hw/display/virtio-gpu-rvk-host.c     the host operations the device core runs on
hw/display/virtio-gpu-pci-rvk.c      the PCI wrapper (virtio-gpu-rvk-pci)
include/hw/virtio/virtio-gpu-rvk.h   the instance struct the PCI wrapper embeds
hw/display/meson.build               + modules 'virtio-gpu-rvk', 'virtio-gpu-pci-rvk'
meson_options.txt / meson.build      + option 'rvk' (feature), deps rvk_device, vulkan
```

The C files live in `src/device/qemu/` with the meson patch
(`qemu-build.patch`); `tools/build_qemu.sh` copies them into the pinned source
(`tools/fetch_qemu.sh`) and builds. No Kconfig symbol is needed: rutabaga's
module is selected by meson alone. The build stamp hashes only the version and
`configure` (`scripts/qemu-stamp.py`), so a module built from the same source
does match the distro binary - verified - but the binary's compiled-in module
table has no entry for new device types, so `qemu-system-x86_64` itself is
rebuilt (about a minute for `x86_64-softmmu`). Rebase the patch on each QEMU
update; keeping it to new files makes that trivial.

QEMU's meson builds no C++ in modules and its modinfo collector reads only `.c`,
so the device core is `librvk_device.a` from this repository's CMake, behind the
C ABI in `rvk_device.h`, found through `pkg-config` - as is the host executor
later. No C++ enters the QEMU tree.

### Protocol

Stock virtio-gpu is the envelope; the rvk wire format is the payload.

| Mechanism | virtio-gpu feature | Used for |
|---|---|---|
| Display | base protocol + `VIRTIO_GPU_F_EDID` | `GET_DISPLAY_INFO`, `GET_EDID`, `SET_SCANOUT`, `RESOURCE_FLUSH` |
| Context per guest API context | `VIRTIO_GPU_F_CONTEXT_INIT` | `CTX_CREATE` with a private capset id, per-context fence rings (`VIRTIO_GPU_FLAG_INFO_RING_IDX`) |
| Memory objects | `VIRTIO_GPU_F_RESOURCE_BLOB` | `RESOURCE_CREATE_BLOB`, `RESOURCE_MAP_BLOB` / `UNMAP_BLOB` into shm id `VIRTIO_GPU_SHM_ID_HOST_VISIBLE` |
| Slow-path commands | `SUBMIT_3D` on the control queue | context setup, object creation, rare calls |
| Hot-path commands | **device extension**: per-context ring + doorbell | everything high-rate — see below |

**Capset id.** Capset ids are 8 bits (`VIRTIO_GPU_CONTEXT_INIT_CAPSET_ID_MASK`)
and 1–6 are assigned (VIRGL, VIRGL2, GFXSTREAM_VULKAN, VENUS, CROSS_DOMAIN,
DRM). The spec reserves no private range. Pick a high id (e.g. `0x72`), define
it in exactly one header shared by device and driver, and let the capset
payload carry an rvk protocol version and a sub-API (Vulkan / GL / CL).

**The hot path is deliberately outside the virtio spec.** The control queue is
serviced on QEMU's main loop under the BQL; high-rate submission must not go
through it. Both ends are ours, so the device adds its own ring + doorbell,
negotiated through the capset.

### Threading

```
guest vCPU stores to doorbell[ctx]
  └ KVM ioeventfd (memory_region_add_eventfd)  — completed in the kernel, no BQL
      └ worker thread for ctx wakes
          decode   (the generated server dispatch)
          import   (a blob, as a host pointer or a dma-buf, once per blob)
          execute  (host Vulkan / GL / CL)
          advance fence value in the ctx's fence page
          event_notifier_set(guest notifier) → irqfd, KVM injects MSI-X, no BQL

control queue (CTX_CREATE, RESOURCE_CREATE_BLOB, SET_SCANOUT, …)
  └ main loop, BQL held → validate, hand off to the ctx thread, return
      completion that must touch virtqueue state
        └ aio_bh_schedule_oneshot → main loop → virtio_gpu_ctrl_response
```

Rules:

- **No decoding or execution on the main loop, ever.** Stock
  `virtio-gpu-gl` does much of its work there; that is a known bottleneck this
  design avoids.
- **One worker thread per guest context**, owning its host `VkQueue` or GL
  context. Satisfies Vulkan's external queue synchronization and GL's
  one-current-thread rule by construction.
- **Virtqueue state only under the BQL**, reached from workers by bottom half.
- **MSI-X from a worker through the virtqueue guest notifiers** virtio-pci
  already routes: `msix_set_vector_notifiers` holds one notifier slot per PCI
  device and virtio-pci owns it, so the device must not install its own irqfd
  routes. Enable the guest notifiers (`set_guest_notifiers`, as virtio-blk and
  vhost do) and `event_notifier_set(virtio_queue_get_guest_notifier(vq))` from
  the worker: with an in-kernel irqchip that is an irqfd with masking handled by
  virtio-pci; without one, virtio-pci's handler raises the interrupt on the main
  loop. Each fence ring is a virtqueue with its own vector.

### Memory

Two kinds of blob, chosen per allocation by the guest KMD:

**Host-visible (`BLOB_MEM_HOST3D` + `USE_MAPPABLE`) — the default for
anything the guest maps.** The worker allocates exportable host memory
(`VK_KHR_external_memory_fd`), `mmap`s it, and the device inserts that host VA
into the `hostmem` BAR at the offset given by `RESOURCE_MAP_BLOB`
(`memory_region_init_ram_ptr` + `memory_region_add_subregion`, the same thing
`virtio-gpu-gl` does for Venus). The guest `vkMapMemory` returns a pointer
into the BAR: real host GPU memory, zero copy.

**Guest-backed (`BLOB_MEM_GUEST`) — rings, fence pages, staging.** The KMD
locks pages and sends their GPA list; the device maps them once for the
object's lifetime. Rings and fence pages live here because the device touches
them from worker threads constantly.

Importing guest-backed pages into host Vulkan with
`VK_EXT_external_memory_host` needs one contiguous host VA range, and locked
guest pages are scattered. With guest RAM on `memory-backend-memfd`, the device
can build a contiguous window by `mmap`ing the memfd at each page's file
offset into a reserved region. Treat this as an optimization with copy as the
fallback, not as a dependency.

### Scanout

- **M1:** the base class's 2D path — `RESOURCE_CREATE_2D`,
  `RESOURCE_ATTACH_BACKING`, `SET_SCANOUT`, `RESOURCE_FLUSH`. Copies from guest
  pages. Unchanged stock behaviour.
- **Later:** host VRAM images exported as dmabuf and handed to QEMU's display
  with `dpy_gl_scanout_dmabuf`. Needs a GL-capable QEMU display
  (`-display gtk,gl=on`, `egl-headless`, `dbus,gl=on`, or SPICE with GL).

### Faults and reset

A context whose host API call reports device loss, or whose decode fails, is
marked faulted in its fence page and gets an interrupt. The device destroys
the host context; the guest KMD completes what the context queued and refuses
its later submissions. It never reports `DXGK_INTERRUPT_DMA_FAULTED`, which
dxgmms2.sys bugchecks 0x119/0xD on for a GPU MMU adapter
(`VidSchDdiNotifyInterruptWorker` @140031b80; `docs/kb/05-scheduling.md`). A malformed frame never asserts — it
faults the context: errors are values on the wire, never assertions.

**Security and stability trade-off, accepted deliberately:** guest-controlled
bytes are decoded inside the QEMU process, which also hosts the host GPU
driver. A decoder memory-safety bug is a guest-to-host escape at QEMU's
privilege, and a crash in the host GPU driver kills the VM. Every decoder read
must be bounds checked, the device should run QEMU under its usual sandboxing
(`-sandbox on`, libvirt's confinement), and fuzzing the decoders against
malformed frames belongs in the codec's test suite.

### One adapter per host GPU

Every host GPU becomes its own guest adapter: one `virtio-gpu-rvk-pci` instance
per host physical device, each loaded by its own `rvkgpu.sys` instance. That is
the only shape Windows understands as "several GPUs" - DXGI and D3D12 adapter
enumeration, D3D12 multi-adapter, per-GPU memory budgets and Task Manager all
come from separate WDDM adapters. A single adapter fronting several GPUs would
show them to Vulkan applications only, and WDDM's linked-adapter mechanism is
for chains of identical GPUs, not a heterogeneous set.

Making that work on any machine is a host-side job, done at launch:

- **Discovery.** `tools/host/list_gpus` enumerates host Vulkan physical devices
  - UUID, vendor, device, type, name, memory. With no GPU, `llvmpipe` appears as
  a CPU device, so there is always at least one.
- **Generated configuration.** The same tool emits one
  `-device virtio-gpu-rvk-pci,host-gpu=<uuid>` per GPU (or libvirt XML), so a
  machine's configuration is produced, not hand-written.
- **Binding by UUID.** The device opens the host physical device whose
  `VkPhysicalDeviceIDProperties::deviceUUID` matches, and fails to realize with
  a named error if it is absent. Enumeration order is not stable across boots
  or driver updates; the UUID is. `host-gpu=auto` binds the first device.
- **Capabilities, not vendor lists.** Each adapter's capset carries its GPU's
  identity and feature bits for what that GPU and host driver actually support
  (for example host-visible memory mapping). The guest driver and ICD branch on
  those bits, so a new vendor or a missing extension needs no guest change.
- **Vendor-neutral guest.** Commands cross at the API level and run on the
  host's own driver, so one `rvkgpu.sys` serves NVIDIA, AMD, Intel or llvmpipe.

### Launching

libvirt does not know the device, so pass it through - one `-device` per host
GPU, as generated by `tools/host/list_gpus`:

```xml
<domain type='kvm' xmlns:qemu='http://libvirt.org/schemas/domain/qemu/1.0'>
  <memoryBacking><source type='memfd'/><access mode='shared'/></memoryBacking>
  <devices><video><model type='none'/></video></devices>
  <qemu:commandline>
    <qemu:arg value='-device'/>
    <qemu:arg value='virtio-gpu-rvk-pci,host-gpu=5f3c...e1,hostmem=8G,max_outputs=1'/>
  </qemu:commandline>
</domain>
```

## The guest driver: `rvkgpu.sys`

### Binding and interrupts

- Binds to the device's PCI identity. The device sets a **distinct PCI
  subsystem id** so the INF matches `virtio-gpu-rvk-pci` and not stock
  `virtio-gpu-pci` (and so Red Hat's `viogpudo` never claims ours). During M0–M1
  the INF temporarily matches stock `PCI\VEN_1AF4&DEV_1050`.
- Message-signaled interrupts via the INF
  (`Interrupt Management\MessageSignaledInterruptProperties`, `MSISupported=1`,
  `MessageNumberLimit` 11). `DxgkDdiInterruptRoutine(MiniportDeviceContext,
  MessageNumber)` receives the vector number: config changes, the control and
  cursor queues, then eight the contexts' fence queues share round their
  doorbells (the runner starts the device with `vectors=11`).
- A real ISR replaces WARP's synthetic-interrupt worker thread. The ISR reads
  the fence page, calls `DxgkCbNotifyInterrupt(DMA_COMPLETED)`, returns TRUE;
  `DxgkCbQueueDpc` follows. `DxgkCbSynchronizeExecution` remains the tool for
  any completion raised outside the ISR.

### virtio-pci transport inside a WDDM miniport

`DxgkDdiStartDevice` receives the translated BAR resources in
`DXGK_DEVICE_INFO`. The KMD walks the virtio PCI capabilities (common, notify,
ISR, device config, shared memory), maps them with `DxgkCbMapMemory`, and runs
split virtqueues on memory it allocates itself. There is no KMDF or virtio
library underneath a WDDM miniport; this layer is written from the virtio 1.2
spec, with Red Hat's `kvm-guest-drivers-windows` (`viogpudo`, `VirtIO/`) as the
reference for doing it in a Windows kernel driver.

### Segments

| # | Kind | Backing |
|--:|---|---|
| 1 | Aperture | guest system memory → `BLOB_MEM_GUEST` |
| 2 | Local, `CpuVisible`, `CpuTranslatedAddress` = hostmem BAR | host GPU memory → `BLOB_MEM_HOST3D` + `MAP_BLOB` |

Segment 2 exists because the hostmem BAR is real storage the CPU can reach.
Paging between them is a device command, not a CPU copy, when both sides are
host-resolvable.

### GPU virtual addresses are host handles

Report GPU MMU support (`DXGKQAITYPE_GPUMMUCAPS`) and encode:

```
  bits 47..24   device object id
  bits 23..0    offset within the object
```

`UPDATE_PAGE_TABLE` / `COPY_PAGE_TABLE_ENTRIES` become map inserts,
`FLUSH_TLB` is a no-op, `DxgkDdiPatch` is never called. Virtual addressing is
also what unlocks hardware queues, user-mode submission and native fences.
Size the split against the codec's handle width before M3 and reject
allocations that do not fit at `CreateAllocation`, not at map time.

### One call, end to end

```
ICD      encode batch into ctx ring           (ring = locked allocation, mapped via Lock2)
ICD      D3DKMTSubmitCommand                  (DMA buffer = {ring, head, tail, fence})
KMD      DxgkDdiRender                        copy descriptor, allocation list, no patches
KMD      DxgkDdiSubmitCommandVirtual          DISPATCH_LEVEL: publish tail, store doorbell, return
QEMU     ioeventfd → worker                   decode → execute → fence page ← fence id
QEMU     irqfd                                MSI-X
KMD      ISR → DxgkCbNotifyInterrupt → DPC    VidSch completes the packet
dxgkrnl  monitored fence signalled
ICD      wait returns, read reply ring        (only for calls that return data)
```

Nothing on the submit path blocks, allocates or takes a mutex. Batching
non-returning calls and flushing at returning calls is the codec's decision.
From M6 the ICD stores the doorbell itself and the KMD leaves this path.

### Presentation, in order

1. **`DxgkDdiPresent` copy.** Host renders; the worker copies into the guest
   swapchain allocation; the KMD flips it via the 2D scanout path. Works with
   DWM on WARP and any host driver.
2. **Flip via `DxgkDdiSetVidPnSourceAddress`** onto an allocation in segment 2.
3. **Host-side composition.** Once DWM composites on `rvkgpu` (M8), the final
   desktop is a host VRAM image and scanout is `dpy_gl_scanout_dmabuf`. Nothing
   crosses back into the guest.

A synthetic vsync (`DxgkCbNotifyInterrupt(CRTC_VSYNC)` at the mode's refresh
rate, from a timer or a device vsync interrupt) is required from M1.

## Milestones

**M0 — load a stub miniport on stock QEMU.** `DriverEntry` → `DxgkInitialize`
with `Version = 0x11007`, stub DDIs, `QueryAdapterInfo` answering
`DRIVERCAPS` and one segment. INF matches stock `virtio-gpu-pci`. Establish the
loop: build, test-sign, push, snapshot, boot. Debug output from the first line
of `DriverEntry` through QEMU's `isa-debugcon` (`-device
isa-debugcon,iobase=0x402,chardev=…`; one `WRITE_PORT_UCHAR` per byte, legal at
any IRQL, no driver needed). **Done when** the device starts with no Code 43,
and its log lands in a host file.

**M1 — virtio transport and display, still stock QEMU.** Capability walk,
split virtqueues, MSI-X ISR, `GET_DISPLAY_INFO`, `GET_EDID`,
`RESOURCE_CREATE_2D`, `ATTACH_BACKING`, `SET_SCANOUT`, `RESOURCE_FLUSH`;
VidPn, cofunctional modality, synthetic vsync. **Done when** the guest desktop
runs at a chosen mode through `rvkgpu`, composited by DWM on WARP.

**M2 — the patched QEMU and a skeleton device.** `virtio-gpu-rvk-pci`
subclassing `VirtIOGPU`; M1 works unchanged on it. Add capset, `CTX_CREATE`,
doorbell BAR with ioeventfd, one worker thread, fence page, irqfd MSI-X.
**Done when** a ping command posted from a test program round-trips through
ring, doorbell, worker, fence and ISR, with latency measured.
Spikes to run here, because each can reshape M3. **Feature 039 ran them**; the
per-GPU answers are in [`04-memory.md`](04-memory.md), and the short version is:
  - Host-visible mapping: **yes on both GPUs here** - but on NVIDIA only through
    the dma-buf export. Its opaque-fd export gives a descriptor `mmap` refuses
    with `EINVAL`, so a device that tries one handle type only would conclude
    the path does not exist. Segment 2 does not have to fall back on this host.
  - Guest-backed import: **yes on both**, over guest RAM that is
    `memory-backend-memfd,share=on`; the device refuses a window it can only
    reach through a bounce buffer.
  - irqfd under masking: the device counts what it injected
    (`InterruptStressProbe`) and the guest suite's `--stress-minutes` run awaits
    fences while the vertical blank's interrupt is enabled and disabled around
    them.

Each answer is a capset bit the guest only sees where the probe passed, and the
device decides it at realize on the GPU that instance is bound to.

**M3 — memory and execution in the KMD.** Two segments, blob creation and
mapping, GPU-VA-as-handle, `BuildPagingBuffer` for `TRANSFER`,
`MAP_APERTURE_SEGMENT`, `UPDATE_PAGE_TABLE`, `FLUSH_TLB`; contexts;
`SubmitCommandVirtual`; monitored fences; TDR plumbing. Device side: object
table, blob mapping, per-context threads. **Done when**
`D3DKMTCreateAllocation2` + `MapGpuVirtualAddress` + `SubmitCommand` + a fence
wait completes from a test program, and a deliberately faulted context
recovers without taking the adapter down.

**M4 — Vulkan end to end.** Vulkan ICD on D3DKMT with the codec's encoders;
device links the decoders and executes on host libvulkan. **Done when** the
existing Vulkan probe/test suite passes in the guest with only this path in
use, and vkd3d-proton's and DXVK's required extensions are reported.

**M5 — presentation.** Swapchain allocations, `DxgkDdiPresent` copy, then
flip. **Done when** a vkd3d-proton D3D12 sample renders in a window on the
guest desktop.

**M6 — user-mode submission.** Doorbells (`CreateDoorbell` /
`ConnectDoorbell` map the context's doorbell page into the process;
`DisconnectDoorbell` remaps a scratch page) and native fences. **Done when**
the M5 sample runs with the KMD out of the submission path and per-submit cost
is measured lower.

The guest transport (feature 043) is how a process reaches a host rvk context
before those DDIs land. A process asks for a context with
`D3DKMTCreateContextVirtual`, carrying the **context record** (RVKT,
docs/protocol.md) as private data: it names, by the process's own allocation
handles, the ring, fence page and reply slot. The miniport decodes it, publishes
those allocations as GUEST blobs, and sends the context's setup. The process
then writes a batch into the ring it mapped, leaves the submission descriptor
(RVKS) in the DMA buffer of `D3DKMTSubmitCommandVirtual`, and the miniport rings
the doorbell it owns; the process waits on the D3DKMT submission fence, which the
miniport completes from the fence-page interrupt. `KmtTransport` (src/guest/
transport) is the `ClientTransport` the generated client is handed for all this.

**M7 — OpenGL and OpenCL.** Same kernel path, same device path, different
capset sub-API and codec. **Done when** each API's existing test suite passes
in the guest.

**M8 — a UMD for the desktop, and for Direct3D.** `rvkumd.dll` implementing the
D3D10/11 and D3D12 UMD DDIs over the same Vulkan remoting path, so DWM
composites on `rvkgpu` and applications using the Microsoft runtime - not only
those shipping vkd3d-proton or DXVK beside them - run on the host GPU; switch
scanout to dmabuf. **Done when** DWM no longer falls back to WARP, desktop
frames reach QEMU's display without a guest copy, and the system `d3d11.dll`
and `d3d12.dll` run their conformance subsets on the adapter.

TDR is not a milestone. It is paid for at M3 and revisited at every milestone
after.

### Milestones as features

`docs/roadmap.md` is the executable form of these milestones; each row there
has a spec. A milestone is done when its features are.

| Milestone | Features |
|---|---|
| M0 — a stub miniport loads | 001–007; live: 035, 037 |
| M1 — transport and display | 005, 011–013, 027; live: 038 |
| M2 — the patched QEMU and a skeleton device | 026, 028–031, 034; live: 036, 039 |
| M3 — memory and execution | 008–010, 014–023, 032, 033; live: 040–044 |
| M4 — Vulkan end to end | 045, 047, 048 |
| M5 — presentation | 046 |
| M6 — user-mode submission | 024, 025, 049 |
| M7 — OpenGL and OpenCL | 050–054 |
| M8 — the UMD, the desktop and Direct3D | 055–059 |
| After M8 — a product | 060–062 |

## Build, signing, and the development loop

- **Driver build:** `clang-cl` + `lld-link` on Linux against the WDK and SDK
  headers in `third_party/windows-kit/include`, linking `displib.lib`,
  `ntoskrnl.lib`, `hal.lib`, `BufferOverflowK.lib` from
  `third_party/windows-kit/lib/x64` (`tools/fetch_windows_kit.sh`). `DxgkInitialize` comes
  from `displib.lib`. mingw cannot build a WDDM miniport. Decide at M0 whether
  SAL/SEH friction warrants MSVC + WDK in a Windows build VM instead.
- **QEMU build:** Arch `qemu` PKGBUILD with the device patch, installed as a
  local package.
- **Signing:** `bcdedit /set testsigning on` with a self-signed test
  certificate; Secure Boot off in the guest firmware. Production signing (EV
  certificate, Partner Center, HLK graphics tests) is out of scope.
- **Recovery:** `virsh snapshot-create` before every driver install. A KMD that
  fails in `StartDevice` can leave the guest with no display.
- **Debugging:** `isa-debugcon` for the KMD from M0; a device-side log per
  context from M2; QEMU's gdbstub with `dxgkrnl.pdb` / `ntoskrnl` symbols for
  bugchecks.

## Risks, most likely first

1. **`DRIVERCAPS` / `GPUMMUCAPS` answers.** Wrong values fail silently later.
   `dxgkrnl.sys` is symbolized in `re/ghidra-proj` — read its validation instead
   of guessing.
2. **Host-visible memory on NVIDIA.** Mapping exported driver memory into a
   KVM memslot is the historically fragile part of Venus-style designs. M2
   spike; fallback is guest-backed memory plus copies.
3. **`EnumVidPnCofuncModality`.** Under-populated mode sets fail without a
   diagnostic.
4. **TDR.** The 2-second watchdog trips on the first host stall.
   `QueryEngineStatus` must reflect worker progress, not ring occupancy.
5. **IRQL on the submit path.** Blocking at DISPATCH_LEVEL is a bugcheck under
   load, not a slowdown.
6. **Shared allocations.** DWM composites surfaces other processes rendered;
   device object identity must be global, not per-open. GPU-PV's
   `DXGKVMB_COMMAND_SHAREOBJECTWITHHOST` is the warning. **Answered by feature
   053**, with a table at each end: `SharedObjectTable`, one per adapter, mints
   the id into the allocation private driver data dxgkrnl carries between
   processes and refuses every record it did not write; `HostSharedObjects`,
   one per device on the host, holds one descriptor per id however many
   contexts name it and closes it when the last lets go. Neither is per-open.
7. **QEMU rebase cost.** Keep the patch to new files so each update is a
   rebuild, not a merge.
8. **In-process decoding.** Accepted trade-off; mitigated by bounds-checked,
   fuzzed decoders.
