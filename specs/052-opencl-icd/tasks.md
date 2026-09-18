---

description: "Tasks: OpenCL - the ICD on rvkgpu"
---

# Tasks: OpenCL - the ICD on rvkgpu

**Input**: Design documents from `/specs/052-opencl-icd/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/icd_interface.md, contracts/wire_additions.md, quickstart.md

**Tests**: Required, and written first (constitution IV). Every new class gets its test before its body, `invariants_hold()` after every mutation, and a hostile-input table plus a fuzz target for anything reading bytes it did not write.

**Organization**: setup, then the foundation nothing works without (the generator's two new shapes, the regenerated wire, the thread seam, the fake loader), then one phase per user story, then the guest proof and the gates. Each `**Checkpoint**` is a commit once `ctest --test-dir build -j32 -LE tools` passes there.

**Branch**: `052-opencl-icd` from `master`. It merges fast-forward once `tools/gate.sh` passes.

## Format: `[ID] [P?] [Story] Description`

- **[Story]**: US1 (the Khronos loader finds rvkgpu's OpenCL device), US2 (buffers, maps and SVM), US3 (events and transfers never deadlock)

---

## Phase 1: Setup

- [ ] T001 [P] `src/proto/icd_stub_report.hpp` declares `void stub_called(const char *api, const char *entry_point, const char *why) noexcept;` beside `payload_dropped`, with the comment saying who defines it; `tests/unit/proto/test_icd_stub_report.cpp` proves a translation unit that includes `src/proto/generated/opencl/cl_icd_stubs.h` compiles and links against a test definition. `cl_icd_stubs.h` has never been compiled by anything (research §3)
- [ ] T002 [P] `tools/opencl_loader_pins.txt` pins the Khronos ICD loader source archive `https://github.com/KhronosGroup/OpenCL-ICD-Loader/archive/refs/tags/v2026.05.29.tar.gz` by SHA-256 `48fd0c5181db7cd046f4f731d5955694892e10998d49d09ee0d997e7e04fd939`, in `tools/fetch_vulkan_runtime.py`'s grammar (`archive <url> <sha256>`, then one line per wanted file); the header says why the URL names a tag and never a branch
- [ ] T003 `tools/fetch_opencl_loader.py` fetches and verifies that archive into `third_party/opencl-loader/` with `--check`, mirroring `tools/fetch_vulkan_runtime.py` exactly: `Refused` as the one failure channel, the archive cached under `third_party/.cache/`, every member hashed before the first is written, nothing written on any mismatch. Khronos publishes no binary assets for this loader (research §2), so what is pinned is source
- [ ] T004 `tests/tools/test_fetch_opencl_loader.py` drives it over a `file://` archive built in memory, as `tests/tools/test_fetch_vulkan_runtime.py` does: clean fetch, `--check` both ways, tamper, re-fetch from cache, archive-hash mismatch writing nothing and emptying the cache, malformed pins in each of its four shapes, and a case reading the repository's real pins file to assert the URL names a version and every digest is 64 hex characters
- [ ] T005 `cmake/` cross-builds `OpenCL.dll` for the guest from the fetched source with `clang-cl`/`lld-link` — `loader/{icd,icd_dispatch,icd_dispatch_generated,icd_trace}.c` and `loader/windows/*.c`, exports through `loader/windows/OpenCL.def`, headers from `third_party/khronos/include` — as a test artifact into `build-guest/`, never into the driver package. Skipped with a printed reason when the loader has not been fetched
- [ ] T006 [P] `CMakeLists.txt` adds the `av_cl_generated` OBJECT library over `src/proto/generated/opencl/cl_dispatch_client.cpp` and the `av_cl_icd` STATIC library over `src/icd/common/*.cpp src/icd/opencl/*.cpp`, both with `third_party/khronos/include` on the include path (the host build carries it nowhere today), and a `/tests/unit/icd/opencl/` arm **before** the `/tests/unit/icd/` catch-all in both test-linking sites
- [ ] T007 [P] `src/um/CMakeLists.txt` adds the `rvkcl` SHARED target (`PREFIX ""`, `/ENTRY:rvkcl_entry`, `/DEF:opencl/rvkcl.def`, imports gdi32/kernel32/ntdll only), `list(REMOVE_ITEM ... ${RVKTEST_GENERATED})` so the recursive proto glob does not sweep `cl_dispatch_server.cpp` into a guest DLL, `opencl` added to the `um/(icd|vulkan|opengl|probe|inspect)/` exclude regex, and `rvkcl.dll`/`rvkclprobe.exe` in the top-level `BUILD_BYPRODUCTS`

