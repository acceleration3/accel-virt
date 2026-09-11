# Feature Specification: QEMU device: skeleton, object model and threading

**Feature Branch**: `026-device-skeleton-and-threading`

**Created**: 2026-09-10

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 026 · **Status**: `implement` · **Depends on**: 004

virtio-gpu-rvk-pci as a VirtIOGPU subclass in a patched QEMU package, one
instance per host GPU bound by Vulkan device UUID (host-gpu property), and
tools/host/list_gpus to discover host GPUs and generate the per-machine QEMU
or libvirt configuration. The threading contract: no decode or execution on
the main loop, one worker per context, virtqueue state only under the BQL.

### Status meaning

Build it, test it to the constitution's standard, and report the capabilities it enables.

### Design rules — binding on this feature

Constitution Principle III and [`CLAUDE.md`](../../CLAUDE.md). Restated here because it is checked at review, not assumed:

- **Every unit of behaviour is a class that owns state.** No static member functions. No stateless class used as a namespace. No free function outside the two ABI boundaries (`DxgkDdi*` entry points in `src/core/ddi/`, and `extern "C"` entry points), each one line with no logic.
- If functions thread the same handle, buffer, bounds or platform through their parameters, **that parameter is a member**. Make it a class and inject its dependencies at construction.
- Name the classes this feature adds before writing them, and say which kind each is: an **object class** (one instance per real thing) or a **system class** (a long-lived service owning a collection).
- Dependencies injected at construction. No global, no singleton, no file-scope state.
- Data members private, trailing underscore. Classes `PascalCase`, methods and locals `snake_case`. No `Manager`, `Helper`, `Utils`, `Handler`, `Impl`, `process()`, `handle()`.
- One class per header, named for it. A header declares; it does not define. One responsibility per class, statable without an "and".
- Errors are `av::Status` values. No exceptions, no RTTI, no C++ standard library, no `new`/`delete` (placement new only in `ObjectStorage`). Cleanup is RAII via `ScopeExit`, never repeated per branch.
- Fallible construction is two-phase: trivial constructor, `initialize(...)` returning `Status`.
- Every public method declares its IRQL ceiling via `platform_.require_irql_at_most(...)`.

### Classes this feature introduces

<!-- Fill this in during /speckit-plan, before writing code. One row per class; if a row has no state, it is not a class. -->

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `Host` | system contract | - | What the device core may ask of QEMU: threads, events, mutexes, the main loop, the host GPU catalog, logging |
| `TableHost` | object | the C host operations table the glue fills | Implements `Host` over the glue's QEMU calls |
| `DeviceUuid` | object | 16 bytes | A Vulkan device UUID, parsed from its 36-character text |
| `GpuBinding` | object | host, the property text, the bound index | Binds a device to the host GPU the `host-gpu` property names |
| `TextBuffer` | object | destination, capacity, length | Builds one NUL-terminated message in caller memory |
| `WorkItem` | object | work routine, completion routine, context | One unit of work a worker runs, then completes on the main loop |
| `Worker` | object | host, thread, wake event, mutex, queue of work items, stop flag | Runs one context's work off the main loop |
| `DeviceContext` | object | context id, its `Worker` | One guest API context on the device |
| `DeviceContextTable` | system | host, fixed slots of `DeviceContext` | Creates and destroys a device's contexts |
| `RvkDevice` | object | host, `GpuBinding`, `DeviceContextTable`, realized flag | One `virtio-gpu-rvk` instance |
| `DeviceInstance` | object | its `TableHost`, its `RvkDevice` | Pairs a device with the host operations it was created over |
| `DeviceModule` | system | fixed slots of `DeviceInstance` | Holds every device the loaded QEMU module created |
| `MockHost` (harness) | object | threads, events, mutexes, main loop queue, GPU catalog, violations, failure injection | The adversarial `Host` for unit tests |

### Test requirements

Constitution Principle III applies in full. Concretely, this feature is not done until:

- every branch of every function it adds to `src/core`, `src/device` or `src/proto` is covered, or marked `AV_UNTESTABLE` with a justification;
- each stateful module it adds exports an invariant function that tests call after **every** mutation;
- every function it adds that reads bytes from outside its own address space has a hostile-input table test and a libFuzzer target with a committed corpus;
- the allocation-failure sweep passes: with each allocation site failed in turn, no crash and no leak;
- IRQL annotations are present and the mock's IRQL checker is satisfied;
- `tools/check_symbol_tests.py` reports no untested exported symbol.

### Reference material

Knowledge base:

