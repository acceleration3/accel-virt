---

description: "Tasks: rvk contexts in a real guest"
---

# Tasks: rvk contexts in a real guest

**Input**: Design documents from `/specs/044-rvk-context-live/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/transport_nop.md, quickstart.md

**Tests**: Required, and written first. Constitution IV.

**Organization**: by user story. Each `**Checkpoint**` is a commit once `ctest --test-dir build -E tools_tests` passes there.

## Format: `[ID] [P?] [Story] Description`

- **[Story]**: US1 (a context per sub-API runs real host work), US2 (a faulted context recovers alone), US3 (the run proves it with numbers)

---

## Phase 1: Setup - the guest can encode a real stream

**Purpose**: the generated clients build where the guest suite builds (research §1).

- [X] T001 [P] `tools/fetch_khronos_registries.py`: fetch `include/vk_video/*.h` from the already-pinned Vulkan-Headers tarball into `third_party/khronos/include/vk_video/`, so no build resolves them from `/usr/include`; its test in `tests/tools/` covers the new entries
- [X] T002 The emitters write freestanding C++: `tools/codegen/rvk_codegen/emit/{client_dispatch,serialize,ids}.py` and `tools/codegen/rvk_codegen/opencl/emit.py` (and the OpenGL emitter where it shares the pattern) include `<stdint.h>`/`<stddef.h>` instead of `<cstdint>`/`<cstddef>`/`<cstring>`, name `uint32_t`/`size_t` unqualified, and call `__builtin_memcpy`/`__builtin_strlen`; regenerate `src/proto/generated/`; `python3 tools/check_generated.py` green; the Linux build and every test unchanged
- [X] T003 `src/um/CMakeLists.txt`: build `vk_dispatch_client.cpp`, `cl_dispatch_client.cpp`, `vk_handle_default.cpp` and `vk_host_image_default.cpp` into `rvktest.exe` with `third_party/khronos/include` and the two generated directories on its include path; `src/um/imports/ntdll.def` gains `strlen`; `tools/check_guest_image.py` still passes (imports only gdi32, kernel32, ntdll)

**Checkpoint**: `rvktest.exe` links the Vulkan and OpenCL clients. Commit.

---

## Phase 2: Foundational - the executor distrusts the guest, and a no-op exists

**Purpose**: nothing a guest sends reaches a host ICD unvalidated (research §2), and a command whose round trip is the transport's own (research §4).

- [X] T004 [P] Write `tests/unit/device/executor/test_cl_handle_registry.cpp` first: a pointer the executor made has a wire value that resolves back; a wire value never made, of another kind, forgotten, zero, or past the capacity resolves to null; the registry refuses past its capacity
- [X] T005 [P] `ClHandleRegistry` (system class) in `src/device/executor/cl_handle_registry.{hpp,cpp}`, modelled on the Vulkan backend's registry; the OpenCL server dispatch resolves every handle argument through it and answers `RVK_UNDECODABLE` for one it does not know (the emitter `tools/codegen/rvk_codegen/opencl/emit.py` writes the lookups; regenerate); `OpenClBackend` records what each creating call returns and forgets what each release frees
- [X] T006 [P] `clEnqueueReadBuffer` refuses `size > ptr_capacity` and `glReadPixels` refuses a `pixels_bytes` smaller than width × height × the format's bytes per pixel, both as `RVK_UNDECODABLE` before any host call - in the server emitters, regenerated, each with a hostile-input test in `tests/unit/device/executor/`
- [X] T007 [P] `tests/fuzz/fuzz_executor_opencl.cpp` and `tests/fuzz/fuzz_executor_opengl.cpp` with committed corpora in `tests/corpus/`, modelled on `fuzz_executor_vulkan.cpp`: hostile batches into the OpenCL and OpenGL server dispatch over a recording fake backend; the outcome is a refusal, never a host call with an unknown handle
- [X] T008 Write `tests/unit/device/execution/test_transport_command.cpp` first (contracts/transport_nop.md): NOP with the reply flag writes an empty reply for the batch's fence and never reaches the executor library; without the flag writes nothing; a NOP with a body is `command_framing`; another API-0 id is skipped as unknown
- [X] T009 `TransportCommand` (object class) in `src/device/execution/transport_command.{hpp,cpp}`, called by `BatchExecutor` for API 0; `CommandHeader`'s NOP id in `src/proto/submission/`; `docs/protocol.md` defines it under "Command stream"; `tests/qemu/test_virtio_gpu_execution.py` sends a NOP under both the echo and the null executor

- [X] T030 [P] The holes T004-T007 found and did not close, each refused before any host call with a hostile-input test and a fuzz seed: `clSetKernelArg` resolves buffer, image and sampler arguments through `ClHandleRegistry` using the kernel's argument info (the server appends `-cl-kernel-arg-info` to every build and compile; where the ICD cannot answer `CL_KERNEL_ARG_TYPE_NAME`, an 8-byte argument is refused rather than trusted); `clCreateImage*` with copied host pointers checks format × extent against the bytes that crossed; the OpenGL decoder checks count-sized outputs (`glGen*(n, ...)`) against their room, never passes a null output, checks pixel and array uploads against what the driver reads, and refuses buffer offsets followed as addresses with no buffer bound. *As built:* the OpenGL checks cover every pointer role the generator emits, and the generator refuses to emit a pointer it has no rule for. Two things beyond the task: every draw refuses when an enabled array (attribute, fixed-function, any texture unit) has no buffer, since a deleted buffer leaves a client array whatever the pointer command checked, and `glEnableClientState` and friends accept only the arrays that check knows. Refused wholesale: `glFeedbackBuffer` and `glSelectBuffer`, whose driver keeps the pointer. Refused in part, because the size is not established here: pixel transfers of type `GL_BITMAP`, and compressed uploads whose unpack skips move the start with the compressed block parameters set. What remains: a state query whose enum the pname table and the list queries do not know is bounded only by the 32-value slack; the draw-time array check costs a few dozen state queries a draw; OpenCL kernels from binaries or IL, having no argument information, cannot take an 8-byte argument; the generated guest client still cannot send a non-zero unpack-buffer offset for `glBitmap`, `glPolygonStipple`, `glPixelMap*v` or the EXT compressed uploads, and sends nothing for `glIndex*v`, which the registry spells as outputs
- [X] T032 The generated clients distrust the host's replies: a count, length or byte total a reply carries is bounded by the caller's own capacity before anything is written into the caller's array or buffer (`rvk_vkEnumeratePhysicalDevices`, `rvk_clGetPlatformIDs`, `rvk_clGetDeviceIDs`, `rvk_clEnqueueReadBuffer` and every command of their shape), in the client emitters, regenerated, with hostile-reply tests over `MockKmt` *As built:* Vulkan: every count/array pair (the 20 enumerations - handles, structures, scalars) reads the count into a local, refuses one past `*count` with the API's failure value, and writes `*count` only once the elements are read; the arrays and byte outputs sized by an input already read exactly that input. OpenCL: every `handle_array_out` is held to its `num_entries`, every `info_value` to `param_value_size`, every `bytes_out` to its size; a reply cut short anywhere is now `CL_OUT_OF_RESOURCES` (it used to answer the result it began with, and a missing result `-1`, which is `CL_DEVICE_NOT_FOUND`). The OpenGL client already held every output to what the caller asked for. A short reply still leaves the element it stopped inside cleared, which is the reader's contract, never one past it. Tests: `tests/unit/proto/generated/test_generated_client_replies.cpp` with `ScriptedHost::cut_next`, `vk_queue_families` and `cl_info`. What remains: a Vulkan output structure with a caller-provided array inside it (`VkPipelineBinaryHandlesInfoKHR`, `VkPhysicalDeviceHostImageCopyProperties` in a chain) is given a pointer into the transport's decode arena rather than filled - nothing is written past the caller's memory, but the caller's array is not filled; and a GL list query whose length only the host knows (`pname_count(rvk_t, ...)`) has no caller capacity to hold it to, since the API gives none; and no fuzz target feeds the generated clients replies
- [X] T033 The OpenCL server never answers a failed call's output: `decode_clGetDeviceIDs`/`decode_clGetPlatformIDs` and every decoder of their shape write ids only when the call succeeded, and only as many as the call reported, so no uninitialised host memory - host pointers included - reaches the guest *As built:* OpenCL: the ICD is always handed the count or size output of an array, info value or image format list the guest wants, whether or not the guest asked for it, and the reply carries `min(reported, room)` for a call that succeeded and nothing for one that did not (the 4 ID lists, 18 info queries, `clGetSupportedImageFormats` - `size_ret` added to its profile entry - and the 3 reads, which send their bytes only on success); an event or other handle output of a failed call is 0 and never `made`; a scalar output is 0 after a failure unless it is a `cl_int`, which is the call's own error report (`errcode_ret`, `binary_status`). Vulkan, which had the same holes: a count/array pair answers 0 for a failed `VkResult` and never more than the room the guest gave, void enumerations included (a driver that raised the count used to serialize the arena past the array); a failed call's output handle is `VK_NULL_HANDLE` - serializing a stale one registered it with the backend - and its output structures, scalars and input-sized arrays are cleared before they are sent. OpenGL has no failed call to gate on and clears every output before the driver writes it. Tests: `test_opencl_dispatch.cpp` over `RecordingClDispatch::answer_next_with`, `test_vulkan_dispatch.cpp` over a driver that fails with a stale pointer left behind
- [X] T036 `tests/fuzz/fuzz_generated_client_replies.cpp` with a committed corpus: hostile reply slots into the generated Vulkan and OpenCL client functions over a scripted transport (the shapes T032 bounded - counted arrays, info queries, byte outputs, short replies), the caller's buffers in guard bands; the outcome is a failed call or a result within capacity, never a write past the caller's memory (constitution IV.3) *As built:* an input is a call (22 of them: the Vulkan handle, structure and scalar enumerations, device groups, memory properties and requirements, two creates, T037's pipeline binaries and chained host-image-copy, layered-API and DRM-modifier lists; OpenCL platform and device IDs, device, platform and build-log info, a blocking read, image formats, kernels, two creates), the room the caller gives, whether it gives the array and asks for the count, and the raw reply, over `MockTransport`. It traps on a disturbed guard band, a Vulkan count read back past the room given or an output structure's array pointer changed, and an info query that succeeded reporting a size past its buffer; 68 seeds from the generated serializers, each shape valid, truncated and over its room. What it found, each fixed in its emitter with a test in `test_generated_client_replies.cpp`: an OpenCL info query answered success with `param_value_size_ret` past `param_value_size` (all 18 info queries now fail with `CL_OUT_OF_RESOURCES` and leave the size unwritten); `VkPhysicalDeviceMemoryProperties::memoryTypeCount` read past `VK_MAX_MEMORY_TYPES`, and the same shape in `memoryHeapCount`, `VkPhysicalDeviceGroupProperties::physicalDeviceCount` and `VkQueueFamilyGlobalPriorityProperties::priorityCount` (a count beside a fixed array is refused past the array before it replaces the destination's, on both sides of the wire); and, found by the same target's name check, a `null-terminated` fixed name (`extensionName`, `deviceName`, `driverName`, ...) filling its array now has its last byte terminated. Clean for 240 s after the fixes; the smoke run is 60 s. Not judged: an OpenCL string info value is not terminated by the client, which does not know which `param_name` values are strings; a host rewriting the `sType` of a structure in the caller's chain is decoded as the node the caller had, with the new `sType` left in it
- [X] T037 Vulkan output structures carrying a caller-supplied array (`VkPipelineBinaryHandlesInfoKHR`, `VkPhysicalDeviceHostImageCopyProperties` in a chain) fill the caller's array up to its stated capacity instead of redirecting its pointer into the transport's decode arena *As built:* the serialize emitter finds the shape itself - every struct a reply decodes into (output parameters and array elements, what they hold, and the chain nodes extending them; vk.xml's `returnedonly` misses three of them) with a non-const handle, scalar or structure array sized by a `uint32_t` count of its own. Five in this profile: `VkPipelineBinaryHandlesInfoKHR`, `VkPhysicalDeviceHostImageCopyProperties` (two arrays), `VkPhysicalDeviceLayeredApiPropertiesListKHR`, `VkDrmFormatModifierPropertiesListEXT` and `...List2EXT`. Their decoder holds the answered count aside; a non-null pointer is the caller's array, filled in place, and a count past its room - or a count with no elements behind it - is refused with the count and array left as the caller set them (the void `vkGetPhysicalDeviceProperties2` has no error to return, so leaving the count is what keeps an application inside its array); a null pointer is the query, and the server's zeroed storage still allocates as before. The top-level `vkCreatePipelineBinariesKHR` request never carried the room at all, so the driver was always handed a null array: the client now sends the count and a presence byte (not the elements - uninitialised handles are numbers the host refuses) and the server gives the driver an array of that room. Tests: `test_generated_client_replies.cpp` through the generated server dispatch (`ServedTransport`, `ArrayFillingDriver`) and hostile replies over `MockTransport`. Not this shape: `VkPipelineCreationFeedbackCreateInfo` hangs an output array off an input create info, and no reply carries an input structure back
- [ ] T034 The executor shows the guest only the GPU the device is bound to: `vkEnumeratePhysicalDevices` answers the physical device whose UUID is the capset's, and `clGetPlatformIDs`/`clGetDeviceIDs` only the platform and device whose `CL_DEVICE_UUID_KHR` is it. The first live run's OpenCL scenario took the first GPU of any platform - the NVIDIA card, while QEMU was bound to the Intel one - and its ICD answered `CL_OUT_OF_RESOURCES` inside QEMU; several GPUs to one guest is feature 060's
- [ ] T035 A context's backend is opened when the host context is set up, not on its first batch, so the first command a process sends is not the one that waits for an EGL or ICD start-up: the first live run measured an OpenGL no-op at 3.09 s under host load, past dxgkrnl's two-second TdrDelay, and a timeout reset is the likeliest cause of that run's `isolation.healthy_after` 0xC01E0200 (STATUS_GRAPHICS_GPU_EXCEPTION_ON_DEVICE); a run without the load passed 45 of 45
- [X] T031 [P] `VulkanBackend` forgets every handle a destroy or free call ends, not only instances and devices; `OpenClBackend::shut_down` releases the ICD objects it still holds, as its comment says. *As built:* the generated decoder reports every `vkDestroy*`/`vkFree*` (and each element of the two list frees) and `vkResetDescriptorPool`; `HandleRegistry` records the parent each handle was made through, so a device, instance or pool takes its children with it. `ClHandleRegistry::release_all` gives each held reference back, events first and devices last

**Checkpoint**: the executor refuses what it does not know, and a NOP round-trips on the device. Commit.

---

## Phase 2b: The full-stack loopback rig (requested 2026-09-14)

**Purpose**: the guest's whole path - the in-guest suite, the real driver through its DDI table, the real device core and executor - in one Linux process, so the rvk scenarios below are proven in seconds before a guest boot confirms them.

- [X] T026 The full-stack rig in `tests/harness/fullstack/`: the PCI function's virtqueues and doorbells served by the real device core (`LoopbackDevice`), interrupts delivered through `FakeDxgkrnlFdo`, and `DdiKmt` - an `av::Kmt` that calls the registered DDI table the way dxgkrnl's D3DKMT layer does (handles, aperture paging, GPU VAs, submission fences and private data, waits on DMA_COMPLETED)
- [X] T027 `tests/loopback/test_fullstack_*.cpp`: the adapter starts on the device core; 043's transport round trip through the real driver; `GuestSuite` over `DdiKmt`, every step that can pass passing and each that cannot named with its reason; a faulted context beside a healthy one; nothing leaked
- [X] T028 [US1] Every guest scenario in Phases 3-4 runs over the rig with `MockExecutorOps`, and with `build/librvk_executor.so` on this machine's GPU when `AV_REAL_EXECUTOR` names it *As built: `FullStackRig(executor table, host GPU)` realizes the device core on any `RvkExecutorOps`; `tests/loopback/test_fullstack_rvk.cpp` links `src/device/shim/executor_ops.cpp` and hands it `rvk_executor_ops()` - the library's own root in process, not a dlopen - bound to `WidestHostGpu`, the GPU the host's loaders say runs the most sub-APIs. The Vulkan, OpenCL and isolation scenarios and the whole `run_rvk` pass there on this machine (Intel Iris Xe, sub-APIs 0x7); a host without a sub-API returns without judging, as the executor's GPU tests do. Over `MockExecutorOps` only the transport no-op means anything - the mock echoes every payload and offers Vulkan alone - so that one stays in `test_fullstack_suite.cpp`.*
- [X] T029 A quick guest mode. *As built:* the long part of a run is not the reboots but the display, mode, power, cycle, sleep and hibernate scenarios (the monitor timeout alone waits minutes), so `run_guest_suite.py --quick` runs install, check and the suite without them, and `--rvk` skips them too; the gate still runs everything. A cached installed-driver disk layer is not built: the prepared image already has test signing, so it would save one install reboot

## Phase 3: US1 - A context per sub-API runs real host work (Priority: P1) 🎯 MVP

**Goal**: Vulkan, OpenCL and OpenGL contexts do real work on the host GPU from a guest process.

**Independent test**: `rvktest.exe rvk` against a guest whose device loads `librvk_executor.so`: every `transport.*`, `vulkan.*`, `opencl.*` and `opengl.*` step is ok.

- [X] T010 [US1] `TransportScenario` sends the NOP instead of the echo command (`src/guest/suite/transport_scenario.{hpp,cpp}`): the reply is empty; the step reports the round trip in microseconds as a `latency` measurement never judged; `MockKmt`'s scripted answer and `tests/unit/guest/suite/test_transport_scenario.cpp` follow
- [X] T011 [P] [US1] Write `tests/unit/guest/suite/test_vulkan_scenario.cpp` first over `MockKmt` answering the generated client's replies: the steps instance, physical_device, device, buffer, memory, bind, destroy each pass on `VK_SUCCESS`; a failure at each is reported and the rest skipped; everything created is destroyed in reverse; zero physical devices fails physical_device
- [X] T012 [US1] `VulkanScenario` (object class) in `src/guest/suite/vulkan_scenario.{hpp,cpp}`: `rvk_vkCreateInstance`, `rvk_vkEnumeratePhysicalDevices`, `rvk_vkCreateDevice` (one queue), `rvk_vkCreateBuffer` (4 KiB, `TRANSFER_DST`), `rvk_vkGetBufferMemoryRequirements`, `rvk_vkAllocateMemory` (the lowest set bit of `memoryTypeBits`), `rvk_vkBindBufferMemory`, then destroy in reverse, over a `KmtTransport` of `SubApi::vulkan`
- [X] T013 [P] [US1] Write `tests/unit/guest/suite/test_opencl_scenario.cpp` first over `MockKmt`: the steps platform, device, context, queue, buffers, program, kernel, run, read, sum; a wrong sum fails sum and reports how many elements matched; a failure at each step is reported and released in reverse *As built: a `release` step follows `sum`, reporting the first error a release answered; the device is the first GPU any platform offers, since the host lists its CPU runtime too.*
- [X] T014 [US1] `OpenClScenario` (object class) in `src/guest/suite/opencl_scenario.{hpp,cpp}`: vector add of 64 `float`s over `CL_MEM_COPY_HOST_PTR` input buffers and a write-only output, the kernel source a named constant under the 1024-byte command limit, the result read back inline with `clEnqueueReadBuffer` (blocking) and compared exactly
- [X] T015 [P] [US1] *As built: `TransportScenario` takes the sub-API and the report's name, so the OpenGL check is `TransportScenario(..., SubApi::opengl, "opengl")` rather than a near-copy class.* Originally `OpenGlScenario` in `src/guest/suite/opengl_scenario.{hpp,cpp}` with its test first: a `KmtTransport` of `SubApi::opengl` initializes (the host opened its EGL context), sends a NOP, tears down
- [X] T016 [US1] *As built so far: `GuestSuite::run_rvk` (adapter, transport, OpenGL) and `rvktest.exe rvk`; Vulkan, OpenCL and isolation join it as they land.* Originally `RvkSuite` (system class) in `src/guest/suite/rvk_suite.{hpp,cpp}` with its test first: finds the adapter, runs transport, Vulkan, OpenCL, OpenGL and (after US2) isolation; a sub-API the capset refuses - the context's setup refused - is reported `skipped` by name rather than failed (spec edge case); `src/um/guest_program.cpp` runs it for `rvktest.exe rvk` *Done: `run_rvk` runs adapter, transport, vulkan, opencl, opengl, isolation; every rvk scenario opens its context through `ScenarioTransport` (adapter, connection, initialize).*
- [X] T017 [US1] `tools/guest/run_guest_suite.py`: `--rvk` runs `rvktest.exe rvk` instead of the suite; `verdict.txt` records the executor library loaded and the device's `host-gpu`; tests in `tests/tools/test_run_guest_suite.py` against the fake guest

**Checkpoint**: the rvk scenarios pass over MockKmt and the runner can ask for them. Commit.

---

## Phase 4: US2 - A faulted context recovers alone (Priority: P1)

**Goal**: a malformed stream faults only its own context; a new link works; the adapter stays started.

- [X] T018 [US2] Write `tests/unit/guest/suite/test_isolation_scenario.cpp` first over `MockKmt` scripting a faulted wait on one context: healthy_before, fault, faulted_send, healthy_after, relink; the faulted context's sends fail, the healthy one's pass
- [X] T019 [US2] `IsolationScenario` (object class) in `src/guest/suite/isolation_scenario.{hpp,cpp}`: two Vulkan transports; the second sends `vkCreateInstance` with a truncated body (research §6), which the host answers undecodable and the driver completes with no reply, refusing its later submissions (not `DMA_FAULTED`, which dxgmms2.sys `VidSchDdiNotifyInterruptWorker` @140031b80 bugchecks 0x119/0xD on `DXGK_INTERRUPT_DMA_FAULTED` from a GPU MMU adapter); its next send fails; the first context's NOP and `vkCreateInstance` pass before and after; a third transport made afterwards passes a NOP. Added to `RvkSuite` *As built: `faulted_send` is judged by the transport, which refuses every send once a wait failed; `relink` is the no-op on a third context.*
- [X] T020 [US2] US2.3 (TDR on one device) stays 040's `tdr` scenario with the echo library's stall; `specs/044-rvk-context-live/spec.md` says so beside the scenario

**Checkpoint**: fault isolation is proven over MockKmt. Commit.

---

## Phase 5: US3 - The run proves it with numbers (Priority: P2)

- [X] T021 [US3] The report carries `transport.latency` (T010) and the runner copies it into `verdict.txt` with the executor and GPU; the regression threshold is set from the first passing live run and recorded in `docs/testing.md` (SC-002)

---

## Phase 6: The guest proof and the merge

- [X] T022 `tools/gate.sh`: when `AV_GUEST_IMAGE` is set and `build-qemu/librvk_executor.so` exists, a second guest boot runs `--rvk --executor build-qemu/librvk_executor.so` after the suite
- [X] T023 The moved scope is recorded where it lands: `specs/045-vulkan-icd/spec.md` (the host-visible pattern round trip), `specs/050-opengl-icd/spec.md` (clear and read-back), `specs/052-opencl-icd/spec.md` (host-visible buffers); `specs/044-rvk-context-live/spec.md` marks US1.2-US1.4 and US3.1 with what moved
- [X] T024 `docs/testing.md` "As built" describes the rvk scenarios, the NOP and the second boot; `docs/protocol.md` the NOP
- [ ] T025 `AV_GUEST_IMAGE=... tools/gate.sh` passes with both boots; merge to `master`; push

---

## Dependencies & Execution Order

- Phase 1 blocks US1's Vulkan and OpenCL scenarios (T011-T014); T010 and T015 need only Phase 2's NOP.
- Phase 2's hardening (T004-T007) must be in before any live run with the real library (T025).
- US2 needs US1's transports; US3 needs T010.

### Parallel opportunities

- T001, T004-T007 and T008-T009 touch disjoint directories (`tools/`, `src/device/executor` + server emitters, `src/device/execution`) and can run as separate builders in separate worktrees.
- T011/T013/T015 are independent test files once Phase 1 lands.

## Implementation Strategy

**MVP**: Phases 1-3 - a real Vulkan and OpenCL stream from a guest process runs on the host GPU. **Then** US2's isolation, US3's numbers, and the gate's second boot.
