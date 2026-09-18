# Feature Specification: Vulkan: the feature set DXVK and vkd3d-proton need

**Feature Branch**: `047-vulkan-for-d3d-translation`

**Created**: 2026-09-11

**Status**: Draft

<!-- AUTOGEN:BEGIN -->

**Feature ID**: 047 · **Status**: `implement` · **Depends on**: 046

Every extension and feature DXVK (D3D8 to D3D11) and vkd3d-proton (D3D12)
require, forwarded when the host GPU has it and absent when it does not, a
32-bit ICD for WoW64 games, and Win32 external memory and semaphores for DXGI
interop - so D3D applications run on rvkgpu with the translation layers placed
beside them.

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
| `DescriptorTemplateLayout` (proto) | object | one template's entries: offset, stride, count and descriptor type each | Says how many bytes one update through this template reads |
| `DescriptorTemplateTable` (icd) | system | the templates a device made, by handle | Owns the layout a template's blob is sized by |
| `HostTemplateTable` (device) | system | the templates the guest made on this context | Refuses a blob length the guest's template does not account for |
| `PlacedMapping` (icd) | object | the address a caller chose, the chunk and the length behind it | Maps host-visible memory where `VkMemoryMapPlacedInfoEXT` says |
| `MemoryWin32HandleRecord` (icd) | object | the fields of `VkMemoryGetWin32HandleInfoKHR` | Reads an export request the vendored headers cannot declare |
| `SemaphoreWin32HandleRecord` (icd) | object | the fields of the two semaphore Win32 structures | Reads a semaphore export or import request |
| `ExternalObjectTable` (icd) | system | the handles this process exported, each naming a host object | Finds again the host object a handle was exported for |
| `LayerRequirements` (tools) | object | one pinned layer archive and the names it read out of it | Says which extensions a release requires and which it prefers |
| `D3dProbeReport` (guest suite) | object | the steps a D3D probe ran and their values | Prints what a D3D run proved, in the runner's grammar |
| `D3d11Work` / `D3d9Work` / `D3d12Work` (guest suite) | object | one device, its swap chain or render target, the frame it drew | Draws and reads back one reference frame through its own API |

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

- [`docs/kb/07-usermode.md`](../../docs/kb/07-usermode.md)

Normative headers: `third_party/windows-kit/include/` (fetched by `tools/fetch_windows_kit.sh`). Reverse-engineering targets, symbolized, in `re/ghidra-proj` (built by `tools/re/build_workspace.sh`) — read `dxgkrnl.sys` when the question is "what does the caller do with this", and `basicrender.sys` when it is "what is the minimum that works".

<!-- AUTOGEN:END -->

## User Scenarios & Testing *(mandatory)*

<!-- Written by /speckit-specify or by hand. The autogen block above is regenerated by tools/gen_specs.py; everything below is preserved. -->

Users: D3D8 through D3D12 applications and games in the guest running on
DXVK and vkd3d-proton placed beside them, which refuse to start or fall back
to software when a single required extension or feature is missing, and which
include many 32-bit titles.

### User Story 1 - DXVK finds everything it requires (Priority: P1)

**Independent Test**: with the pinned DXVK release, a D3D11 sample and a D3D9
sample start on rvkgpu with DXVK's device creation reporting no missing
required extension or feature; the list DXVK treats as required is read from
the pinned release at test time, not hand-copied into the test.

**Acceptance Scenarios**:

1. **Given** a host GPU that supports them, **When** DXVK creates its device,
   **Then** every extension it requires is present, including those in
   render-server's generated profile today: `VK_EXT_robustness2`,
   `VK_EXT_graphics_pipeline_library`, `VK_KHR_pipeline_library`,
   `VK_EXT_vertex_attribute_divisor`, `VK_EXT_depth_clip_enable`,
   `VK_EXT_transform_feedback`, `VK_EXT_custom_border_color`,
   `VK_EXT_4444_formats`, `VK_EXT_attachment_feedback_loop_layout`,
   `VK_EXT_memory_priority`, `VK_EXT_pageable_device_local_memory`,
   `VK_KHR_map_memory2` and `VK_KHR_get_surface_capabilities2`.
2. **Given** DXVK's optional extensions, **Then** each is reported exactly when
   the host GPU has it and the path implements it.
3. **Given** a D3D9 32-bit game, **When** it runs through 32-bit DXVK, **Then**
   the 32-bit ICD serves it with the same feature set as the 64-bit one.

### User Story 2 - vkd3d-proton finds everything it requires (Priority: P1)

**Independent Test**: with the pinned vkd3d-proton release, a D3D12 sample
creates a device at the feature level the host GPU supports; the test reads
vkd3d-proton's required list (at least Vulkan 1.3 with dynamic rendering,
synchronization2 and timeline semaphores, buffer device address, descriptor
indexing, mutable descriptor types, shader demote-to-helper, image view min
LOD and push descriptors) from the pinned release.

**Acceptance Scenarios**:

1. **Given** a host GPU meeting vkd3d-proton's requirements, **When** it
   creates a D3D12 device, **Then** device creation succeeds and the reported
   feature level equals what vkd3d-proton reports for the same GPU natively on
   the host.
2. **Given** a host GPU missing a vkd3d-proton requirement, **Then** the guest
   reports the same absence and vkd3d-proton fails device creation with its
   own diagnostic, as it would natively.
3. **Given** descriptor handling, **Then** `VK_EXT_descriptor_buffer` is
   reported only once implemented and tested; until then vkd3d-proton uses its
   descriptor-indexing path.

