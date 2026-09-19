---

description: "Tasks: Sharing across APIs and processes"
---

# Tasks: Sharing across APIs and processes

**Input**: Design documents from `/specs/053-cross-api-sharing/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/shared_objects.md, contracts/wire_additions.md, quickstart.md

**Tests**: Required, and written first (constitution IV). Every new class gets its test before its body, `invariants_hold()` asserted after every mutation, and a hostile-input table plus a fuzz target for anything reading bytes it did not write.

**Organization**: the wire first, because both sides decode it; then the device's identity (US3), which is what US1's kernel half and US2's three ICDs both name; then US1's kernel and NT handle; then US2's three APIs; then the proof and the gates. Each `**Checkpoint**` is a commit once `ctest --test-dir build -j32 -LE tools` passes there.

**Branch**: `053-cross-api-sharing` from `052-opencl-icd`, rebased onto `master` once 052 lands.

## Format: `[ID] [P?] [Story] Description`

- **[Story]**: US1 (shared allocations and fences across processes), US2 (the interop extensions), US3 (object identity is global on the host)

---

## Phase 1: The wire

**Purpose**: the three records both sides read, and the ids that name them. Nothing else can be written first: the miniport writes what the device reads.

- [ ] T001 [P] `tests/unit/proto/resources/test_shared_object_kind.cpp` first: `SharedObjectKind` in `src/proto/resources/shared_object_kind.hpp` is an `enum class : uint32_t` with `memory = 1`, `monitored_fence = 2`, `native_fence = 3`, and a `SharedObjectKinds` object class whose `known(uint32_t)` refuses 0 and 4 and everything above (data-model.md). No free function: the predicate is a member of a class holding the highest kind it knows.
- [ ] T002 [P] `tests/unit/proto/resources/test_shared_object_record.cpp` first, then `src/proto/resources/shared_object_record.{hpp,cpp}`: 40 bytes, little-endian, the layout in data-model.md. `encode(bytes, capacity)`, `decode(bytes, length)` unchanged unless ok. Refuses a magic that is not `0x534B5652`, a version that is not 1, `object_id` 0, a kind `SharedObjectKinds` does not know, a reserved field that is not 0, `size` 0 or not a whole number of 4096-byte pages, and any length but 40. The hostile table covers zero length, every truncation at a field boundary, a length past the buffer, maximum and minimum of every scalar, and a byte-flip sweep of a valid record.
- [ ] T003 [P] `tests/unit/proto/submission/test_shared_object_request.cpp` first, then `src/proto/submission/shared_object_request.{hpp,cpp}`: 24 bytes, the layout in data-model.md, with `encoded_size = 24`. Refuses `object_id` 0, an unknown kind, a reserved field that is not 0, and any length but 24. `wire_handle` is carried unjudged: which of the three commands may set it is the command's rule, not the body's.
- [ ] T004 [P] `tests/unit/proto/submission/test_shared_object_answer.cpp` first, then `src/proto/submission/shared_object_answer.{hpp,cpp}`: 16 bytes, `encoded_size = 16`, a `Status` enum of the seven statuses in data-model.md, and `encode` only - nothing in this repository decodes an answer except a test and the guest ICDs, which do decode it, so `decode` exists and refuses an unknown status.
- [ ] T005 `src/proto/submission/command_header.hpp` gains `share_object_id = 0x4`, `open_shared_object_id = 0x5` and `release_shared_object_id = 0x6` beside the blob pair, with the comment naming feature 053; `tests/unit/proto/submission/test_command_header.cpp` gains a case per id proving `api()` is 0 for each.
- [ ] T006 [P] `tests/fuzz/fuzz_shared_object_record.cpp` and `tests/fuzz/fuzz_shared_object_request.cpp` with committed seed corpora in `tests/corpus/fuzz_shared_object_record/` and `tests/corpus/fuzz_shared_object_request/`: a valid encoding of each, a zero-length input, and one of each refusal. Registered in `CMakeLists.txt` beside `fuzz_blob_import`.

**Checkpoint** — the wire decodes and refuses; `ctest -R shared_object` is green and the two fuzz targets survive their smoke run.

---

## Phase 2: US3 — one host resource per object, for the whole device

**Goal**: two contexts of different guest devices naming one id hold one host resource with two references, and a context that dies leaves it alive.

**Independent Test**: `ctest --test-dir build -R 'host_shared_object|context_shared_objects|shared_object_command'`, and the two named tests in quickstart.md.

- [ ] T007 [US3] `src/device/abi/rvk_executor.h` goes to version 6: `RVK_EXECUTOR_ABI_VERSION 6`, `RVK_EXECUTOR_ABI_VERSION_SHARING 6`, `RVK_SHARED_MEMORY`/`RVK_SHARED_MONITORED_FENCE`/`RVK_SHARED_NATIVE_FENCE`, and the three calls `export_object`, `import_object` and `release_object` with the signatures and the threading rules in contracts/wire_additions.md. The header's version paragraph gains the sentence saying what a version 5 library does (nothing: the device answers status 4 without calling).
- [ ] T008 [P] [US3] `tests/unit/device/memory/test_host_shared_object.cpp` first, then `src/device/memory/host_shared_object.{hpp,cpp}`: the object class in data-model.md. `take(id, kind, blob_kind, size, fd)` refuses id 0, an unknown kind, a `blob_kind` that is neither `RVK_BLOB_DMA_BUF` nor `RVK_BLOB_HOST_POINTER`, size 0, a negative fd, and a slot already live; `hold()`/`drop()` move the count and `drop()` of the last holder closes the descriptor exactly once; `give_back()` is safe on a slot holding nothing. `invariants_hold()` after every one.
- [ ] T009 [US3] `tests/unit/device/memory/test_host_shared_objects.cpp` first, then `src/device/memory/host_shared_objects.{hpp,cpp}`: the system class in data-model.md, `max_objects = 256`, its own host mutex, `initialize()` answering `no_memory` when the host has no lock. `publish(id, kind, blob_kind, size, fd)` refuses a live id with a different descriptor (`conflict`) and answers ok for the same one again; `descriptor(id, kind, size, blob_kind, fd)` answers a `dup` of the fd and refuses an unknown id (`not_found`) or a kind or size that disagrees (`invalid_parameter`); `hold`/`drop`; `release_all()`. `report(StateWriter&)` writes `busy=true` and nothing more while another thread holds it.
- [ ] T010 [US3] `tests/unit/device/execution/test_context_shared_objects.cpp` first, then `src/device/execution/context_shared_objects.{hpp,cpp}`: `max_objects = 64`, one row per object this context published or opened with the wire handle its executor made. `publish(id, kind, wire_handle)`, `open(id, kind, size, wire_handle)`, `release(id)` and the destructor, which releases everything left through the executor and drops every reference on the device's table. Modelled on `BlobImports`, which it sits beside.
- [ ] T011 [US3] `src/device/execution/context_executor.{hpp,cpp}` gains `export_object`, `import_object` and `release_object`, each refusing with `not_supported` when the library is older than `RVK_EXECUTOR_ABI_VERSION_SHARING`, when there is no executor, or when the library answers anything but `RVK_EXECUTED`; `tests/unit/device/execution/test_context_executor.cpp` gains a case per refusal over `MockExecutorOps`.
- [ ] T012 [US3] `tests/harness/executor/mock_executor_ops.{hpp,cpp}` gains the three version 6 entries, a scriptable refusal for each, and a record of every call, so a test can make the library refuse an export, an import, or both; its `abi_version` becomes settable so a version 5 table can be presented.
- [ ] T013 [US3] `tests/unit/device/execution/test_shared_object_command.cpp` first, then `src/device/execution/shared_object_command.{hpp,cpp}`: one class answering ids 4, 5 and 6, framed exactly as `BlobImportCommand` frames its two - `malformed` for a body of any length but 24, `not_found` for another id - and every other refusal a status in the 16-byte answer. The status table in data-model.md is the contract, and every status has a test.
- [ ] T014 [US3] `src/device/execution/transport_command.{hpp,cpp}` dispatches ids 4-6 to `SharedObjectCommand` as it dispatches 2-3 to `BlobImportCommand`; `tests/unit/device/execution/test_transport_command.cpp` gains the three ids and a case proving an unknown id 7 is still `not_found`.
- [ ] T015 [US3] `src/device/context/device_context.{hpp,cpp}` holds a `ContextSharedObjects` declared before its `ContextExecutor` (an open points into an object until the executor that holds it is gone), configured at bind, released before the executor is destroyed; `src/device/module/virtio_gpu_rvk.{hpp,cpp}` holds the `HostSharedObjects` and hands it to the context table, initialized where `BlobTable::initialize` is, released on reset. `DeviceContextTable`'s constructor takes it.
- [ ] T016 [US3] `tests/unit/device/context/test_device_context.cpp` and `tests/unit/device/module/test_virtio_gpu_rvk.cpp` gain the cases US3 needs: two contexts publishing and opening one id hold one host resource with two references; the publisher's context destroyed leaves the object and the other context's handle alive; a device reset releases every object and closes every descriptor; a context faulting drops only its own references.

**Checkpoint** — US3 is provable on the device: one id, one descriptor, a refcount that survives its publisher. Commit.

---

## Phase 3: US3 — the three backends

**Goal**: the executor library really exports and imports, so the identity is a host resource and not a bookkeeping entry.

- [ ] T017 [P] [US3] `tests/unit/device/executor/test_vulkan_backend.cpp` first, then `src/device/executor/vulkan_backend.{hpp,cpp}`: `export_object` for `RVK_SHARED_MEMORY` through `vkGetMemoryFdKHR` and for the two fence kinds through `vkGetSemaphoreFdKHR`, refusing a `wire_handle` this backend did not make (`knows_handle`) and a kind it cannot export; `import_object` through `VkImportMemoryFdInfoKHR` and `vkImportSemaphoreFdKHR`, recording what it made with `made()` so the guest may name it; `release_object` through `vkFreeMemory`/`vkDestroySemaphore` for a handle it made and nothing for one it did not. The dma-buf and opaque-fd handle types are both accepted on import; which one is asked for on export is `ImportableTypes`' decided kind.
- [ ] T018 [P] [US3] `src/device/executor/opengl_backend.{hpp,cpp}`: `export_object` answers `RVK_UNKNOWN`; `import_object` makes a memory object with `glCreateMemoryObjectsEXT` + `glImportMemoryFdEXT` for `RVK_SHARED_MEMORY` and a semaphore with `glGenSemaphoresEXT` + `glImportSemaphoreFdEXT` for the fence kinds, each refused unless the context's host lists the matching `GL_EXT_*_fd` extension and the descriptor is a dma-buf; `release_object` deletes it. Tests over the existing scripted GL.
- [ ] T019 [P] [US3] `src/device/executor/opencl_backend.{hpp,cpp}`: `export_object` answers `RVK_UNKNOWN`; `import_object` makes a `cl_mem` with `CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR` for `RVK_SHARED_MEMORY`, refused unless the host device lists `cl_khr_external_memory_dma_buf` and the descriptor is a dma-buf, and answers `RVK_UNKNOWN` for the fence kinds; `release_object` releases it.
- [ ] T020 [US3] `src/device/shim/executor_ops.cpp` publishes version 6 with the three entries; `tests/unit/device/shim/test_executor_ops.cpp` (or the executor-root test beside it) proves the table's version and that each entry is filled.

**Checkpoint** — the library exports and imports for real. Commit.

---

## Phase 4: US1 — the miniport mints an identity the adapter agrees on

**Goal**: an allocation created shareable has an id no other adapter and no forger can name, and a second device's open is counted.

**Independent Test**: `ctest --test-dir build -R 'shared_object_table|allocation|device_table|ddi_scenarios'`.

- [ ] T021 [P] [US1] `src/core/platform/lock_rank.hpp` gains `shared_objects`, ordered above `allocation_table` and below the device table's, with the comment saying why; `tests/unit/core/platform/test_lock_rank.cpp` gains it.
- [ ] T022 [P] [US1] `tests/unit/core/memory/test_shared_object.cpp` first, then `src/core/memory/shared_object.{hpp,cpp}`: the object class in data-model.md, over `MockPlatform`, with `open(device)`/`close(device)` counting and `invariants_hold()` after each. `close` of a device that never opened it is `invalid_parameter` and changes nothing.
- [ ] T023 [US1] `tests/unit/core/memory/test_shared_object_table.cpp` first, then `src/core/memory/shared_object_table.{hpp,cpp}`: the system class in data-model.md. `mint(allocation, kind, size, luid, record)` fills the record and answers `no_memory` under the failing allocator; `open(record, device, object)` refuses an id it never minted, a LUID that is not this adapter's, a kind or size that disagrees, and a record that does not decode; `close`; `forget(allocation)`. The allocation-failure sweep runs the whole sequence once per site.
- [ ] T024 [US1] `src/proto/resources/allocation_descriptor.{hpp,cpp}` and `src/core/memory/allocation_table.cpp`: private data of 48 bytes is unshared and unchanged; 88 bytes is a descriptor followed by a `SharedObjectRecord`, which `AllocationTable::create` mints into and writes back; any other length is refused as today. `Allocation` gains `shared_id()` and its `open(device)` allows a second device for a shared allocation while still refusing one for a ring or a fence page. `tests/unit/core/memory/test_allocation.cpp` and `test_allocation_table.cpp` gain a case per branch.
- [ ] T025 [US1] `src/core/scheduling/device_table.cpp`: `open_one` finds the shared object a record names and counts the open on it as well as on the allocation, and `close` uncounts it; `tests/unit/core/scheduling/test_device_table.cpp` proves a second device's open, a close in either order, and a forged record refused with no allocation opened.
- [ ] T026 [US1] `src/core/adapter/adapter.{hpp,cpp}` owns the `SharedObjectTable`, hands it to the allocation and device tables, and reports it in the adapter's state dump; `tests/unit/core/adapter/test_adapter.cpp` gains the dump's line.
- [ ] T027 [US1] `tests/unit/scenarios/test_ddi_scenarios.cpp` gains the L2 scenario: `DxgkDdiCreateAllocation` with a shareable private data, `DxgkDdiOpenAllocation` on a second device with the creator's private data, `DxgkDdiCloseAllocation` in each order, and `DxgkDdiDestroyAllocation` - all through the registered table, with `FakeVidMm`'s handles.

**Checkpoint** — the identity is minted, refused and refcounted in the kernel. Commit.

---

## Phase 5: US1 — the NT handle

**Goal**: a handle a process can send to another process, and a handle from nowhere that is refused.

- [ ] T028 [US1] `src/guest/kmt/kmt.hpp` gains `share_object(device, allocation, handle)`, `open_shared_object(device, handle, allocation)` and `close_shared_object(handle)`, each documented with what gdi32 function it becomes and what it refuses; `src/um/icd/gdi_kmt.{hpp,cpp}` translates each (`D3DKMTShareObjects`, `D3DKMTOpenResourceFromNtHandle`, `CloseHandle`) with no branch the ABI does not mandate.
- [ ] T029 [US1] `tests/harness/guest/mock_kmt.{hpp,cpp}` scripts the three: a handle minted, a handle opened, a handle the mock never minted refused, a handle opened twice, and a close of one never opened. `tests/unit/guest/kmt/test_mock_kmt.cpp` (or the file that already holds MockKmt's own cases) proves each.
- [ ] T030 [US1] `tests/unit/icd/common/test_shared_handles.cpp` first, then `src/icd/common/shared_handles.{hpp,cpp}`: the system class in data-model.md, `max_handles = 64`, over the injected `Kmt` and `ThreadLock`. `mint(device, allocation, object_id, handle)`, `resolve(handle, object_id)` answering false for a handle it did not mint and did not open, `adopt(device, handle, object_id)` for one another process sent, `forget(handle)` closing it, and `forget_device(device)`. `invariants_hold()`: a row is empty or holds one non-zero handle and one non-zero id, and no handle or id appears twice.

**Checkpoint** — a handle crosses a process boundary and a forged one does not. Commit.

---

## Phase 6: US2 — Vulkan

**Goal**: `VK_KHR_external_memory_win32` and `VK_KHR_external_semaphore_win32` on a handle another process opened.

- [ ] T031 [US2] `src/icd/vulkan/external_objects.{hpp,cpp}`: an export now mints an NT handle through `icd::SharedHandles` and publishes the object with `SHARE_OBJECT`; an import resolves a handle to an object id - its own or another process's - and opens it with `OPEN_SHARED_OBJECT`, taking the `VkDeviceMemory` or `VkSemaphore` the device answers. A named handle is still refused, and so is a handle type outside `Win32HandleKind`. `tests/unit/icd/vulkan/test_external_objects.cpp` gains a case per branch, including a handle minted in this process and imported by a second device, and a handle `MockKmt` says is another process's.
- [ ] T032 [US2] `src/icd/vulkan/icd_root.{hpp,cpp}` owns the `icd::SharedHandles` and hands it to `ExternalObjects`; `vkGetMemoryWin32HandlePropertiesKHR` and `vkGetSemaphoreWin32HandlePropertiesKHR` are answered for a handle the table knows and refused for one it does not, which 047 could not answer at all. `contracts/external_objects.md` of 047 gains a line saying 053 carried FR-005.
- [ ] T033 [US2] `src/icd/vulkan/external_objects.cpp` forgets a handle with the object it names and with its device, and the ICD's teardown closes every handle the process still holds; `test_external_objects.cpp` proves a process that exits holding everything leaves nothing.

**Checkpoint** — Vulkan's two Win32 extensions cross a process. Commit.

---

## Phase 7: US2 — OpenGL

**Goal**: `GL_EXT_memory_object_win32` and `GL_EXT_semaphore_win32` on the same handle.

- [ ] T034 [US2] `tools/codegen/profiles/opengl.toml` gains the six commands in contracts/wire_additions.md to `extra`, `tools/codegen/profiles/opengl_ids.toml` gains their ids append-only, and `src/proto/generated/opengl/` is regenerated and committed; `tools/check_generated.py` and `tools/check_protocol_ids.py` pass.
- [ ] T035 [US2] `tests/unit/icd/opengl/test_gl_external_objects.cpp` first, then `src/icd/opengl/gl_external_objects.{hpp,cpp}`: the system class in data-model.md. `import_memory(handle, size, name)` and `import_semaphore(handle, name)` resolve the handle through `icd::SharedHandles`, send `OPEN_SHARED_OBJECT` and answer the GL name the device made; `delete_memory`/`delete_semaphore` send `RELEASE_SHARED_OBJECT`. A handle from nowhere is `GL_INVALID_OPERATION` with `glGetError` set once.
- [ ] T036 [US2] `src/icd/opengl/gl_entry_points.{hpp,cpp}` and `manual_calls.cpp` answer `glCreateMemoryObjectsEXT`, `glDeleteMemoryObjectsEXT`, `glImportMemoryWin32HandleEXT`, `glGenSemaphoresEXT`, `glDeleteSemaphoresEXT` and `glImportSemaphoreWin32HandleEXT` locally through `GlExternalObjects`, and let the six generated commands through; `tests/unit/icd/opengl/test_gl_entry_points.cpp` gains a case per entry.
- [ ] T037 [US2] `src/icd/opengl/gl_extension_policy.cpp` gains the four rows in contracts/wire_additions.md, each naming its test; `tools/check_extension_policy.py` passes and a scripted host without `GL_EXT_memory_object_fd` is shown neither.

**Checkpoint** — OpenGL imports what Vulkan exported. Commit.

---

## Phase 8: US2 — OpenCL

**Goal**: `cl_khr_external_memory_win32` and `cl_khr_gl_sharing`.

- [ ] T038 [US2] `tests/unit/icd/opencl/test_cl_external_memory.cpp` first, then `src/icd/opencl/cl_external_memory.{hpp,cpp}`: `clCreateBufferWithProperties` carrying `CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR` resolves the handle, opens the object and wraps the `cl_mem` the device answers in a `ClObject`; every other external handle property is `CL_INVALID_PROPERTY`, a handle from nowhere is `CL_INVALID_VALUE`, and the property list is judged by the existing `ClPropertyList` hostile table.
- [ ] T039 [US2] `tests/unit/icd/opencl/test_cl_gl_sharing.cpp` first, then `src/icd/opencl/cl_gl_sharing.{hpp,cpp}`: the acquire depth per object, `clEnqueueAcquireGLObjects` refusing an object already acquired and `clEnqueueReleaseGLObjects` one that is not, both over the generated commands that already exist. `clGetGLObjectInfo` answers from the row rather than the host.
- [ ] T040 [US2] `src/icd/opencl/cl_icd_root.{hpp,cpp}` and `guest_cl_context.{hpp,cpp}` own the two tables and release them with the context; `src/icd/opencl/cl_extension_policy.cpp` gains the two rows, each naming its test, reported only under the conditions in contracts/wire_additions.md.

**Checkpoint** — all three APIs share one object. Commit.

---

## Phase 9: The proof, the documents and the gates

- [ ] T041 [P] `tests/loopback/test_fullstack_shared_objects.cpp`: two rvk contexts on one `FullStackRig`, one publishing and one opening, both naming one host resource; the publisher destroyed with the object alive; a hostile guest naming an id it never published, an id twice, and a kind that disagrees, each faulting or refusing only itself. On a host whose GPU cannot export by fd the test names the condition and returns, as `test_fullstack_external_objects` does.
- [ ] T042 [P] `docs/protocol.md` gains the three transport commands, their bodies, their answers and their status table, and the private-data paragraph; `docs/kb/04-memory.md`'s "Allocation lifetime traps" gains what this feature built for the third trap; `docs/kb/11-implementation-plan.md` risk 6 is marked answered with the two tables that answer it.
- [ ] T043 [P] `docs/testing.md`'s L4 list gains the shared-object scenario, and its "Still to come" line about a shared allocation opened by a second simulated process is struck.
- [ ] T044 `tools/scope_marks.py --apply` over every new file, then `ctest -R scope_marks`; `tools/check_design.py`, `tools/check_symbol_tests.py` and `tools/check_self_contained.py` pass.
- [ ] T045 The coverage configuration: every new branch covered or marked, the `AV_UNTESTABLE` ratchet unchanged at 0, and the allocation-failure sweep over both tables.
- [ ] T046 `tools/gate.sh`: default, coverage, fuzz and qemu green; the guest reported as skipped, which research.md §10 records as leaving SC-001 and SC-003 open.

**Checkpoint** — the gate's four Linux configurations pass. Commit.

---

## Dependencies & Execution Order

- **Phase 1** blocks everything: both sides decode the same records.
- **Phase 2** blocks Phase 3 (the backends implement the ABI Phase 2 defines) and Phase 6-8 (the ICDs send the commands Phase 2 answers).
- **Phase 4** is independent of Phases 2-3 and can be written in parallel with them: the miniport's table talks to nothing on the host.
- **Phase 5** blocks Phases 6, 7 and 8: all three ICDs resolve a handle the same way.
- **Phases 6, 7 and 8** are independent of each other.
- **Phase 9** depends on whatever landed.

### Parallel opportunities

- T001-T004 and T006 are four files with no dependency between them.
- T008, T017, T018 and T019 touch four different translation units.
- T021 and T022 are independent of everything in Phases 2-3.
- T035, T038 and T039 are three ICDs and never the same file.

---

## Implementation Strategy

US3 first, because it is the thing the other two stories name; then US1's kernel
half, which is what makes an identity crossable; then US2, one API at a time,
each independently testable and each reported only where the host carries the
extension it is built on (constitution VI).

Stopping after Phase 5 leaves a coherent feature: an identity minted in the
kernel, refcounted on the host, and reachable across a process boundary, with
047's in-process Win32 extensions unchanged. Stopping after Phase 6 adds
Vulkan's cross-process half, which is the one DXGI and DWM need.
