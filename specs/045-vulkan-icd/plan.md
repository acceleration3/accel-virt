# Implementation Plan: Vulkan - the ICD on rvkgpu

**Branch**: `045-vulkan-icd` | **Date**: 2026-09-15 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/045-vulkan-icd/spec.md`

## Summary

A freestanding `rvkvk.dll` the Windows Vulkan loader finds through the adapter's
`VulkanDriverName`, answering one physical device per rvkgpu adapter with the
adapter's LUID. Every command crosses 043's transport, one rvk context per
`VkInstance`, command buffers recording into their own batches. Host-visible
memory is guest RAM the host GPU imports with `VK_EXT_external_memory_host`, so
`vkMapMemory` is the process's own view with no copy; that needs a new allocation
kind in the KMD, an `IMPORT_BLOB` transport command and a contiguous host view in
the device, and the first thing built is a live experiment proving the import.
Waits are sliced under TdrDelay, device loss is `VK_ERROR_DEVICE_LOST`, and only
extensions this feature tests are reported. Research changed scope in three
places (research.md §11): the 32-bit ICD to 061, bulk data above 256 KiB to 050,
presentation to 046.

## Technical Context

**Language/Version**: C++23 freestanding (ICD core, KMD, device, executor);
Python 3 (tools, runner); GLSL compiled to committed SPIR-V.

**Primary Dependencies**: 043/044's transport (`GuestConnection`, `KmtTransport`,
`AdapterSearch`, `Kmt`); 063's generated Vulkan client and ICD wrappers; 042's
executor (`import_blob`, `VulkanBackend`); 041's allocation kinds and 043's
context binding in the KMD; the Khronos Vulkan runtime in the guest, fetched by pin.

**Storage**: N/A.

**Testing**: L1 over `MockKmt` and `MockIcdMemory` for every ICD class; KMD and
device unit tests and fuzz targets for the new record and command; L4 full-stack
rig with the real executor running the probe's scenarios through the ICD's entry
table; qtest for `IMPORT_BLOB`; L5 `run_guest_suite.py --vulkan` with the loader,
`vulkaninfo` and `rvkvkprobe.exe`.

**Target Platform**: Windows 11 guest (x64 user mode, no CRT); Linux host (QEMU).

**Project Type**: driver components (user-mode DLL, KMD, QEMU device, host library).

**Performance Goals**: `vkMapMemory` with no round trip and no copy; a recorded
command buffer costs one submission.

**Constraints**: no STL/CRT; no host call blocks a context worker beyond 500 ms
(TdrDelay 2 s); commands up to 256 KiB, replies up to 64 KiB; only gdi32, kernel32,
ntdll imports; the DLL loads, unloads and reloads cleanly; nothing reported that is
not tested (constitution VI).

**Scale/Scope**: one ICD DLL, ~12 ICD classes, one KMD allocation kind and record,
one device command and host view, executor import completion, a probe, runner and
INF changes.

## Constitution Check

| Principle | How this feature satisfies it |
|---|---|
| I. Specification precedes implementation | research.md resolves every open question and records the three moves; 061's spec carries the 32-bit ICD |
| II. Core portable, shim translation only | ICD decisions in `src/icd/vulkan`, portable and tested on Linux; `src/um/vulkan` holds only the two exports and the Windows seam implementations |
| III. Object-oriented, with state | every class below owns state; the ICD root is one `ObjectStorage` in the exports' translation unit - the loader's C entry has no `this`, the same exception as the executor root. CLAUDE.md and the constitution's list of process-lifetime state gain this one line (PATCH amendment) |
| IV. Test-first and exhaustive | each class over MockKmt/MockIcdMemory at 100% branches; hostile replies to the ICD's own parsing; fuzz targets for the KMD record and `IMPORT_BLOB`; invariants after every mutation |
| V. Everything from outside is hostile | the KMD validates the binding record and page ownership; the device validates `IMPORT_BLOB` ranges against the blob it owns; the executor imports only what the device handed it |
| VI. Feature levels are phases | only tested extensions reported; WSI withheld until 046 |
| VII. IRQL and locking | the KMD binding runs at PASSIVE like 043's; ICD locking is user mode, recorded order checked in tests |
| VIII. No speculative structure | `IcdMemory`, `Lock` and `LogSink` each have the Windows implementation and the test one |
| IX. Reproducible and observable | loader and runtime fetched by pin; the ICD's log reaches `diag/` |

**Amendment carried (PATCH):** "The only process-lifetime state" gains the Vulkan
ICD's root behind `vk_icdGetInstanceProcAddr`, for the same reason as the executor
library's.

## Classes this feature introduces

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `IcdRoot` | system (guest UM) | the instance pool, the entry table, the injected seams | Answer the loader's entry points |
| `EntryTable` | system | name to function pointer rows: generated wrappers and ICD-local entries | Resolve a Vulkan entry point by name |
| `Connection` | object | one adapter, `GuestConnection`, `KmtTransport`, a lock | Carry one instance's commands to its rvk context |
| `LoaderHandle` | object | loader magic, the host handle, its connection, its kind | Be one dispatchable object the loader can dispatch through |
| `HandleRegistry` | system | the pool of `LoaderHandle`s | Wrap host handles into loader handles |
| `CommandBatch` | object | framed commands of one command buffer, their chunks | Record a command buffer's commands until it ends |
| `MemoryTypeTable` | object | the guest's memory types and heaps | Translate host memory types into the adapter's |
| `HostVisibleChunk` | object | one `host_visible_memory` allocation, its view, its blob, its import | Back host-visible device memory with guest RAM |
| `ChunkAllocator` | system | the chunks of one device and their free ranges | Place a host-visible allocation in a chunk |
| `ExtensionPolicy` | object | the table of reportable extensions and their tests | Decide which host extensions the guest sees |
| `SlicedWait` | object | the deadline, the slice, the clock | Bound one host wait under TdrDelay |
| `ProcessHeap` | object (um shim) | the process heap handle | Give the ICD memory from the Windows heap |
| `HostVisibleBinding` | object (KMD) | the chunk's allocation, its resource id | Publish a chunk's guest pages to the device |
| `BlobImportCommand` | object (device) | the batch's blob table and executor | Import a guest blob into the context's executor |
| `GuestPageAlias` | object (device) | a reserved host range and its per-run mappings | Present a blob's guest pages as one host range |
| `VulkanProbe` | object (guest suite) | the Vulkan objects one run makes | Prove an instance, device, memory and workload through the loader |

`KmtTransport` gains construction-time ring order, command limit and reply size
and a batch publish; `Kmt` gains the adapter LUID and video memory info;
`VulkanBackend::adopt` honours the requested memory type and registers the handle.

## Project Structure

### Documentation (this feature)

```text
specs/045-vulkan-icd/
├── plan.md, research.md, data-model.md, quickstart.md
├── contracts/loader_interface.md       # exports, manifest, INF value
├── contracts/host_visible_memory.md    # binding record, IMPORT_BLOB, executor import
└── tasks.md
```

### Source Code

```text
src/icd/vulkan/                   # NEW: the ICD's decisions, portable, in av_icd
src/proto/icd_connection.hpp      # NEW: 063 contract §F (Connection, acquire/release)
src/proto/icd_handle_registry.hpp # NEW: 063 contract §F (wrap, connection_of, record_target)
src/um/vulkan/                    # NEW: rvkvk.dll exports, ProcessHeap, SRW lock, file log, .def, rvkvk.json
src/um/probe/                     # NEW: rvkvkprobe.exe shim (imports vulkan-1.dll)
src/guest/suite/vulkan_probe.*    # NEW: the probe's scenarios, portable
src/guest/transport/              # KmtTransport sizes and batch publish
src/guest/kmt/, src/um/gdi_kmt.*  # LUID, QueryVideoMemoryInfo
src/core/memory/, src/core/rvk/   # host_visible_memory kind, HostVisibleBinding, escape
src/proto/resources/, src/proto/submission/  # binding record, IMPORT_BLOB ids
src/device/execution/, src/device/memory/, src/device/qemu/  # BlobImportCommand, GuestPageAlias, alias host op
src/device/executor/, src/device/shim/  # adopt by type, handle registration, CreateDevice extensions
src/km/rvkgpu.inf                 # VulkanDriverName, rvkvk.dll, rvkvk.json
tools/fetch_vulkan_runtime.py, tools/gen_spirv.py, tools/check_guest_image.py
tools/guest/run_guest_suite.py    # --vulkan: push loader, ICD, probe; vulkaninfo; ICD log to diag/
tests/unit/icd/vulkan/, tests/harness/icd/, tests/loopback/test_fullstack_vulkan_icd.cpp
tests/fuzz/fuzz_host_visible_record.cpp, fuzz_blob_import.cpp
```

**Structure Decision**: a new semantic area `src/icd/` for guest user-mode ICD
logic (050 and 052 add `opengl/` and `opencl/` beside it); README's layout lists it.

## Complexity Tracking

| Violation | Why needed | Simpler alternative rejected because |
|---|---|---|
| A fourth piece of process-lifetime state (the ICD root) | the loader calls `vk_icdGetInstanceProcAddr(NULL, name)` with no context | the executor root already set this pattern; there is nowhere else to keep the entry table |
| A second mapping of guest RAM in the QEMU process | the executor needs one contiguous host pointer per import | copying at map/unmap is not what US2.1 asks and makes every map a round trip |
