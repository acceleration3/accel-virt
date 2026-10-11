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

Every guest user-mode binary also links `src/guest/kmt/` (the D3DKMT contract
and AdapterSearch) and `src/guest/transport/` (the av transport over it). Every
binary links `src/core/`.

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
| `support/` | `Status`, `NtStatus`, `ObjectPool`/`ObjectStorage`, `ScopeExit`, `Alignment`, `ByteRange`, `TextBuffer`, `RuntimeCall`, `AV_UNTESTABLE` |
| `trace/` | `AV_SCOPE`, trace tables, event rings, crash and state dumps |
| `log/` | `DriverLog`, `KeyedLog`, `LogLine`, `LogSink`, `LogQueue`: the logging the host, the guest drivers and the kernel's debug port share |
| `platform/` | the `Platform` contract the miniport runs on (implemented by `src/km` and `MockPlatform`), locks and their guards, lock ranks, `LogBudget`, `InvariantScope`; also `BlockSource` and `LockMarks`, which every binary uses |

## src/miniport - avgpu.sys's logic

Portable and freestanding; `src/km` is its Windows kernel shim.

| Directory | Holds |
|---|---|
| `driver/` | the driver image and root, the DDI table, dxgkrnl's callbacks, interrupt notification |
| `ddi/` | the DDI records (dxgkrnl-owned memory, layout-proven in `src/km/ddi_record_layout.cpp`) and the `DxgkDdi*` entry points |
| `adapter/` | the `Adapter`, its capabilities, power, debug report, device identity and the capset exchange |
| `av_context/` | av contexts as the driver drives them: setup, ring submission, fence observation, context memory |
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
| `wire/` | - | `WireReader`, `WireWriter`, `WireArena`: the codecs everything below uses |
| `client/` | guest driver -> device | the contract generated guest code is written against: `ClientTransport`, `ClientBlobArena`, `SyncCallScope`, the ICD hooks |
| `context/` | guest <-> device | context setup, capset info, GPU identity, device UUID, protocol version, sub-API |
| `submission/` | guest <-> device | rings, batches, command and reply headers, fence pages, blob imports, shared-object requests |
| `resources/` | guest <-> device | allocation descriptors, shared-object records, surface and texel layouts, guest page lists |
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
| `host/` | the `Host` contract, `TableHost` (QEMU's implementation), `EnvSetting`, `StdioLog` |
| `module/` | the composition root: `VirtioGpuAv`, `DeviceModule`, `DeviceInstance` |
| `gpu/` | binding to a host GPU, its identity, the capset the guest is offered |
| `context/` | `DeviceContext`, the context table, its ring consumer, the local and remote context planes |
| `threading/` | per-context `Worker`, `Doorbell`, `WorkItem` |
| `fence/` | fence pages, the fence reporter, the device interrupt line |
| `memory/` | blobs, guest page mappings, hostmem |
| `control/` | answers to virtio-gpu control commands |
| `display/` | heads, modes, scanout, the display clock |
| `present/` | the present engine, queued presents, region moves |
| `execution/` | the device's side of `av_executor.h`: loading the executor library, the device's domain in it, and running batches - shared-object commands included, which it routes to the library - through it |
| `export/` | the export socket's connections, their threads and revocations (docs/external-export.md) |
| `isolation/` | the device's side of av-renderer: `RendererContextPlane`, `RemoteContext`, `RendererLink` |
| `renderer/` | av-renderer's process service |
| `recording/` | the wire recorder and the recording file format |
| `replay/` | avrecord: reading, filtering, printing and replaying recordings |
| `feasibility/` | the probes a device runs at realize time |
| `shim/` | the POSIX edge of the host binaries: trace and state services, the renderer channel, module loading, the avrecord and av-renderer entry points |
| `qemu/` | the C glue and build patch installed into QEMU's tree, translation only |

## src/executor - libav_executor.so

One `ContextSession` per guest context runs its API's commands on the host GPU.

| Directory | Holds |
|---|---|
| (root) | `ExecutorRoot`, `ContextSession`, `SessionBackend`, `ExecutorDomains`, host GPUs and APIs, blob imports, guest regions, the GPU clock hold, `PollBudget`, `WakeSignal` |
| `sharing/` | `HostShares`, every shared object of one GPU of every API by (domain, id), its `Share`s, their holders and index, and `SessionShares`, what one session reaches it through (docs/shared-surfaces.md §0) |
| `vulkan/`, `opengl/`, `opencl/` | each API's backend and its tables |
| `d3d/` | Direct3D 9, 11 and 12 sessions, DXBC, the present surfaces and frame pools |
| `shim/` | the host runtimes the executor loads: Vulkan imports, EGL/GL, OpenCL, DXVK and vkd3d-proton (`ExportableImages`, the Vulkan memory under shared Direct3D 11 textures), dma-buf, the heap (`HostHeap`); and `executor_ops.cpp`, the library's entry point |

## src/guest, src/icd, src/um - the guest's user mode

| Directory | Holds |
|---|---|
| `guest/kmt/` | the `Kmt` contract (D3DKMT), its small value types, `AdapterSearch` and avgpu's PCI identity |
| `guest/transport/` | the av transport over Kmt |
| `guest/suite/` | avtest's scenarios, and the step and report framework the probes use too |
| `guest/probe/` | the probe programs' portable halves: Vulkan, GL, CL and D3D work, the `*_entry` seams, shaders |
| `guest/inspect/` | avinspect, over its `InspectHost` seam |
| `icd/common/` | what two or more user-mode drivers share: transports, waits, locks, handle tables, connections, window seams |
| `icd/vulkan/`, `icd/opengl/`, `icd/opencl/` | each ICD's logic |
| `icd/d3d/` | the Direct3D user-mode driver's portable logic; `ddi/` mirrors the runtimes' structures |
| `um/common/` | the Windows halves of `icd/common`'s seams: file log, lazy driver, process heap, SRW lock, window chains |
| `um/vulkan/`, `um/opengl/`, `um/opencl/` | each ICD's exports and library root |
| `um/d3d/` | avumd's exports, devices and DDI thunks (see "Known exceptions") |
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
- `tests/fuzz/`, `tests/corpus/`, `tests/loopback/`, `tests/qemu/`,
  `tests/conformance/`: the layers in `docs/testing.md`.
- `tools/`: gates at the root (`check_*.py`, `scope_marks.py`, `gate.sh`),
  `codegen/`, `guest/` (the VM runner), `host/`, `etw/`, `mcp/`, `re/`.

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
