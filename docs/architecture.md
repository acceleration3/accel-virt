# Architecture and code map

Where each part of accel-virt lives, which binary it ends up in, and which
directories may include which. If you know the feature or subsystem you want to
change, this page should tell you the directory before you search.

## Binaries and the directories they are built from

| Binary | Runs in | Built from |
|---|---|---|
| `avgpu.sys` | guest kernel | `src/miniport/` + `src/core/` + the guest-visible half of `src/proto/`, through the shim `src/km/` |
| `avumd.dll` | guest user mode | `src/icd/d3d/` + `src/icd/common/`, through `src/um/d3d/` and `src/um/common/` |
| `avvk.dll`, `avgl.dll`, `avcl.dll` | guest user mode | `src/icd/<api>/` + `src/icd/common/`, through `src/um/<api>/` and `src/um/common/` |
| `avtest.exe`, `av*probe.exe`, `avinspect.exe` | guest user mode | `src/guest/suite/`, `src/guest/probe/`, `src/guest/inspect/`, through `src/um/avtest/`, `src/um/probe/`, `src/um/inspect/` |
| the virtio-gpu-av QEMU module | host, inside QEMU | `src/device/` (the `av_device` archive) + `src/device/qemu/` (C glue) |
| `libav_executor.so` | host, loaded by QEMU or av-renderer | `src/executor/`, entry point `src/executor/shim/executor_ops.cpp` |
| `av-renderer` | host, own process | `src/device/renderer/` + the device core, entry point `src/device/shim/renderer_main.cpp` |
| `avrecord` | host tool | `src/device/replay/` + `src/device/recording/`, entry point `src/device/shim/recording_tool.cpp` |

Every guest user-mode binary also links `src/guest/kmt/` (the D3DKMT contract,
AvgpuAdapters and AdapterSearch) and `src/guest/transport/` (the av transport
over it). Every binary links `src/core/`.

A guest has one avgpu adapter per host GPU. `AvgpuAdapters` is the one list of
them every user-mode driver builds its devices from - the primary display's
first, then the kernel's order - so every API shows the same GPUs in the same
order: the Direct3D runtimes open each adapter themselves (`AdapterSearch` by
LUID), the OpenCL ICD shows a device per adapter (`ClDevices`), the Vulkan ICD
a physical device per adapter through an instance per adapter
(`InstanceSpan`), and the vendor libraries describe each. OpenGL, which has no
device selection, runs on the first.

## Layers

```
src/core/       foundation: every binary links it
   ^
src/proto/      the wire and the UM<->KM records; depends only on core
   ^
src/miniport/   src/device/   src/executor/   src/guest/   src/icd/
   ^               ^              ^               ^            ^
src/km/         src/device/qemu, src/*/shim/      src/um/
```

- `src/core` includes nothing outside itself.
- `src/proto` includes only `src/core` (its generated ICD-side files are the
  exception, see "Known exceptions").
- `src/miniport` is only for avgpu.sys: nothing outside `src/km` and the tests
  includes it.
- `src/executor` may include `src/device/abi` (the ABI it implements),
  `src/device/host` and `src/device/shim` (process services). Nothing in
  `src/device` includes `src/executor`.
- `src/icd` may include `src/guest/kmt` and `src/guest/transport`, never
  `src/guest/suite` or `src/guest/probe`.
- The platform shims (`src/km`, `src/um`, `src/device/qemu`, every `shim/`)
  sit on top of the portable code they translate for, and nothing portable
  includes them.

## src/core - the foundation

