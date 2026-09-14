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

**Checkpoint**: the executor refuses what it does not know, and a NOP round-trips on the device. Commit.

---

## Phase 2b: The full-stack loopback rig (requested 2026-09-14)

**Purpose**: the guest's whole path - the in-guest suite, the real driver through its DDI table, the real device core and executor - in one Linux process, so the rvk scenarios below are proven in seconds before a guest boot confirms them.

- [X] T026 The full-stack rig in `tests/harness/fullstack/`: the PCI function's virtqueues and doorbells served by the real device core (`LoopbackDevice`), interrupts delivered through `FakeDxgkrnlFdo`, and `DdiKmt` - an `av::Kmt` that calls the registered DDI table the way dxgkrnl's D3DKMT layer does (handles, aperture paging, GPU VAs, submission fences and private data, waits on DMA_COMPLETED)
- [X] T027 `tests/loopback/test_fullstack_*.cpp`: the adapter starts on the device core; 043's transport round trip through the real driver; `GuestSuite` over `DdiKmt`, every step that can pass passing and each that cannot named with its reason; a faulted context beside a healthy one; nothing leaked
- [ ] T028 Every guest scenario in Phases 3-4 runs over the rig with `MockExecutorOps`, and with `build/librvk_executor.so` on this machine's GPU when `AV_REAL_EXECUTOR` names it
- [X] T029 A quick guest mode. *As built:* the long part of a run is not the reboots but the display, mode, power, cycle, sleep and hibernate scenarios (the monitor timeout alone waits minutes), so `run_guest_suite.py --quick` runs install, check and the suite without them, and `--rvk` skips them too; the gate still runs everything. A cached installed-driver disk layer is not built: the prepared image already has test signing, so it would save one install reboot

## Phase 3: US1 - A context per sub-API runs real host work (Priority: P1) 🎯 MVP

**Goal**: Vulkan, OpenCL and OpenGL contexts do real work on the host GPU from a guest process.

**Independent test**: `rvktest.exe rvk` against a guest whose device loads `librvk_executor.so`: every `transport.*`, `vulkan.*`, `opencl.*` and `opengl.*` step is ok.

- [X] T010 [US1] `TransportScenario` sends the NOP instead of the echo command (`src/guest/suite/transport_scenario.{hpp,cpp}`): the reply is empty; the step reports the round trip in microseconds as a `latency` measurement never judged; `MockKmt`'s scripted answer and `tests/unit/guest/suite/test_transport_scenario.cpp` follow
- [ ] T011 [P] [US1] Write `tests/unit/guest/suite/test_vulkan_scenario.cpp` first over `MockKmt` answering the generated client's replies: the steps instance, physical_device, device, buffer, memory, bind, destroy each pass on `VK_SUCCESS`; a failure at each is reported and the rest skipped; everything created is destroyed in reverse; zero physical devices fails physical_device
- [ ] T012 [US1] `VulkanScenario` (object class) in `src/guest/suite/vulkan_scenario.{hpp,cpp}`: `rvk_vkCreateInstance`, `rvk_vkEnumeratePhysicalDevices`, `rvk_vkCreateDevice` (one queue), `rvk_vkCreateBuffer` (4 KiB, `TRANSFER_DST`), `rvk_vkGetBufferMemoryRequirements`, `rvk_vkAllocateMemory` (the lowest set bit of `memoryTypeBits`), `rvk_vkBindBufferMemory`, then destroy in reverse, over a `KmtTransport` of `SubApi::vulkan`
- [ ] T013 [P] [US1] Write `tests/unit/guest/suite/test_opencl_scenario.cpp` first over `MockKmt`: the steps platform, device, context, queue, buffers, program, kernel, run, read, sum; a wrong sum fails sum and reports how many elements matched; a failure at each step is reported and released in reverse
- [ ] T014 [US1] `OpenClScenario` (object class) in `src/guest/suite/opencl_scenario.{hpp,cpp}`: vector add of 64 `float`s over `CL_MEM_COPY_HOST_PTR` input buffers and a write-only output, the kernel source a named constant under the 1024-byte command limit, the result read back inline with `clEnqueueReadBuffer` (blocking) and compared exactly
- [X] T015 [P] [US1] *As built: `TransportScenario` takes the sub-API and the report's name, so the OpenGL check is `TransportScenario(..., SubApi::opengl, "opengl")` rather than a near-copy class.* Originally `OpenGlScenario` in `src/guest/suite/opengl_scenario.{hpp,cpp}` with its test first: a `KmtTransport` of `SubApi::opengl` initializes (the host opened its EGL context), sends a NOP, tears down
- [ ] T016 [US1] *As built so far: `GuestSuite::run_rvk` (adapter, transport, OpenGL) and `rvktest.exe rvk`; Vulkan, OpenCL and isolation join it as they land.* Originally `RvkSuite` (system class) in `src/guest/suite/rvk_suite.{hpp,cpp}` with its test first: finds the adapter, runs transport, Vulkan, OpenCL, OpenGL and (after US2) isolation; a sub-API the capset refuses - the context's setup refused - is reported `skipped` by name rather than failed (spec edge case); `src/um/guest_program.cpp` runs it for `rvktest.exe rvk`
- [ ] T017 [US1] `tools/guest/run_guest_suite.py`: `--rvk` runs `rvktest.exe rvk` instead of the suite; `verdict.txt` records the executor library loaded and the device's `host-gpu`; tests in `tests/tools/test_run_guest_suite.py` against the fake guest