- [`docs/kb/11-implementation-plan.md`](../../docs/kb/11-implementation-plan.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## Clarifications

### Session 2026-09-11

- Q (agent, verified by building): How is the device built into QEMU? -> A: As
  two QEMU modules in a patched QEMU 11.1.1 (sha256
  `079ffbff8a7111bbc89022107cbabf3bbfd614d5fc9d7cc675991196aca12482`, the tarball
  Arch's `qemu 11.1.1-1` builds): `hw-display-virtio-gpu-rvk` (type
  `virtio-gpu-rvk-device`, a `TYPE_VIRTIO_GPU` subclass) and
  `hw-display-virtio-gpu-pci-rvk` (type `virtio-gpu-rvk-pci`). The patch adds a
  `rvk` meson feature, a `rvk_device` pkg-config dependency and the two module
  source sets beside rutabaga's; no Kconfig symbol is needed. QEMU's meson builds
  no C++ in modules and `modinfo-collect.py` reads only `.c`, so the glue is C
  (`src/device/qemu/`, copied into `hw/display/`) and the core is this
  repository's `librvk_device.a` behind a C ABI, found through `rvk_device.pc`.
- Q (agent, verified): Can the module load into the distro binary? -> A: The
  build stamp hashes only the version and `configure`, so it matches; but the
  binary's compiled-in module table has no entry for the new types, so
  `qemu-system-x86_64` itself is rebuilt (`tools/build_qemu.sh`, about a minute
  for `x86_64-softmmu`). The kb's claim that the stamp cannot match is corrected.
- Q (agent): Threading contract? -> A: No decode or execution on the main loop:
  each context owns a `Worker` thread created through QEMU
  (`qemu_thread_create` blocks signals a bare thread would take); work runs on
  it and completes on the main loop through `aio_bh_schedule_oneshot`, which is
  also how a completion reaches virtqueue state, touched only under the BQL.
  `Host::require_main_loop` and `require_worker` state the contract in every
  method; the mock records violations. Interrupts from workers go through the
  virtqueue guest notifiers virtio-pci already routes (feature 031), not a
  separate irqfd.
- Q (agent): Binding? -> A: `host-gpu` is a string property: `auto` binds the
  first Vulkan physical device, a UUID the one whose
  `VkPhysicalDeviceIDProperties::deviceUUID` matches; anything else, no match,
  or no device fails realize with a named error. The glue enumerates the
  catalog with the Vulkan loader; matching is core.
- Q (agent): Where does a device live? -> A: QEMU's C glue cannot hold a C++
  object, so the loaded module's `DeviceModule` holds up to 16 devices (one per
  host GPU) in the ABI translation unit's static storage, the device-side
  counterpart of the kernel shim's static root; the constitution records it.
- Q (agent): What does the device claim? -> A: Nothing beyond stock virtio-gpu:
  no blob, context-init or 3D feature bit until the features implementing them
  (028, 029). Contexts and workers exist in core and are driven by tests until
  CTX_CREATE arrives in 029.
- Q (agent): Discovery? -> A: `tools/host/list_gpus.py` reads `vulkaninfo` and
  prints the GPUs, one `-device virtio-gpu-rvk-pci,host-gpu=<uuid>` per GPU, or a
  libvirt fragment.

## User Scenarios & Testing *(mandatory)*

Users: the host operator launching a VM; QEMU realizing, resetting and
finalizing the device; later features posting work to contexts.

### User Story 1 - One adapter per host GPU (Priority: P1)

1. **Given** a host with GPUs, **When** `list_gpus.py --qemu` runs, **Then** it
   prints one `virtio-gpu-rvk-pci,host-gpu=<uuid>` device per Vulkan device.
2. **Given** that argument, **When** QEMU starts, **Then** the guest sees a
   virtio-gpu PCI function (1af4:1050, subsystem 1af4:5256); `host-gpu=auto`
   works; a malformed or absent UUID fails with a message naming it.

### User Story 2 - Work off the main loop (Priority: P1)

1. **Given** a context, **When** work is posted from the main loop, **Then** it
   runs on the context's worker, never on the main loop, and its completion runs
   on the main loop in posting order.
2. A full queue, a context that does not exist, or a thread, event or mutex the
   host cannot create is refused and leaks nothing; destroying a context stops and
   joins its worker, dropping work not yet run; reset destroys every context.

## Requirements *(mandatory)*

- **FR-001**: The device core is freestanding C++23 with no QEMU or libc
  header; the glue has no branch QEMU's ABI does not mandate.
- **FR-002**: No virtio feature beyond stock virtio-gpu is advertised.

## Success Criteria *(mandatory)*

- **SC-001**: `tools/gate.sh` builds the patched QEMU against `librvk_device.a`
  and a qtest smoke run sees the PCI function realize and a bad `host-gpu` fail.
- **SC-002**: 100% branch coverage of `src/device`; the worker tests pass under
  ASan and UBSan with real threads.

## Out of Scope

- The 2D protocol behaviour (027, the base class), blobs and hostmem (028),
  capsets and CTX_CREATE (029), rings and doorbells (030), fences and interrupts
  (031), execution (032), faults (033), an Arch package.