| Directory | Holds |
|---|---|
| `support/` | `Status`, `NtStatus`, `ObjectPool`/`ObjectStorage`, `GrowableArray`, `KeyedSlots` and `ClaimedRows` (storage for counts an application controls, see "Counts with no fixed bound"), `ScopeExit`, `Alignment`, `ByteRange`, `TextBuffer`, `RuntimeCall`, `SettingText` (how every knob's text is judged), `Fnv1aDigest`, `AV_UNTESTABLE` |
| `trace/` | `AV_SCOPE`, trace tables, event rings, crash and state dumps; `ReportGate`, how a state dump takes its root's `ThreadLock` without ever waiting on it |
| `log/` | `DriverLog`, `KeyedLog`, `LogLine`, `LogSink`, `LogQueue`: the logging the host, the guest drivers and the kernel's debug port share |
| `platform/` | the `Platform` contract the miniport runs on (implemented by `src/km` and `MockPlatform`), locks and their guards, lock ranks, `LogBudget`, `InvariantScope`; also `BlockSource` and `LockMarks`, which every binary uses, and `ThreadLock` with `ThreadLockHold`, the one lock contract for a binary's threads (the guest drivers' is `SrwLock`, the host binaries' `PosixLock`), in core so the device and the executor take the same one |

## src/miniport - avgpu.sys's logic

Portable and freestanding; `src/km` is its Windows kernel shim.

| Directory | Holds |
|---|---|
| `driver/` | the driver image and root, the DDI table, dxgkrnl's callbacks, interrupt notification |
| `ddi/` | the DDI records (dxgkrnl-owned memory, layout-proven in `src/km/ddi_record_layout.cpp`) and the `DxgkDdi*` entry points |
| `adapter/` | the `Adapter`, its capabilities, power, debug report, device identity and the capset exchange |
| `av_context/` | av contexts as the driver drives them: setup, ring submission, fence observation, context memory, and the host window chunks a context's process holds (`ContextChunk`) with their host blob ids (`ChunkIds`) |
| `memory/` | segments, allocations, GPU virtual addresses, page tables, paging, the host window, shared objects |
| `scheduling/` | devices, contexts, the execution node, hardware queues, doorbells, native fences |
| `display/` | modes, EDID, scanout sources, synthetic vsync, the hardware cursor |
| `vidpn/` | VidPn topology, mode sets, cofunctional modality, commit |
| `present/` | `DdiPresent`: blits, the copier, present queue and tickets, addressing |
| `pci/` | the PCI resource list, BAR addresses, the mapped register window |
| `virtio/` | the virtio-pci transport and its BAR mappings, virtqueues, the virtio-gpu device |

## src/proto - what crosses a boundary

| Directory | Crosses | Holds |
|---|---|---|
| `wire/` | - | `WireReader`, `WireWriter`, `WireArena`: the codecs everything below uses; `GuestBytes`, the blob arena every API's generated decoder resolves a command's offsets in |
| `client/` | guest driver -> device | the contract generated guest code is written against: `ClientTransport`, `ClientBlobArena`, `SyncCallScope`, the ICD hooks |
| `context/` | guest <-> device | context setup, capset info, GPU identity, device UUID, protocol version, sub-API, and the most av devices a VM has (`most_av_devices`, which bounds the guest's adapters) |
| `submission/` | guest <-> device | rings, batches, command and reply headers, fence pages, blob imports, shared-object requests |
| `resources/` | guest <-> device | allocation descriptors, shared-object records, surface and texel layouts, guest page lists, host memory requests (`HostMemoryRequest`) and host blob ids (`HostBlobId`) |
| `dma/` | miniport -> device | the commands in DMA buffers: present and paging |
| `pages/` | device -> miniport | pages the device writes: the clock page, vblank records, present progress |
| `virtio_gpu/` | miniport <-> device | virtio-gpu control-queue requests and responses, and av's own |
| `escape/` | guest UM <-> guest KM | escapes and private data the host never sees: av context records, context views, tail publish, UMD adapter info, host-window, follow, present-point and trace-escape records |
| `renderer/` | QEMU <-> av-renderer | the renderer process's messages |
| `export/` | QEMU <-> a host process | the export socket's requests and answers (docs/external-export.md) |
| `d3d/`, `vulkan/` | - | DXGI/D3D9 format layouts; descriptor-template and exportable-image layouts |
| `generated/<api>/` | - | written by `tools/codegen`, never by hand |

## src/device - the QEMU device

| Directory | Holds |
|---|---|
| `abi/` | `av_device.h` and `av_executor.h`, the C ABIs, and their C entry points |
| `host/` | the `Host` contract - also the `BlockSource` the device core's tables grow in - `TableHost` (QEMU's implementation), `EnvSetting`, `StdioLog` |
| `module/` | the composition root: `VirtioGpuAv`, `DeviceModule`, `DeviceInstance` |
| `gpu/` | binding to a host GPU, its identity, the capset the guest is offered |
| `context/` | `DeviceContext`, the context table, its ring consumer, the local and remote context planes |
| `threading/` | per-context `Worker`, `Doorbell`, `WorkItem` |
| `fence/` | fence pages, the fence reporter, the device interrupt line |
| `memory/` | guest blobs and their page mappings (`BlobTable`); host blobs (`HostBlobTable`, `HostBlob`, `HostBlobOwners`), executor memory placed in the hostmem region (docs/host-memory.md) |
| `control/` | answers to virtio-gpu control commands |
| `display/` | heads, modes, scanout, the display clock |
| `present/` | the present engine, queued presents, region moves |
| `execution/` | the device's side of `av_executor.h`: loading the executor library, the device's domain in it, and running batches - shared-object commands included, which it routes to the library - through it |
| `export/` | the export socket's connections, their threads and revocations (docs/external-export.md) |
| `isolation/` | the device's side of av-renderer: `RendererContextPlane`, `RemoteContext`, `RendererLink` |
| `renderer/` | av-renderer's process service, and which context each host blob was made for there (`RendererHostBlobs`) |
| `recording/` | the wire recorder and the recording file format |
| `replay/` | avrecord: reading, filtering, printing and replaying recordings |
| `feasibility/` | the capset's feature mask as realize decides it, and the interrupt stress probe |
| `shim/` | the POSIX edge of the host binaries: trace and state services, the renderer channel, module loading, the avrecord and av-renderer entry points; `PosixLock`, the `ThreadLock` every lock of the device module, av-renderer and the executor library is, and `PosixCondition` over it |
| `qemu/` | the C glue and build patch installed into QEMU's tree, translation only |

## src/executor - libav_executor.so

One `ContextSession` per guest context runs its API's commands on the host GPU.

| Directory | Holds |
|---|---|
| (root) | `ExecutorRoot`, `ContextSession`, `SessionBackend`, `ExecutorDomains`, host GPUs and APIs; `HostObjectRegistry`, the host objects one session's guest may name - by kind, with what each was made through and the references held on it - which every backend's commands are judged by and its shut-down releases; `DeviceTenants`, the sessions on one shared host device, woken when it is lost, and `HostDeviceRenewal`, which declares such a device lost and has it made again (`RenewableDevice`: Direct3D 11's and 12's); host memory (docs/host-memory.md): each GPU's `HostMemorySource`, its allocations (`SourceMemory`), the `MemoryExporter` contract it allocates through and what that answers (`ExportedMemory`); blob imports (`ImportedBlob`, `ImportTable`, which every backend finds the import a command names in) and `GuestRegion`, the import a Direct3D, OpenCL or OpenGL session adopts as its arena, and `PublishedSlots`, what the host publishes into that arena's tail for the guest to read without asking; `OwnedFd`, a descriptor of a host object - a dma-buf, an opaque memory or semaphore fd - held until an import or a caller takes it; the GPU clock hold; `PollBudget` (every bounded poll) on `HostClock`/`MonotonicClock`; `WakeSignal` and `EventfdWake`, which raises it when a Direct3D fence writes its eventfd |
| `sharing/` | `HostShares`, every shared object of one GPU of every API by (domain, id), its `Share`s, their holders and index, and `SessionShares`, what one session reaches it through (docs/shared-surfaces.md §0) |
| `vulkan/`, `opengl/`, `opencl/` | each API's backend and its tables |
| `d3d/` | Direct3D 9, 11 and 12 sessions, DXBC, the present surfaces and frame pools; `WatchedBlocks`, the source a session's tables grow in, which tells a refusal for want of memory apart so the device is removed with `E_OUTOFMEMORY`; `FormatCapsRows`, every DXGI format as a host device answered it, which Direct3D 11's and 12's caps queries hand the guest in one reply |
| `shim/` | the host runtimes the executor loads: each GPU's compositing context (`CompositorDevice`, docs/compositor.md) and the memory it allocates for the memory source (`CompositorMemory`), every Vulkan device's entry points (`VulkanDeviceTable`, whose `import_memory` is the one memory import every image, buffer and guest allocation goes through), Vulkan memory imports onto a guest's device (`VulkanMemoryImport`), images over exported memory (`VulkanImage`) and how one is described to an importer (`ExportedImageLayout`), EGL/GL, OpenCL, DXVK and vkd3d-proton (`ExportableImages`, the Vulkan memory under shared Direct3D 11 textures), the heap (`HostHeap`); and `executor_ops.cpp`, the library's entry point |

## src/guest, src/icd, src/um - the guest's user mode

| Directory | Holds |
|---|---|
| `guest/kmt/` | the `Kmt` contract (D3DKMT), its small value types, `AvgpuAdapters` (every avgpu adapter, the primary display's first), `AdapterSearch` (one of them, held open) and avgpu's PCI identity |
| `guest/transport/` | the av transport over Kmt - the one place a driver's commands are held for a batch (`KmtTransport::hold_in`, framed in a `FramedBlock`), so every command goes in the order it was made and a blob lease is retired only once the batch carrying its command has run - and host memory as a process holds it: `HostAllocation` (a host window chunk and its import, docs/host-memory.md) and the arenas built on it |
| `guest/suite/` | avtest's scenarios, and the step and report framework the probes use too |
| `guest/probe/` | the probe programs' portable halves: Vulkan, GL, CL and D3D work, the `*_entry` seams, shaders |
| `guest/inspect/` | avinspect, over its `InspectHost` seam |
| `icd/common/` | what two or more user-mode drivers share: transports and the host channel over them (`HostChannel`, which keeps every host allocation it opened - by its handle, for its owner, a Vulkan device or a session - and owns the channel's send lock and its publish barrier, and says when its context is lost and why), waits, locks, handle tables, connections and their generations (`LinkGenerations`: the device or session new users join, the lost ones left to theirs), device loss (`LossLatch`, the first fault code a context was lost to), window seams, what Windows says of an adapter (`AdapterFacts`) and the memory its budget leaves (`AdapterMemory`), the reportable-extension table every Khronos ICD's policy answers from (`ExtensionRows`), the present thread OpenGL's swaps and Vulkan's presents hand their frames to (`WindowPresentQueue`, woken by a `ThreadWake`), and the bookkeeping of CPU maps: a live map's private copy or the memory it names (`StagedMapping`), a context's open maps by object and subresource or by pointer (`OpenMappings`), and the merged runs of a copy an unmap sends back (`WrittenRuns`) |
| `icd/vulkan/`, `icd/opengl/`, `icd/opencl/` | each ICD's logic |
| `icd/d3d/` | the Direct3D user-mode driver's portable logic; `ddi/` mirrors the runtimes' structures |
| `icd/vendor/` | the ADL and NVAPI libraries' portable logic (docs/vendor-libraries.md); `ddi/` mirrors their structures |
| `um/common/` | the Windows halves of `icd/common`'s seams: file log, lazy driver, process heap, SRW lock, window chains; the process's `AV_` settings (`GuestSetting`), its performance counter (`PerformanceClock`) and `C:\ProgramData\av` (`DiagnosticsDirectory`) |
| `um/vulkan/`, `um/opengl/`, `um/opencl/` | each ICD's exports and library root |
| `um/d3d/` | avumd's exports, devices and DDI thunks (see "Known exceptions") |
| `um/vendor/` | the ADL and NVAPI libraries' exports, lazy root and record layout proofs |
| `um/kmt/` | `GdiKmt`, the Kmt over gdi32's D3DKMT calls |
| `um/avtest/`, `um/inspect/`, `um/probe/{vk,gl,cl,d3d}/` | the programs' Windows entry points |
| `um/trace/` | the Windows trace hosts and crash handling |
| `um/imports/`, `um/opencl_loader/`, `um/sample/` | import-library definitions; the Khronos loader's C runtime; a standalone D3D11 program |

## Tests, tools, docs

- `tests/unit/` mirrors `src/`: the test of `src/a/b/c.hpp` is
  `tests/unit/a/b/test_c.cpp`.
- `tests/harness/` holds the fakes, named after what they stand in for:
  `platform/` (MockPlatform), `dxgkrnl/`, `vidmm/`, `vidsch/`, `vidpn/`,
  `virtio/` (the virtio-gpu PCI function the miniport drives), `host/`
  (MockHost), `executor/`, `icd/`, `guest/` (MockKmt), `context/`, ...
- `tests/gpu_fault/`: `VK_LAYER_AV_device_loss`, a Vulkan layer that loses a
  host device on request, which the tests that lose one and a guest run with
  `--device-loss` load, and nothing else does.
- `tests/fuzz/`, `tests/corpus/`, `tests/loopback/`, `tests/qemu/`,
  `tests/conformance/`: the layers in `docs/testing.md`.
- `tools/`: gates at the root (`check_*.py`, `scope_marks.py`, `gate.sh`),
  `codegen/`, `guest/` (the VM runner), `host/`, `etw/`, `mcp/`, `re/`.

## Counts with no fixed bound

A count the application or the guest workload decides - objects, handles,
syncs, fences, queries, mapped ranges, imports, shared objects, descriptors -
has no fixed maximum anywhere in the stack. Its storage grows:

- `GrowableArray<T>` (`src/core/support/growable_array.hpp`): a contiguous run
  of trivially copyable values, doubled when it fills - handle lists, sorted
  range tables.
- `KeyedSlots<Key, Entry>` (`src/core/support/keyed_slots.hpp`): the storage
  under every keyed table, host and guest. Each entry has a slot in a chunk
  that never moves, so its address and slot number hold until it is erased
  and a side table can be kept by slot; erased slots are reused. A key - any
  value, 0 and all-ones included - finds its slot through an open-addressed
  index at most half full, doubling as it fills, whose erase shifts the probe
  back rather than leaving a tombstone, so a guest that makes and erases keys
  for ever never lengthens a probe.
- `ClaimedRows<Row>` (`src/core/support/claimed_rows.hpp`): rows appended and
  read without a lock - what a driver remembers from inside its own locks,
  `KeyedLog`'s keys - in chunks that never move, since a lock-free reader
  cannot follow a realloc.

All three allocate through a `BlockSource` with an explicit `Pool`. A growth that
fails is `Status::no_memory`, which each boundary answers as its own
out-of-memory error - `E_OUTOFMEMORY`, `VK_ERROR_OUT_OF_HOST_MEMORY`,
`GL_OUT_OF_MEMORY`, `CL_OUT_OF_HOST_MEMORY`, `STATUS_NO_MEMORY` - never a
silent refusal, a context fault or a lost device.

What stays fixed is a bound set by a specification, the hardware or the wire
(eight render targets, fifteen GL context attributes, the 64 doorbell pages of
BAR 0 and so the 64 contexts a device runs, a field's width), and what the
kernel driver must have in hand before DISPATCH, where it cannot allocate.

A guest's Vulkan and OpenCL commands that hand back new handles ask their
backend for room for all of them before the host driver is called
(`ServerDispatch::room_for` in both APIs, written by `tools/codegen`), so a
table that cannot grow answers the API's out-of-memory error with nothing made
on the host.

Host safety against a hostile guest is a byte budget, not a count per table.
No accounting exists yet. Where it goes: every container a session grows
allocates through a heap that session owns - `ContextSession`'s `HostHeap`,
each Vulkan, OpenCL and OpenGL backend's own, and a Direct3D session's
`WatchedBlocks` - and the device core's through its `Host`. A budget charged
there - bytes handed out against a per-session or per-VM ceiling, a refusal
past it answered as the API's out-of-memory error - bounds everything a guest
can make the host hold for it, whatever the counts.

## Where new code goes

1. Find the binary it belongs to in the first table, then the subsystem
   directory in that binary's section. Code lives with the subsystem that
   owns it, not with the code that happens to call it.
2. A class used by one subsystem lives in that subsystem's directory, even if
   it looks generic. It moves to `src/core` or `icd/common` only when a second
   owner actually uses it.
3. Decisions go in portable code; a `shim/` (or `src/km`, `src/um`,
   `src/device/qemu`) only translates. The coverage gate exempts any directory
   named `shim`, so never name a directory `shim` that holds decisions.
4. Do not add a directory for one small class; put it beside its owner.
5. Do not add `utils`, `helpers`, `misc`, `common` or `shared` directories.
   `icd/common` and `um/common` exist because four drivers really share them;
   anything in them must have at least two users among those drivers.
6. A new top-level directory under `src/` means a new binary or a new layer.
   Check this page first.

## Paths the build and the gates name

Moving a directory means updating these as well as the includes:

- `CMakeLists.txt`: the portable glob, the `av_device` sources, the three entry
  files, the generated file lists, `av_d3d_umd`'s named `icd/common` files,
  and the test link rules on `tests/unit/icd/<api>/`.
- `src/km/CMakeLists.txt` and `src/um/CMakeLists.txt`: globs written relative
  to their own directory (`../miniport/*.cpp`, `../core/support/*.cpp`, ...).
- `tools/header_scan.py` (`SOURCE_DIRECTORIES`, `RECORD_DIRECTORIES`),
  `tools/check_coverage.py`, `tools/check_design.py`,
  `tools/check_symbol_tests.py`, `tools/scope_marks.py`,
  `tools/check_extension_policy.py`, `tools/gen_ddi_slots.py`,
  `tools/gen_d3d*_ddi.py`, `tools/gen_spirv.py`, `tools/build_qemu.sh`,
  `tools/generated_sources.txt` and the codegen emitters under
  `tools/codegen/av_codegen/`.

## Known exceptions

- `src/proto/generated/` holds the ICD-side generated files
  (`vk_icd_entries`, `gl_icd_entries`, `gl_icd_table.h`, `cl_icd_stubs.h`)
  next to the wire, because each generator writes one tree per API. They
  include `src/icd` headers, and `proto/client/icd_connection.hpp` includes
  `icd/vulkan/connection.hpp` for them.
- `src/um/d3d` holds most of the Direct3D device logic, because it names WDK
  DDI types that `src/icd/d3d/ddi` does not mirror. It is not unit tested
  (constitution II asks for portable halves under test).
- `src/icd/common/frame_pacer.cpp` reads `miniport/display/display_mode.hpp`
  and `src/guest/transport/guest_doorbell.hpp` reads a `miniport/ddi` record:
  two miniport types the guest's user mode shares.
- `src/guest/probe/loader_vulkan.cpp` reads
  `icd/vulkan/win32_surface_create_record.hpp`, the Win32 surface record the
  Vulkan ICD defines.