**Checkpoint**: the rvk scenarios pass over MockKmt and the runner can ask for them. Commit.

---

## Phase 4: US2 - A faulted context recovers alone (Priority: P1)

**Goal**: a malformed stream faults only its own context; a new link works; the adapter stays started.

- [ ] T018 [US2] Write `tests/unit/guest/suite/test_isolation_scenario.cpp` first over `MockKmt` scripting a faulted wait on one context: healthy_before, fault, faulted_send, healthy_after, relink; the faulted context's sends fail, the healthy one's pass
- [ ] T019 [US2] `IsolationScenario` (object class) in `src/guest/suite/isolation_scenario.{hpp,cpp}`: two Vulkan transports; the second sends `vkCreateInstance` with a truncated body (research §6), which the host answers undecodable and the driver reports `DMA_FAULTED`; its next send fails; the first context's NOP and `vkCreateInstance` pass before and after; a third transport made afterwards passes a NOP. Added to `RvkSuite`
- [X] T020 [US2] US2.3 (TDR on one device) stays 040's `tdr` scenario with the echo library's stall; `specs/044-rvk-context-live/spec.md` says so beside the scenario

**Checkpoint**: fault isolation is proven over MockKmt. Commit.

---

## Phase 5: US3 - The run proves it with numbers (Priority: P2)

- [ ] T021 [US3] The report carries `transport.latency` (T010) and the runner copies it into `verdict.txt` with the executor and GPU; the regression threshold is set from the first passing live run and recorded in `docs/testing.md` (SC-002)

---

## Phase 6: The guest proof and the merge

- [ ] T022 `tools/gate.sh`: when `AV_GUEST_IMAGE` is set and `build-qemu/librvk_executor.so` exists, a second guest boot runs `--rvk --executor build-qemu/librvk_executor.so` after the suite
- [X] T023 The moved scope is recorded where it lands: `specs/045-vulkan-icd/spec.md` (the host-visible pattern round trip), `specs/050-opengl-icd/spec.md` (clear and read-back), `specs/052-opencl-icd/spec.md` (host-visible buffers); `specs/044-rvk-context-live/spec.md` marks US1.2-US1.4 and US3.1 with what moved
- [ ] T024 `docs/testing.md` "As built" describes the rvk scenarios, the NOP and the second boot; `docs/protocol.md` the NOP
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