**Checkpoint** — the loader fetches, verifies and cross-builds; `cl_icd_stubs.h` compiles; the new targets configure.

---

## Phase 2: Foundational — the wire, the seam, the fake loader

**Blocking**: every user story needs the regenerated wire and the fake loader.

- [ ] T008 [P] `tests/tools/test_cl_emit.py` asserts, before any generator change, what the emitter must produce for the two new shapes: a `void *`-returning command encodes its result `write_pod<uint64_t>`, returns `nullptr` on failure and judges success as `ret != nullptr`; an SVM-pointer parameter declares `uint64_t` on the client, `void *` on the server, and decodes through `svm_for`. Built in memory as `tests/tools/test_client_call_scope.py:78` builds its registry. These fail first
- [ ] T009 `tools/codegen/rvk_codegen/opencl/model.py` and `emit.py` gain the address return: a third classification beside `returns_handle`/`returns_void`, with `_succeeded`, `_client_failure`, `_decode_reply`, the server's reply write and `_default_body` each given their arm. Without it the emitter writes `return CL_OUT_OF_RESOURCES;` from a `void *` function and reports a failed map as success (research §5.1)
- [ ] T010 `tools/codegen/rvk_codegen/opencl/{model,lower,emit}.py` gain `Role.SVM_POINTER`: `uint64_t` on the client, `write_pod<uint64_t>` on the wire, `void *` on the server resolved through a new `ServerDispatch` virtual `void *svm_for(uint64_t address, uint64_t bytes) const`, with the profile spelling `role = "svm_pointer"` and `len` naming the size parameter so the extent refusal is generated. `EXEC_INFO_POINTERS`'s refusal of `CL_KERNEL_EXEC_INFO_SVM_PTRS` is retired with it
- [ ] T011 `tools/codegen/rvk_codegen/opencl/emit.py` gains the `clSetEventCallback` hook: the guest's function pointer and user data do not cross (the lowering already drops them), and the server's generated call reaches a per-command hook instead of passing `nullptr, nullptr`, which the API refuses with `CL_INVALID_VALUE` (research §5.3)
- [ ] T012 `tools/codegen/profiles/opencl.toml` adds the ten commands of contracts/wire_additions.md in that order with their annotations — `clEnqueueSVMMemFill.pattern` is `bytes_in` with `len = "pattern_size"`, each SVM pointer is `svm_pointer` with its size parameter — and the tree is regenerated into `src/proto/generated/opencl/` with `tools/codegen/profiles/opencl_ids.toml` taking ids `0x2006A`-`0x20073`. The regenerated sources, the ids file and `docs/protocol.md` are one commit
- [ ] T013 [P] `tests/unit/device/executor/test_opencl_dispatch.cpp` and `tests/harness/executor/recording_cl_dispatch.{hpp,cpp}` learn the ten commands: a stranger handle, a stray span, an SVM address outside any live range, an SVM extent that overflows, and a truncated frame at every field boundary are each refused, and the existing `CL_KERNEL_EXEC_INFO_SVM_PTRS` refusal test is rewritten to prove it is now carried
- [ ] T014 [P] `src/icd/common/service_thread.hpp` declares the `icd::ServiceThread` seam (`start(Runnable &)`, `stop()`, `running()`), with a protected non-virtual destructor; `src/um/icd/windows_service_thread.{hpp,cpp}` implements it over `CreateThread` and a stop event; `tests/harness/icd/scripted_thread.{hpp,cpp}` runs the loop on the test's own thread when the test steps it
- [ ] T015 `tests/unit/icd/common/test_service_thread.cpp` proves the scripted thread's contract: a loop that is stepped runs once per step, `stop()` ends it, a second `start()` while running is refused, and nothing runs after `stop()`
- [ ] T016 `tests/harness/icd/fake_cl_loader.{hpp,cpp}` is the Khronos loader's side: it calls the ICD's `clGetExtensionFunctionAddress`, asks it for `clIcdGetPlatformIDsKHR`, enumerates in two calls, reads the `cl_icd_dispatch *` out of the first word of every object it is handed and **dispatches through the table**, and carries the refusal levers `FakeOpengl32` carries (`hide_export`, a `Load` enum). A slot left null or filled with the wrong arity fails a test rather than a guest
- [ ] T017 `tests/harness/icd/cl_icd_rig.{hpp,cpp}` owns `MockPlatform`, `MockKmt`, `ScriptedHost`, `LockTrace`, two `RecordingThreadLock`s (`'H'`, `'S'`), `RecordingLog`, `ScriptedThread`, `FakeClLoader` and `ClIcdRoot`, adds the rvkgpu adapter and sets a LUID whose halves differ; `tests/harness/icd/cl_object_rig.{hpp,cpp}` is the sub-root rig with a `link_objects` constant