### User Story 3 - DXGI interop through Win32 external objects (Priority: P2)

**Independent Test**: a test shares a Vulkan image and a timeline semaphore
through `VK_KHR_external_memory_win32` and `VK_KHR_external_semaphore_win32`
between two devices in one process, reading the same pixels and waking on the
same fence. Sharing across processes is feature 053's, which extends these
extensions rather than adding new ones.

**Acceptance Scenarios**:

1. **Given** an exported memory handle, **When** another device in the same
   process imports it, **Then** both see the same host GPU memory; a handle
   opened by another process is refused until feature 053 provides the
   device-global object identity it needs.
2. **Given** a handle type the path does not implement, **Then** it is absent
   from `VkExternalMemoryProperties`.

### Edge Cases

- Host without an extension DXVK needs: absent in the guest, and DXVK reports
  the missing extension exactly as on the host.
- Device lost mid-frame in a D3D11 game: DXVK's device-removed path runs; the
  application sees `DXGI_ERROR_DEVICE_REMOVED`.
- Window minimize, resize and occlusion through DXGI swapchains on DXVK.
- Multi-monitor fullscreen switches in D3D9 and D3D11 titles.
- 32-bit WoW64 processes with large address awareness and without it.
- Adapter removal while a D3D12 device exists: `DXGI_ERROR_DEVICE_REMOVED`
  through vkd3d-proton.
- Memory budget exhaustion with pageable device-local memory: allocations
  succeed by eviction where the extension promises it, fail otherwise.
- A pipeline-library-heavy title compiling thousands of pipelines: no
  transport stall trips TDR.

## Clarifications

### Session 2026-09-18

- **Q: FR-004 and SC-001 name a 32-bit ICD, but `specs/061-driver-package/spec.md:204-208`
  already owns building and registering one, and `specs/045-vulkan-icd/research.md:252-255`
  deferred it there. Which feature builds it?** → 061 builds and registers
  `rvkvk32.dll`; 047 delivers what a 32-bit process needs *from the ICD's
  behaviour* - `VK_KHR_map_memory2` and `VK_EXT_map_memory_placed`, without
  which a 32-bit DXVK cannot map memory at all
  (`tools/codegen/profiles/vulkan.toml:141-174`) - and keeps the reported set
  independent of pointer width. The 32-bit halves of US1 scenario 3 and SC-001
  are 061's acceptance. `specs/041-kmd-host-visible-memory/tasks.md:79-107`
  records that placing a host-visible view below 4 GiB is not the miniport's to
  control, so it is not promised here either.
- **Q: how is a layer's required list "read from the pinned release at test
  time" without a network?** → the pin is a source archive with a SHA-256, like
  every other pin (`tools/fetch_vulkan_runtime.py`); `LayerRequirements` parses
  the archive on disk, and its own test drives it over a small archive the test
  writes, so the parser is proven without the release being present. A run
  whose pins are not fetched is refused, not skipped silently.
- **Q: `VK_KHR_push_descriptor` and `VK_KHR_map_memory2` are in the generator's
  profile but never reach its output. Which route fixes them?** → different
  ones. Descriptor templates get a generator hook that sizes their blob, the
  way `host_image_copy_bytes` is shaped, because the size is the template's
  property and both sides need it. `vkMapMemory2` joins `vkMapMemory` in
  `[commands].exclude` and is written as a local ICD entry, because a mapping's
  address is the guest's business and never crosses the wire.
- **Q: what does an exported Win32 handle mean when no host driver has the
  extension?** → within one process it names a host object the device already
  holds, and an import finds it again through `ExternalObjectTable`; the host
  is asked for the FD-based extensions it does have. A handle from another
  process is refused until feature 053 gives objects a device-global identity.

## Requirements *(mandatory)*

- **FR-001**: The required-extension lists of the pinned DXVK and vkd3d-proton
  releases are extracted from those releases by the test tooling; the versions
  are pinned in the repository.
- **FR-002**: Every extension and feature is reported only when the host GPU
  has it and this path implements and tests it; `VK_EXT_host_image_copy`
  remains withheld and `VK_EXT_descriptor_buffer` excluded until a spec change
  implements them.
- **FR-003**: `VK_EXT_map_memory_placed`, which the ICD provides itself, is
  implemented over host-visible allocations and tested in both 32- and 64-bit
  processes.
- **FR-004**: The 32-bit ICD reports the same feature set as the 64-bit ICD
  for the same adapter.
- **FR-005**: Win32 external memory and semaphore handles are NT handles backed
  by rvkgpu shared allocations and fences.
- **FR-006**: The guest ICD runs the DXVK and vkd3d-proton samples under a heap
  checker (page heap) with no corruption; the intermittent heap corruption
  seen on render-server's current transport is reproduced and fixed first.

## Success Criteria *(mandatory)*

- **SC-001**: DXVK D3D9 (32-bit) and D3D11 samples, and vkd3d-proton D3D12
  samples, start and render reference frames on every supported host GPU class.
- **SC-002**: Reported feature levels match the same GPU natively on the host.
- **SC-003**: One hour of a DXVK title under page heap produces no heap error.

## Assumptions

- DXVK and vkd3d-proton are placed beside applications; the Microsoft runtime
  path is features 055-057.
- Feature 053's shared-allocation object model backs the Win32 handles.

## Out of Scope

- Replacing the system D3D DLLs globally; D3D through the Microsoft runtime.
- DXVK-NVAPI, DLSS and vendor extensions.
- Conformance runs and the application suite (048).
