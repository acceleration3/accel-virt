---

description: "Tasks: the host executor"
---

# Tasks: the host executor

**Input**: Design documents from `/specs/042-host-executor-library/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/executor_abi.md, quickstart.md

**Tests**: Required, and written first. Constitution IV.

**Organization**: by user story, in priority order. Each `**Checkpoint**` is a commit once `ctest --test-dir build -E tools_tests` passes there.

## Format: `[ID] [P?] [Story] Description`

- **[Story]**: US1 (the device runs this project's backends), US2 (memory is imported, not copied), US3 (any executor is held to the contract)

---

## Phase 1: Setup - the library exists and the device can load it

**Purpose**: a second implementation of the ABI, built in every configuration, before it does anything.

- [X] T001 `src/device/executor/` and a `rvk_executor` shared-library target in `CMakeLists.txt`, built in **every** configuration rather than only under `AV_QEMU`: the conformance suite must run sanitized, and `AV_QEMU` returns at `CMakeLists.txt:143` before the sanitizer flags (research §5)
- [X] T002 [P] Write `tests/unit/device/executor/test_host_gpu.cpp` first: a `HostGpu` answers its UUID, its render node and the sub-APIs it serves; one with no Vulkan answers no Vulkan rather than failing to construct
- [X] T003 `HostGpu` (object class) in `src/device/executor/host_gpu.{hpp,cpp}` - one host GPU: its 16-byte Vulkan device UUID, its DRM render node path, the sub-API bits and the `RVK_BLOB_*` kinds it can import. Two-phase: `initialize()` returns `Status`
- [X] T004 [P] Write `tests/unit/device/executor/test_host_gpu_set.cpp` first: a set with two GPUs finds each by UUID; an unknown UUID is not found; a host with no GPU at all is an empty set, not a failure
- [X] T005 `HostGpuSet` (system class) in `src/device/executor/host_gpu_set.{hpp,cpp}`, `max_gpus = 8` (data-model; this machine has two, a `HostGpu` is under 100 bytes)
- [X] T006 The `extern "C"` boundary in `src/device/executor/executor_ops.cpp`: `rvk_executor_ops()` answering a file-scope `const RvkExecutorOps`, each function one line into the library root held in `ObjectStorage`. This is the fourth process-lifetime object and `CLAUDE.md` now names it

**Checkpoint**: a library that loads, answers `sub_apis` and creates nothing. Commit.

---

## Phase 2: Foundational - what every backend needs

**Purpose**: the decode path and the session, which no story can do without.

- [X] T007 [P] Write `tests/unit/device/executor/test_decode_arena.cpp` first: a decode that fits leaves `refusals() == 0`; one that does not is refused and counted; `reset()` starts the next decode and not the count
- [X] T008 `DecodeArena` (object class) in `src/device/executor/decode_arena.{hpp,cpp}`: owns the block and the `WireArena` over it. `arena_bytes = 1 MiB` (data-model: no largest command exists - every count comes off the wire - so this is a cost decision at 64 MiB across the 64 sessions the device can ask for, and the refusal count is the signal to raise it)
- [X] T009 [P] Write `tests/unit/device/executor/test_context_session.cpp` first: a session runs a command and answers executed; an undecodable payload answers undecodable and counts no refusal; a payload the arena will not hold answers undecodable **and** counts one, which is how the two are told apart (contract §C)
- [X] T010 `ContextSession` (object class) in `src/device/executor/context_session.{hpp,cpp}`: one context's backend, arena and imports. States are created → running → faulted → destroyed (data-model)
- [X] T011 `ExecutorLibrary` (system class) in `src/device/executor/executor_library.{hpp,cpp}` - **the library-side one**; the device already has `src/device/execution/executor_library.*` and the two are different things. Owns the `HostGpuSet` and at most `max_sessions = 64` sessions, which is `DeviceContextTable::max_contexts` (`src/device/context/device_context_table.hpp:25`) rather than a second cap nobody enforces

**Checkpoint**: the ABI answers for real, with no host API behind it yet. Commit.

---

## Phase 3: US1 - The device runs this project's backends (Priority: P1) 🎯 MVP

**Goal**: a guest's Vulkan commands run on the host GPU and the answer comes back.

**Independent Test**: the conformance suite submits `vkCreateInstance` and `vkEnumeratePhysicalDevices` built with the generated serializers and reads back the host's own physical device count.

- [X] T012 [US1] Write `tests/unit/device/executor/test_vulkan_backend.cpp` first: a command the backend implements runs; a command outside the profile answers unknown, not undecodable; a host API error that loses the device answers device-lost
- [X] T013 [US1] `VulkanBackend` (object class) in `src/device/executor/vulkan_backend.{hpp,cpp}`, deriving from `av::vkproto::DefaultServerDispatch` **and not from `ServerDispatch`**, so that widening feature 063's profile is a behaviour change rather than a build break (research §2)
- [X] T014 [US1] The four `RVK_*` results are produced honestly: `dispatch_command` false with a refusal counted and false without are both `RVK_UNDECODABLE` on the wire but distinguishable in the log; an id the profile does not carry is `RVK_UNKNOWN`; `VK_ERROR_DEVICE_LOST` is `RVK_DEVICE_LOST` for that executor only, because the device has no device-wide loss path (research §1)
- [X] T015 [US1] The 36 Vulkan ids reserved but not generated are answered `RVK_UNKNOWN` deliberately rather than by falling off a switch, and `contracts/executor_abi.md` records which they are: the ICD answers them without a round trip (research §2)
- [X] T016 [US1] Fix the two device defects this feature owns: `rvk_executor_ops()` returning null must fail realize rather than yield a capset-less device (`src/device/qemu/virtio-gpu-rvk.c:65`), and a result outside the defined set must be reported as a broken library rather than folded into device-lost (`src/device/execution/context_executor.cpp:50`). A test for each

**Checkpoint**: Vulkan runs on the host GPU. This is the MVP and the feature could stop here. Commit.

---

## Phase 4: US2 - Memory is imported, not copied (Priority: P1)

**Goal**: a blob the guest wrote is memory the host GPU reads, with no transfer command.

**Independent Test**: the conformance suite imports a host-visible blob, writes a pattern through the host's mapping, and reads it back through a Vulkan buffer bound to the imported memory.

- [X] T017 [US2] `src/device/abi/rvk_executor.h` becomes version 2: `RvkBlobHandle` with exactly the fields contract §B1 names, `import_kinds`, `import_blob`, `release_blob`. Version 1 keeps working and the device accepts both (FR-006); a v1 table simply imports nothing
- [X] T018 [P] [US2] Write `tests/unit/device/executor/test_imported_blob.cpp` and `test_import_table.cpp` first: a handle whose `size` is not a whole number of pages is refused; a `kind` that is not exactly one bit is refused; `release_blob` on an id never imported is a no-op; a re-import of a live id is refused rather than silently tearing the old one down (research §3)
- [X] T019 [US2] `ImportedBlob` (object class) and `ImportTable` (system class) in `src/device/executor/`, `max_imports = 256` per session (data-model). Named `ImportTable` because `av::BlobTable` already exists at `src/device/memory/blob_table.hpp:22`
- [ ] T020 [US2] `BlobHandleSource` (system class) in `src/device/memory/`: builds the handle on the **main loop** and caches it, because every host memory operation is main-loop-only and `execute` runs on the worker (contract §B4). A guest-backed blob becomes one dma-buf through `UDMABUF_CREATE_LIST`, which QEMU's own `hw/display/virtio-gpu-udmabuf.c:30-67` already does and which removes the device-side flattening copy entirely
- [X] T021 [US2] `VulkanBackend` imports a host pointer through `VK_EXT_external_memory_host`: the memory type comes from `vkGetMemoryHostPointerPropertiesEXT` **intersected with the buffer's own `memoryTypeBits`**, per pointer, per device - measured 0x7f on Intel and 0x18 on NVIDIA, so an imported pointer can never be device-local there (research §3)
- [X] T022 [US2] `feature_guest_memory_import` in the capset is set only when `import_kinds` answers non-zero for the bound GPU, so the bit stops being a promise and starts being a report (FR-007, research §5). Per-sub-API granularity deliberately does not go on the wire
- [ ] T023 [US2] *Waiting for a caller.* The refusal path is in - an API that cannot take a kind answers no, the device is told, and the capset bit is withdrawn - but no command in any profile names a blob yet, so a copy would be a path nothing walks (constitution VIII). It lands with the first command that names one. The fallback copy lives in the executor, because only it knows the destination's shape - a `memoryTypeBits` intersection, a `cl_mem`'s 128-byte base alignment - and a device-side copy would land in a bounce buffer the executor copies again (research §3)

**Checkpoint**: zero-copy on the paths that have it, an honest copy where they do not. Commit.

---

## Phase 5: US3 - Any executor is held to the contract (Priority: P1)

**Goal**: the suite FR-001 promises and this repository has never had.

- [X] T024 [US3] `ExecutorConformance` (system class) and `tests/conformance/executor/`, run as `executor_conformance <library path>` against **both** `rvk_echo_executor` and `rvk_executor`: a suite only one library passes has tested nothing
- [X] T025 [US3] Every statement in contract §A is a case, including the two the device cannot enforce for itself: A9, every call returns, checked under a watchdog so a hung call fails the suite instead of hanging it; A10, a result outside the defined set fails
- [X] T026 [US3] Contract §A7 at the ABI level: a capacity one byte short of the answer leaves the reply block untouched and `*reply_size` at the full length. The device's `status_too_large` header is the layer above and is already covered by `tests/qemu/test_virtio_gpu_execution.py:104-111`
- [X] T027 [P] [US3] Vulkan's is written, seeded and finding things; OpenGL's and OpenCL's land with their backends. `tests/fuzz/fuzz_executor_vulkan.cpp`, `fuzz_executor_opengl.cpp`, `fuzz_executor_opencl.cpp` with committed corpora seeded from real encodes - three targets, because SC-003 wants 60 seconds per sub-API and the gate budgets per target
- [X] T028 [US3] The suite records what it cannot check: worker **identity** is not enforced, because `TableHost::require_worker` only tests that the BQL is not held (`src/device/abi/table_host.cpp:25-28`) and cannot tell one worker from another

**Checkpoint**: the contract is a thing a library can be held to. Commit.

- [X] T029 Enum fields decoded from the wire are loaded as enums, which is undefined for every value the enum does not have. Three separate instances so far, all found by fuzzing: the pNext chain's `sType` (fixed - switched on as the number it is), and a length expression reading `rasterizationSamples`, whose reproducer is in `tests/corpus/fuzz_executor_vulkan/enum_in_a_length.bin`. The emitter must stop naming an enum-typed member anywhere a wire value can reach a load: read through the underlying type, and cast only what matched something known. `fuzz_executor_vulkan` joins the fuzz gate in the same commit
- [X] T030 One backend per session rather than one per library, destroyed with it, so a context that dies without destroying its instance does not leak a host object. Found by the fuzz target in seconds; the reproducer is `tests/corpus/fuzz_executor_vulkan/instance_never_destroyed.bin` and leak detection is off in that target until this lands
- [X] T031 Every non-dispatchable handle is checked the way the five dispatchable ones now are. A guest naming a VkBuffer this executor never made still reaches the driver, which is the same hole one layer down

- [X] T032 The strong `handle_to_wire` and `wire_to_handle` live in `av_core`, so every binary that links it gets the host's semantics - including the guest-side ICD feature 045 will build, which needs the identity default and would otherwise have every handle it decodes refused. The host's hooks belong to the executor library alone, and getting them there without the weak-symbol trap (a strong definition in an object nothing references is never pulled in) is the work

---

## Phase 6: OpenGL, which is not the same work

**Purpose**: three things must exist before `gl_dispatch_server.cpp` compiles at all.

- [X] T033 The generated OpenGL sync hooks gain a user pointer. They became three function-pointer fields on `RealGl` beside `rvk_blob`, with an `rvk_sync_user`, and `gl_dispatch_server.cpp` compiles standalone for the first time. They are parameterless free functions today - `sync_for`, `bind_sync`, `forget_sync`, nine call sites - and a parameterless function cannot reach one context's state without thread-local or file-scope storage, which this project does not have. They join `rvk_blob` as fields on `RealGl` with an `rvk_sync_user` beside `rvk_blob_user`. An emitter change in `tools/codegen/rvk_codegen/opengl/emit.py`, regenerated and gated; no id moves (research §6)
- [X] T034 [P] Write `tests/unit/device/executor/test_sync_tokens.cpp` first, then `SyncTokens` (system class) and `src/proto/gl_sync_table.hpp`, `max_sync_tokens = 256` (data-model: past any real pipeline depth, and a guest that never deletes must be bounded)
- [X] T035 [P] Write `tests/unit/device/executor/test_guest_region.cpp` first: an offset and length inside the region resolve; one past it answers null; a length that overflows answers null. Then `GuestRegion` (object class) - the resolver `RealGl::rvk_blob` names, consulted at **1016** call sites, where null means every one of them refuses
- [ ] T036 A blob with more than one extent is refused by `GuestRegion` rather than stitched: it must answer a host pointer and a scattered blob has none. Vulkan and OpenCL are unaffected - the device hands them one `UDMABUF_CREATE_LIST` dma-buf (research §6)
- [ ] T037 `OpenGlBackend` (object class): EGL through `EGL_EXT_platform_device` and `eglQueryDevicesEXT`, **not** `EGL_MESA_platform_surfaceless`, which gives no GPU choice and landed on the wrong GPU in the probe; `create()` is given a UUID and must honour it. Device enumeration returns entries that fail `eglInitialize` and must be tolerated, not indexed into (research §3)
- [ ] T038 The four mapping commands the generator refused to guess - `glMapBuffer`, `glMapBufferRange`, `glUnmapBuffer`, `glFlushMappedBufferRange` - because a pointer into host GL memory cannot cross the wire
- [X] T039 `gl_dispatch_server.cpp` joins `AV_GENERATED_SOURCES`, which it cannot today

**Checkpoint**: OpenGL runs. Commit.

---

## Phase 7: OpenCL

- [ ] T040 `OpenClBackend` (object class) deriving from `av::clproto::RealDispatch`; the one reserved id, `clGetKernelSuggestedLocalWorkSize`, answered unknown deliberately
- [ ] T041 Import by dma-buf where the device offers it, probed through `CL_DEVICE_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR` rather than by matching the extension string: Intel NEO advertises dma-buf through the query while listing only `cl_khr_external_memory` at provisional 0.9.1, and NVIDIA's `opaque_fd` is a CUDA handle that cannot take a memfd (research §3). `CL_MEM_USE_HOST_PTR` is not a zero-copy guarantee and is not treated as one
- [ ] T042 A blob larger than `CL_DEVICE_MAX_MEM_ALLOC_SIZE` cannot become one `cl_mem` at all - 1.914 GiB on the NVIDIA here - and is refused rather than truncated

**Checkpoint**: three APIs behind one ABI. Commit.

---

## Phase 8: What the feature promised

- [ ] T043 SC-002's three workloads in the conformance suite: a Vulkan triangle, an OpenGL clear, an OpenCL vector add, built with the generated serializers and read back from the reply. Driving the same three from a real guest is feature 044
- [ ] T044 `docs/protocol.md` and `docs/kb/11-implementation-plan.md` describe version 2 and the import path
- [ ] T045 Feature 063's `contracts/generated_code.md` records that the no-STL line did not move, which is what settles its T016 for good
- [ ] T046 `AV_GUEST_IMAGE=... tools/gate.sh` passes; merge to `master`; push

---

## Dependencies & Execution Order

- **Phase 1** blocks everything: without a library there is nothing to test.
- **Phase 2** blocks every story: every backend decodes through the same arena and session.
- **US1 (Phase 3)** is the MVP and can stop there - Vulkan on the host GPU is already worth having.
- **US2 (Phase 4)** needs US1 for something to import *into*.
- **US3 (Phase 5)** can be written against the echo library before US1 exists, and should be: a suite written after the implementation tests the implementation, not the contract.
- **Phase 6** is the risk. Nothing in it is shared with Vulkan or OpenCL.
- **Phase 7** is the smallest of the three APIs.

### Parallel opportunities

- T002, T004, T007, T009 are test files written against classes that do not exist yet.
- T018 and T027 are independent test files.
- T030 and T031 are two unrelated OpenGL prerequisites.
- Phases 6 and 7 touch no file in common and can run together once Phase 2 is in.

---

## Implementation Strategy

**MVP**: Phases 1-3 - a library that loads, decodes and runs Vulkan on the host GPU. **Increment 2**: Phases 4 and 5, which make the memory claim true and the contract enforceable. **Increment 3**: Phases 6 and 7, the other two APIs.

---

## Notes

- The executor library is built like everything else here: no STL, no exceptions, no RTTI, memory from the host ops with an explicit pool. The line did not move (research §4).
- Every call the device makes must return. Nothing times it out, so any host call that can block needs its own bound or `CTX_DESTROY` hangs QEMU.
- The payload pointer is borrowed for the length of one `execute` and the next batch overwrites it.