**Checkpoint** — the wire carries 115 commands, the generator tests pass, the seam and the fake loader exist.

---

## Phase 3 (US1): The Khronos loader finds rvkgpu's OpenCL device

**Goal**: an application calling `clGetPlatformIDs` through the system loader finds one device per rvkgpu adapter, and runs a kernel on it.

**Independent test**: `FakeClLoader` over `MockKmt` enumerates, queries and runs; `rvkclprobe.exe` does the same in the guest through the real `OpenCL.dll`.

- [ ] T018 [P] [US1] `tests/unit/icd/opencl/test_cl_dispatch_table.cpp` then `src/icd/opencl/cl_dispatch_table.{hpp,cpp}`: the table is zeroed, this ICD's entries written, `av::clproto::fill_unimplemented` called last; no slot this ICD could be dispatched through is null afterwards, an entry already written is not overwritten, and a refused slot reports once through `stub_called`. The test must not assert "no slot is null" — on a Linux host build the D3D/DX9 slots are `void *` and stay null (research §3)
- [ ] T019 [P] [US1] `tests/unit/icd/opencl/test_cl_platform.cpp` then `src/icd/opencl/cl_platform.{hpp,cpp}`: `CL_PLATFORM_ICD_SUFFIX_KHR` answers `RVK`, `CL_PLATFORM_PROFILE` `FULL_PROFILE`, and both `CL_PLATFORM_EXTENSIONS` calls succeed — **with no adapter opened**, asserted by `kmt().calls(Call::enumerate_adapters) == 0`. A size too small is `CL_INVALID_VALUE`; a null `param_value` with a size ret answers the size; a query the platform does not carry is `CL_INVALID_VALUE`
- [ ] T020 [P] [US1] `tests/unit/icd/opencl/test_cl_object_table.cpp` then `src/icd/opencl/{cl_object.hpp,cl_object_table.{hpp,cpp}}`: every object carries the dispatch pointer in its first word; `made(pointer, kind)` refuses a stack address, a freed object, an object of another kind and a forged one; destroying a context destroys its subtree at any depth; wire handle 0 is never wrapped; the 257th object of a full pool is `CL_OUT_OF_HOST_MEMORY` and the slot is reusable after a release. Probe struct corrupts the generation and `invariants_hold()` catches it
- [ ] T021 [US1] `tests/unit/icd/opencl/test_cl_connection.cpp` then `src/icd/opencl/cl_connection.{hpp,cpp}`: `acquire` opens on the first user and reuses after; `release` closes the device with its last user and the adapter with its last device; `lose` retires a link while the contexts on it keep theirs; a fifth lost-and-held link is `no_space` with nothing opened; no rvkgpu adapter is `not_supported`. `kmt().open_objects()` is 0 at the end of every case
- [ ] T022 [US1] `tests/unit/icd/opencl/test_guest_cl_context.cpp` then `src/icd/opencl/guest_cl_context.{hpp,cpp}`: nothing reaches the device before the context is made; a make that fails holds nothing and leaves it `created`; `wire()` answers null unless made and not lost, and a faulted transport makes it lost there; the record's reserved tail is zero, because it is OpenGL's and `KmtTransport` refuses a non-default one on this sub-API
- [ ] T023 [P] [US1] `tests/unit/icd/opencl/test_cl_extension_policy.cpp` then `src/icd/opencl/cl_extension_policy.{hpp,cpp}`: `using Row = icd::ExtensionPolicy::Row;` and matching over `CL_DEVICE_EXTENSIONS`' single space-separated string, as `GlExtensionPolicy` does over GL's. An extension is reported only when a row names it and the host reports it; `cl_khr_gl_sharing` has no row until 053; a host string longer than the buffer, one with no separators and an empty one are each judged
- [ ] T024 [P] [US1] `tests/unit/icd/opencl/test_cl_device_report.cpp` then `src/icd/opencl/cl_device_report.{hpp,cpp}`: `CL_DEVICE_VENDOR_ID` and `CL_DEVICE_LUID_KHR` are the rvkgpu adapter's, `CL_DEVICE_VERSION` is the host's capped at `OpenCL 3.0`, `CL_DEVICE_SVM_CAPABILITIES` is coarse-grain-only or 0, and every other `CL_DEVICE_*` is forwarded byte for byte. A host answer cut short (`ScriptedHost::cut_next`) is refused, not truncated into an answer
- [ ] T025 [US1] `tests/unit/icd/opencl/test_cl_icd_root.cpp` then `src/icd/opencl/{cl_icd_root.{hpp,cpp},cl_icd_binding.{hpp,cpp},cl_hooks.cpp}`: the root binds itself in its constructor and unbinds in its destructor; `clIcdGetPlatformIDsKHR` answers in the loader's two-call shape; `clGetExtensionFunctionAddress` answers `clIcdGetPlatformIDsKHR` and null for what it does not serve; `stub_called` and `payload_dropped` are defined here, one line into the log
- [ ] T026 [US1] `tests/unit/icd/opencl/test_cl_icd_loader_sequence.cpp` drives the whole of contracts/icd_interface.md's seven-step table through `FakeClLoader`, and proves step 7's rule: a platform whose `CL_PLATFORM_ICD_SUFFIX_KHR` fails is dropped by the loader. Also proves steps 3-7 open no D3DKMT adapter, device or context
- [ ] T027 [US1] `src/um/opencl/{rvkcl_exports.cpp,rvkcl.def,cl_icd_library.{hpp,cpp}}`: the `constinit ClIcdLibrary` with `ObjectStorage<Seams>`, lazy construction behind **its own one-shot** — the Khronos loader holds no lock while it scans drivers, unlike the Vulkan loader and opengl32 (research §3) — a `rvkcl_entry` that returns TRUE and does nothing, `_purecall`, `_fltused`, and the crash filter
- [ ] T028 [US1] `tests/unit/icd/opencl/test_cl_icd_library.cpp`: a process with no D3DKMT wait event is refused permanently and the refusal is logged once; the one-shot constructs exactly once under concurrent first calls; every entry answers the API's own refusal when the root is null
- [ ] T029 [P] [US1] `src/guest/suite/{cl_entry.hpp,cl_probe.{hpp,cpp}}` — the probe's scenarios over a pure-virtual `ClEntry` seam, portable and freestanding, one `Status`-returning method per step: platform, device, luid, context, queue, program, kernel, read; `tests/unit/guest/suite/test_cl_probe.cpp` drives it over a scripted entry with the rig's destructor asserting no live object and no misuse
- [ ] T030 [P] [US1] `src/um/probe/cl/{rvkclprobe_entry.cpp,cl_probe_program.{hpp,cpp},windows_cl_entry.{hpp,cpp}}` and `src/um/imports/OpenCL.def`: translation only, the exit code the failure count, the report written to stdout as `rvkclprobe: N passed, M failed`
- [ ] T031 [US1] `tests/loopback/test_fullstack_opencl_icd.cpp` with a `ClProcess` aggregate factored as `test_fullstack_opengl_icd.cpp` factors `GlProcess`: the real ICD over `DdiKmt`, the real driver, device core and executor library, on `WidestHostGpu`. It runs the probe's US1 steps and asserts the ICD hands the application the same `CL_DEVICE_NAME` bytes the executor answers with no guest in between. `__lsan_default_suppressions` names the host CL drivers

**Checkpoint** — US1 is provable on Linux end to end: the loader's sequence, the device's identity, and a kernel that runs.

---

## Phase 4 (US2): Buffers, maps and SVM over host-visible memory

**Goal**: buffers with every `CL_MEM_*` flag, maps that cost no copy where host-visible memory is offered, and coarse-grained SVM.

**Independent test**: loopback tests map and unmap with invariants after each step, and hostile sizes and offsets.

- [ ] T032 [P] [US2] `tests/unit/icd/opencl/test_mapped_region.cpp` then `src/icd/opencl/mapped_region.{hpp,cpp}`: which of the two paths a buffer maps by is decided once per buffer and logged once per context; unmapping a pointer that is not a live mapping of that object is `CL_INVALID_VALUE`; a buffer released with a mapping live keeps the mapping's memory until the unmap; overlapping maps of one buffer are each tracked; the 65th mapping is `CL_OUT_OF_HOST_MEMORY`
- [ ] T033 [US2] `src/icd/opencl/guest_cl_context.cpp` opens a `KmtBlobArena` on the context and backs a mappable buffer from it, naming the offset so the device makes the host buffer over the chunk's host address with `CL_MEM_USE_HOST_PTR`; `tests/unit/icd/opencl/test_cl_buffer_paths.cpp` proves the zero-copy path is taken when the capset offers host-visible memory and the staged path when it does not, and that the log says which
- [ ] T034 [P] [US2] `tests/unit/icd/opencl/test_svm_allocation.cpp` then `src/icd/opencl/svm_allocation.{hpp,cpp}`: the application is handed the mirror; a map fills the mirror from the host and an unmap writes it back; nested maps count; freeing one still mapped is refused; a zero size, a size that overflows when added to the base, and an alignment that is not a power of two are each refused before anything is sent
- [ ] T035 [US2] `tests/unit/icd/opencl/test_svm_table.cpp` then `src/icd/opencl/svm_table.{hpp,cpp}`: `host_address_of(pointer)` answers host base plus the same offset for a pointer inside a live allocation and null for one outside, for one in a freed allocation, for the byte just past the end, and for a pointer the application invented; the 257th allocation is refused. This is the only address translation in the design
- [ ] T036 [US2] `src/icd/opencl/cl_icd_root.cpp` answers `clSVMAlloc`, `clSVMFree`, `clEnqueueSVMMap/Unmap/Memcpy/MemFill` and `clSetKernelArgSVMPointer`, translating every address through `SvmTable` before it is encoded, so the wire carries host addresses only; `CL_DEVICE_SVM_CAPABILITIES` reports coarse-grain-buffer alone and only when the host device does
- [ ] T037 [P] [US2] `tests/unit/device/executor/test_host_svm_ranges.cpp` then `src/device/executor/host_svm_ranges.{hpp,cpp}`: `contains(address, bytes)` is a bounds check with the addition overflow-checked and never a dereference; a zero-length extent at a live base, an extent one byte past the end, an address in a freed range and an address in no range are each refused; `shut_down` forgets every range
- [ ] T038 [US2] `src/device/executor/opencl_backend.cpp` implements `svm_for` over `HostSvmRanges`, registers a range at `clSVMAlloc` and forgets it at `clSVMFree`; `tests/unit/device/executor/test_opencl_backend.cpp` gains the ten commands' cases, including the truncated-at-every-length sweep the file already applies
- [ ] T039 [US2] `tests/fuzz/fuzz_cl_property_list.cpp` and `tests/fuzz/fuzz_cl_info_query.cpp` with committed corpora under `tests/corpus/`: the property lists (`cl_context_properties`, `cl_queue_properties`, `cl_mem_properties`) and the info queries where a size and a buffer both come from the application. A returned error, no write outside the destination, no unbounded loop, clean under ASan and UBSan
- [ ] T040 [US2] `tests/loopback/test_fullstack_opencl_icd.cpp` gains the US2 steps on the real executor: a mapped buffer written through and seen by the kernel, and an SVM allocation that survives map, write, unmap and a kernel argument

**Checkpoint** — US2 is provable: both map paths, SVM end to end, the device's bounds check, and two fuzz targets.

---

## Phase 5 (US3): Events and transfers never deadlock

**Goal**: a non-blocking transfer that is not made blocking, a callback that runs once on a thread the ICD owns, and a `clFinish` that sleeps rather than polls.

**Independent test**: loopback tests gate transfers on user events set from the same thread, another thread and a callback, with a timeout that fails the test instead of hanging.

- [ ] T041 [P] [US3] `src/device/abi/rvk_executor.h` becomes version 5: `RVK_EXECUTOR_ABI_VERSION_EVENTS 5` and the first device-supplied callback appended last (a `signal` function pointer and the device's opaque), with the "a version 4 table ends above" comment; `src/device/execution/executor_library.cpp` null-checks it in `accept` and gains `names_events()`, and `ContextExecutor::accepts` consults it
- [ ] T042 [US3] `tests/unit/device/execution/test_executor_library.cpp`, `tests/qemu/echo_executor.cpp` and `tests/conformance/executor/executor_conformance.cpp` learn version 5: the literal version assertions move, the version 1 and 3 tables stay so the gate is exercised, and a table claiming 5 without the callback fails realize
- [ ] T043 [P] [US3] `tests/unit/device/fence/test_fence_reporter.cpp` then `src/device/fence/fence_reporter.{hpp,cpp}` gains `nudge()`: worker-side, bumps `progress`, republishes the page, sets `pending_` so the next `signal()` raises, and leaves `completed_fence` where it was. A nudge after a fault writes nothing, as every other write does
- [ ] T044 [US3] `tests/unit/proto/submission/test_fence_progress.cpp` proves the guest accepts exactly this shape: `progress` advancing alone is accepted, and a `completed_fence` that moves past what was submitted, either counter going backwards, and a fault that un-faults are each still fatal to the context
- [ ] T045 [US3] `src/device/threading/worker.{hpp,cpp}` gains an any-thread wake beside its main-loop-only `post`, and `src/device/context/context_signal.{hpp,cpp}` is the landing pad: a flag set under a short mutex and one `Host::signal_event`, touching no fence page, no handle registry and no decode arena. `tests/unit/device/context/test_context_signal.cpp` proves a foreign-thread signal records no `worker_required` violation and that the worker clears the flag exactly once
- [ ] T046 [US3] `src/device/executor/cl_event_signals.{hpp,cpp}` and `src/device/executor/opencl_backend.cpp`: `clSetEventCallback` registers the backend's **own** host callback, which on completion records the event and signals the context. Every registration carries an epoch, and a callback whose epoch is stale does nothing — OpenCL cannot unregister a callback, so one can fire after `shut_down`, after `ContextExecutor::destroy` and after the session is gone
- [ ] T047 [US3] `tests/unit/device/executor/test_cl_event_signals.cpp` destroys a session with a callback in flight and proves the late callback touches nothing: the epoch is stale, no fence page is written, no violation is recorded, and the test is clean under ASan. This is the most dangerous code in the feature (research §6.5)
- [ ] T048 [P] [US3] `tests/unit/icd/opencl/test_cl_event_watcher.cpp` then `src/icd/opencl/cl_event_watcher.{hpp,cpp}`: the watcher sleeps in the transport's wait and never polls; on waking it asks which events completed, runs each callback **once**, with no ICD lock held — asserted by `LockTrace` — and forgets the registration before running it, so a callback that releases its own event or context is safe; nothing is registered on a lost context; the 257th registration is refused
- [ ] T049 [US3] `src/icd/opencl/cl_icd_root.cpp` answers `clWaitForEvents`, `clFinish` and every blocking transfer through `icd::SlicedWait`'s 500 ms slices, because a host call that blocks holds its context's worker and a batch outliving TdrDelay has dxgkrnl reset the adapter; `tests/unit/icd/opencl/test_cl_waits.cpp` proves a zero timeout still asks once, an infinite one never passes, and a clock with no frequency gets one slice
- [ ] T050 [US3] `src/icd/opencl/cl_icd_root.cpp` makes no transfer blocking that the application did not: a non-blocking read returns as soon as the host has queued it, its destination is recorded against the event, and the bytes are delivered from the host-visible blob when the ICD observes completion — or fetched then, where the capset offers no host-visible memory
- [ ] T051 [US3] `tests/loopback/test_fullstack_opencl_icd.cpp` gains the three deadlock scenarios on the real executor, each with a timeout that fails the test rather than hanging: a non-blocking read gated on a user event set later by the same thread, one set from another thread, and one set from inside a callback
- [ ] T052 [P] [US3] `tests/fuzz/fuzz_cl_host_reply.cpp` with a committed corpus: the host replies the ICD decodes, including an answer cut at every field boundary, as `ScriptedHost::cut_next` leaves one

**Checkpoint** — US3 is provable: ABI v5, the out-of-band raise, the epoch, the watcher, and three scenarios that would hang if it were wrong.

---

## Phase 6: Device loss, the package, and the guest

- [ ] T053 [P] `tests/unit/icd/opencl/test_cl_device_loss.cpp`: a faulted context answers `CL_OUT_OF_RESOURCES` on its queue's operations while contexts on other devices are untouched; a context made after the fault opens a new device beside the lost one; the probe's forced fault uses `ContextFault`'s command-id constructor with an OpenCL id, because a Vulkan command is merely skipped by this decoder
- [ ] T054 [P] `src/km/rvkgpu.inf` adds `rvkcl.dll` to `[SourceDisksFiles]` and `[Rvkgpu_Files]`, and `HKR,, OpenCLDriverName, 0x00000000, "%13%\rvkcl.dll"` to `[Rvkgpu_SoftwareSettings]` — `REG_SZ`, **not** the `0x00010000` its two neighbours use, because the loader's D3DKMT path asks for a `REG_SZ` and returns nothing for another type while only its SetupAPI fallback takes either (research §1)
- [ ] T055 `tests/tools/test_driver_package.py` learns `rvkcl.dll` in `PACKAGE_FILES` and in the staged-set assertion, and gains an `OpenCLDriverName` test beside the `OpenGL*` one: the value, its type, that no `OpenCLDriverNameWow` is written, and that nothing names the global Khronos vendors key
- [ ] T056 [P] `CMakeLists.txt` registers `rvkcl_image` (`--dll --only-exports --require-export clGetExtensionFunctionAddress --require-export clIcdGetPlatformIDsKHR`, no `--allow-import`) and `rvkclprobe_image` (`--allow-import OpenCL.dll --require-import clGetPlatformIDs`)
- [ ] T057 `tools/guest/run_guest_suite.py` gains `--opencl`: the `CL_SUMMARY`/`CL_STEP`/`CL_NOTE`/`CL_STEPS` constants, four arguments, the `prepare` check that the pinned loader is on disk reusing `fetch_opencl_loader.py`'s own `pins_from`/`problems_on_disk`, `rvkcl.dll` in the package push and `OpenCL.dll` plus `rvkclprobe.exe` beside the probe, the entry in the `real_executor` tuple, and the `opencl()` method
- [ ] T058 `tests/tools/test_run_guest_suite.py` and `tests/tools/fake_guest/fake_qemu.py` learn the mode: `rvkclprobe` joins `PROGRAMS`, a literal `CL_REPORT` and its script, the journal assertions for the push, the `cmd /c ... > opencl.txt 2>&1` exec and the pull, and the table of mutated reports — a failed step, a missing step, no summary, and a device name that is a software device
- [ ] T059 `tools/gate.sh` appends `--opencl` to the real-executor list only when `tools/fetch_opencl_loader.py --check` passes, printing the skip reason otherwise, exactly as it gates `--vulkan`

**Checkpoint** — the package, the image gates and the runner agree; `ctest --test-dir build -j32` is green.

---

## Phase 7: Polish and the full gate

- [ ] T060 [P] `docs/protocol.md` records the ten commands, the executor ABI's version 5 and its callback, and the out-of-band raise — `progress` moves, `completed_fence` does not — in the same commit as the regenerated sources and `opencl_ids.toml`
- [ ] T061 [P] `docs/kb/07-usermode.md` gains "How the Khronos loader finds an ICD on a WDDM adapter" from research.md §1: the three discovery paths, why the value is `REG_SZ`, the one export, and the two platform queries an ICD cannot decline
- [ ] T062 [P] `README.md`'s layout gains `src/icd/opencl/` and `src/um/opencl/`, and the guest configuration row gains `--opencl`
- [ ] T063 `cmake --build build-coverage -j32 && ctest --test-dir build-coverage -j32` is green: 100% branches over everything this feature added to `src/core`, `src/device` and `src/proto`, with every `AV_UNTESTABLE` justified at its site and the ratchet not risen
- [ ] T064 `ctest --test-dir build-fuzz -j32 -L fuzz` is green: each target in `tests/fuzz/` this feature added survives its 60-second smoke run from its corpus under `tests/corpus/`
- [ ] T065 `tools/gate.sh` with `AV_GUEST_IMAGE` set passes: the package installs under Driver Verifier, the adapter starts, and the `--opencl` boot reports `rvkclprobe: N passed, 0 failed` with `CL_DEVICE_NAME` naming the host GPU

**Checkpoint** — every task `[X]`, the full gate green, ready to fast-forward into `master`.

---

## Dependencies

- Phase 1 blocks everything: nothing compiles `cl_icd_stubs.h` until T001, and no ICD target exists until T006/T007.
- Phase 2 blocks every story: T009-T012 are the wire, and T016-T017 are how any ICD class is tested.
- US1 (Phase 3) is independent of US2 and US3 and is the MVP.
- US2 (Phase 4) needs US1's context and the arena; US3 (Phase 5) needs US1's context and events.
- T041-T047 (the device push) are independent of the ICD classes and may run beside Phase 3 or 4.
- Phase 6 needs US1 for the probe to have anything to report.

## Parallel opportunities

- T001, T002, T006, T007 in Phase 1.
- T008 with T014 in Phase 2; T013 once T012 lands.
- T018, T019, T020, T023, T024 in Phase 3 — five different files, no shared state.
- T032, T034, T037, T039 in Phase 4.
- T041, T043, T048, T052 in Phase 5.
- T054, T056, T060, T061, T062 in Phase 6 and 7.

## Implementation strategy

US1 is the MVP: an OpenCL device the Khronos loader finds, that runs a kernel and reads it back. It is a complete, shippable increment on its own and it is what proves the registration path nothing else can. US2 and US3 then add the memory and the event semantics real applications need, in that order, because a map is reached for far more often than a callback.
