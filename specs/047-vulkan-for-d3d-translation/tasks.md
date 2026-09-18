---
description: "Tasks: the feature set DXVK and vkd3d-proton need (047)"
---

# Tasks: the feature set DXVK and vkd3d-proton need

**Input**: plan.md, spec.md (Clarifications), research.md, data-model.md,
contracts/{descriptor_templates,external_objects,placed_mappings}.md, quickstart.md

**Tests**: written first (constitution IV); invariants after every mutation;
a hostile table and a fuzz target for every byte string that comes from outside
(a template's blob, the Win32 records, a layer archive). Commit at every
**Checkpoint** once `ctest --test-dir build -LE tools -j6` passes; the coverage
build before a new class is committed.

**The 32-bit ICD is not here.** spec.md's Clarifications record why: feature
061 owns building and registering it. What a WoW64 process needs from the ICD's
behaviour is T006-T010 of this list.

## Phase 1: Foundational - descriptor update templates on the wire

Nothing carries `vkUpdateDescriptorSetWithTemplate` today, not even the core
1.1 command, because its `pData` has no length of its own. Three extensions
wait on this: `VK_KHR_descriptor_update_template`, `VK_KHR_push_descriptor`
(vkd3d-proton requires it) and the core command DXVK calls.

- [X] T001 `DescriptorTemplateLayout` (object) in `src/proto/resources/descriptor_template_layout.{hpp,cpp}`: the entries of one `VkDescriptorUpdateTemplateCreateInfo` (`dstBinding`, `dstArrayElement`, `descriptorCount`, `descriptorType`, `offset`, `stride`, at most `max_entries`), and the byte length one update reads - `max over entries of (offset + (descriptorCount - 1) * stride + bytes of one descriptor of that type)`, every term 64-bit and overflow-checked, `too_large` when any product or sum overflows; inline uniform block is one byte per `descriptorCount`; an empty template answers 0. Tests `tests/unit/proto/resources/test_descriptor_template_layout.cpp`: every descriptor type's size, the overflow of each term, an entry whose end is past another's, and the invariant after every mutation
- [X] T002 `tests/fuzz/fuzz_descriptor_template.cpp` with a committed corpus in `tests/corpus/fuzz_descriptor_template/`: entries from attacker-chosen bytes, asserting a returned status, no read outside the entry array, and that a length it answers is never smaller than an entry's own end
- [X] T003 The generator emits the hook: `descriptor_template_bytes(uint64_t)` declared in the generated header with a weak default of 0 in its own translation unit (the shape `host_image_copy_bytes` has), the three `WithTemplate` commands lowered with their blob sized by it, and `vkCmdPushDescriptorSetWithTemplateKHR` no longer withdrawing its extension. `tools/codegen/rvk_codegen/emit/{classify,serialize}.py`; regenerate `src/proto/generated/vulkan/`; `tools/codegen/profiles/vulkan.toml` gains `VK_KHR_push_descriptor`'s companion names only if the registry needs them. Tests in `tests/tools/test_client_call_scope.py` style over the generator: `tests/tools/test_gen_descriptor_templates.py`
- [X] T004 `DescriptorTemplateTable` (system) in `src/icd/vulkan/descriptor_template_table.{hpp,cpp}`: the layouts of the templates a device made, by handle, forgotten on destroy; the strong `descriptor_template_bytes` in `src/icd/vulkan/icd_hooks.cpp` answers from it. Tests `tests/unit/icd/vulkan/test_descriptor_templates.cpp` over IcdRig: a create records, a destroy forgets, an update carries exactly the layout's bytes, a template no device made is refused, a create the host refused records nothing
- [X] T005 `HostTemplateTable` (system) in `src/device/executor/host_template_table.{hpp,cpp}`: the same layouts from the create command the device decoded, and the strong `descriptor_template_bytes` in `src/device/executor/vulkan_backend.cpp`; a blob length that disagrees with the layout refuses the frame (contracts/descriptor_templates.md). Tests `tests/unit/device/executor/test_host_template_table.cpp`: a short blob, a long blob, an unknown template, a destroyed one

**Checkpoint**: commit. `ctest -R 'generated_sources|protocol_ids'` passes and the three commands cross the wire.

## Phase 2: Foundational - vkMapMemory2 and placed mappings

What a 32-bit DXVK cannot live without (`vulkan.toml:141-174`), and what FR-003
asks the ICD to provide itself.

- [X] T006 `vkMapMemory2KHR` and `vkUnmapMemory2KHR` named in `[commands].exclude` in `tools/codegen/profiles/vulkan.toml`, regenerated, and answered as `local_vkMapMemory2`/`local_vkUnmapMemory2` in `src/icd/vulkan/local_entries.{hpp,cpp}` with rows in `local_rows`; the two `kManualEntryPoints` self-checks (`tests/unit/icd/vulkan/test_entry_points.cpp`, `test_icd_root.cpp`) cover the new names
- [X] T007 **Replaced by a finding.** A placed mapping is not this ICD's to give: a chunk's process view is the address `D3DKMTLockAllocation` returned and no lock flag takes one (research.md §9, and 041 T023 one level down). `VK_EXT_map_memory_placed` is therefore not reported, `PlacedMapping` is not built, and contracts/placed_mappings.md is corrected to say so
- [X] T008 [US1] What is delivered instead: `vkMapMemory2KHR` behaves exactly as `vkMapMemory` does, a `VkMemoryMapPlacedInfoEXT` in its chain is refused with `VK_ERROR_MEMORY_MAP_FAILED` rather than silently ignored, and `VK_MEMORY_MAP_PLACED_BIT_EXT` is refused the same way - a caller that asks for placement is told no, not lied to
- [X] T009 [US1] The `VK_KHR_map_memory2` policy row with the test it names; a test states that `VK_EXT_map_memory_placed` is not reported and why (FR-002's shape: nothing reported that is not implemented)
- [X] T010 [US1] Not a fuzz target of its own: `vkMapMemory2` reads three scalars and refuses every chain, so there is no parser to fuzz. The refusals are the table in `test_icd_memory.cpp`, and what the mapping itself does with hostile offsets and sizes is already `fuzz_chunk_placement`'s

**Checkpoint**: commit.

## Phase 3: US1 - DXVK finds everything it requires (P1)

- [X] T011 [P] [US1] `VK_EXT_image_view_min_lod` and `VK_EXT_mutable_descriptor_type` added to `[extensions].include` in `tools/codegen/profiles/vulkan.toml` and regenerated (no commands, structs already serialize)
- [X] T012 [US1] A policy row and the test it names for every extension DXVK requires that the generator implements - the thirteen US1 scenario 1 lists plus `VK_KHR_descriptor_update_template`, `VK_KHR_push_descriptor`, `VK_EXT_image_view_min_lod`, `VK_EXT_mutable_descriptor_type` - in `src/icd/vulkan/icd_root.cpp`'s `reported_extensions`, each with its `VK_..._SPEC_VERSION`. Tests grouped by what they exercise in `tests/unit/icd/vulkan/test_reported_extensions.cpp`: for each extension, the structure or command that makes it real reaches the host, and the extension is absent when the host does not list it
- [X] T013 [US1] `LayerRequirements` (object) in `tools/layer_requirements.py`: one pinned layer archive on disk, the required and the optional names it declares, a name it cannot classify reported rather than dropped, every name matching `VK_[A-Z]+_[a-z0-9_]+`, the two sets disjoint (data-model.md). Tests `tests/tools/test_layer_requirements.py` over archives the test writes itself
- [X] T014 [US1] One tool for both layers rather than two near-identical ones: `tools/fetch_layers.py` with `tools/dxvk_pins.txt` and `tools/vkd3d_proton_pins.txt`, in the shape of `tools/fetch_vulkan_runtime.py` - a pinned source archive (for the list) and a pinned release archive (for the DLLs), a SHA-256 each, nothing written when a hash disagrees, `--check`, `.zip`/`.tar.gz`/`.tar.zst`. Tests `tests/tools/test_fetch_layers.py` over archives the test builds and serves through `file://`. **The pin lines themselves are empty**: they are two URLs and three hashes of files this machine has never downloaded, and a hash written from memory would pin whatever a guess named, so each file documents its own shape and whoever first runs the fetch with a network fills them in
- [X] T015 [US1] A gate rather than a C++ test, because both halves are already written down and neither should be copied into a third place: `tools/check_layer_requirements.py` reads the layer's required list out of its pinned source and the reported set out of the ICD's own policy tables, and fails naming the first requirement that is not reported (FR-001). What a loader answers rather than a driver is not counted as missing. A layer whose pins are not fetched is skipped with a line saying so; a layer whose source is there but names no requirement at all is a failure, because that is what a broken extractor looks like. `ctest -R layer_requirements`, tests in `tests/tools/test_check_layer_requirements.py`

**Checkpoint**: commit.

## Phase 4: US2 - vkd3d-proton finds everything it requires (P1)

- [X] T016 [US2] Delivered by T014's one tool: `fetch_layers.py vkd3d-proton` with `tools/vkd3d_proton_pins.txt`, `.tar.zst` read through Python 3.14's `compression.zstd`, and `tests/tools/test_fetch_layers.py` driving exactly that layer
- [X] T017 [US2] `LayerRequirements` reads the table shape vkd3d-proton uses as well as the mode shape DXVK uses, and `check_layer_requirements.py` holds the ICD to both layers' lists. `VK_EXT_descriptor_buffer` stays out of the profile, and `reported_extensions.a_structure_the_wire_does_not_carry_is_not_sent` states that a node of it is not even sent (FR-002, US2 scenario 3)
- [X] T018 [US2] The features vkd3d-proton requires - dynamic rendering, synchronization2, timeline semaphores, buffer device address, descriptor indexing, mutable descriptor types, demote-to-helper, image view min LOD, push descriptors - answered from the host unchanged, proven by a test that asks for each `VkPhysicalDeviceFeatures2` node and compares it with what the scripted host said (US2 scenario 1), and by one that makes a host missing one answer its absence (US2 scenario 2)

**Checkpoint**: commit.

## Phase 5: US3 - DXGI interop through Win32 external objects (P2)

- [X] T019 [US3] The records in `src/icd/vulkan/{memory_win32_handle_record,semaphore_win32_handle_record}.hpp` beside `win32_surface_create_record.hpp`, with the registry's own structure-type arithmetic written out and the core handle-type constants used rather than literals. `VkMemoryWin32HandlePropertiesKHR` is deliberately absent: that query answers for a handle from outside, and this path refuses one. Read - `sType` first, then the fields - by `ExternalObjects`, whose tests are the hostile table over each structure
- [X] T020 [US3] `VK_KHR_external_memory_win32` and `VK_KHR_external_semaphore_win32` in `[extensions].include` **with their four commands in `[commands].exclude`**, so nothing generated names `HANDLE` at all and the width audit has nothing to teach: the four are local ICD entries over the records, the route `vkMapMemory2` already took. Reported only once T022's entry points exist (research.md §9-10)
- [X] T021 [US3] `ExternalObjects` (system) in `src/icd/vulkan/external_objects.{hpp,cpp}` - one class reading the records and owning the table, rather than a record reader beside a table: the handles this process exported, each naming one memory or one semaphore of one device, the same object exported twice answering the same handle, and every handle forgotten with what it names. Tests `tests/unit/icd/vulkan/test_external_objects.cpp`: an export an import follows, the two kinds not answering for each other, every refusal of contracts/external_objects.md on both sides, a table with no room, and each clause of the invariant
- [X] T022 [US3] The four local entries over `ExternalObjects`, each translating to the FD extensions the host has (research.md §10): an export remembers and answers a handle, an import asks for a **fresh** FD of the remembered object and forwards `VkImportMemoryFdInfoKHR` / `vkImportSemaphoreFdKHR` in place of the Win32 structures. `vkGetPhysicalDeviceExternalBufferProperties` and `...ExternalSemaphoreProperties` answer the two `OPAQUE_WIN32` types with export and import and every other Win32 type absent, and the extensions are reported only when the host lists the FD-based ones. Tests in `test_external_objects.cpp` and `test_icd_extensions.cpp`
- [X] T023 [US3] `tests/loopback/test_fullstack_external_objects.cpp`: on the real host GPU, one device exports a buffer's memory and a semaphore, a second device in the same process imports both, waits on the signal and reads back what the first wrote. A buffer rather than an image, because what is being proven is the shared allocation and the wait, and a buffer's contents are readable without a layout transition

**Checkpoint**: commit.

## Phase 6: The guest - D3D through the layers

- [X] T024 **No such class.** `GuestReport("rvkd3d11probe")` already prints that grammar - `ok <scenario>.<step> <value>` and `<program>: P passed, F failed` - and `ScenarioSteps::check_result` already judges a step by a graphics API's own result code, which an HRESULT is (0 is success, a failure is reported as its two's-complement bits). Writing a second report class would be the same behaviour under a different name (CLAUDE.md); the three probes name their program and their scenario and use these
- [X] T025 [P] `D3d11Work` in `src/guest/suite/d3d11_work.{hpp,cpp}` over a `D3d11Entry` seam (`src/guest/suite/d3d11_entry.hpp`, scripted in tests): device at the feature level the layer reports, a swap chain on the probe's window, **a cleared frame** rather than a triangle - a triangle needs DXBC and no shader compiler is pinned here - a read-back and a BMP. Tests `tests/unit/guest/suite/test_d3d11_work.cpp` over a scripted D3D11
- [X] T026 [P] `D3d12Work` in `src/guest/suite/d3d12_work.{hpp,cpp}` over a `D3d12Entry` seam: device, command queue, a render target **cleared** through a command list and a fence, a read-back, a BMP. Tests `tests/unit/guest/suite/test_d3d12_work.cpp`
- [X] T027 [P] `D3d9Work` in `src/guest/suite/d3d9_work.{hpp,cpp}` over a `D3d9Entry` seam: device on the probe's window, a **cleared** frame, a read-back, a BMP. Tests `tests/unit/guest/suite/test_d3d9_work.cpp`
- [X] T028 `rvkd3d9probe.exe`, `rvkd3d11probe.exe` and `rvkd3d12probe.exe` in `src/um/probe/d3d/`, each a no-CRT program with its own entry, the window from `ProbeWindow`, and the layer's DLL beside it; `src/um/CMakeLists.txt` builds them and `CMakeLists.txt` gates each with `check_guest_image.py`
- [X] T029 `run_guest_suite.py --d3d`: pushes the pinned DXVK and vkd3d-proton DLLs and the three probes, runs each in the console session (`ConsoleProgram`), requires every step and the summary, pulls the three BMPs; refused when the pins are not fetched. Tests in `tests/tools/test_run_guest_suite.py`
- [X] T030 FR-006: `--page-heap`, the same run with full page heap on each probe - the Image File Execution Options values `gflags /p /enable` writes, since the debugging tools are not in the image - asserting no heap, verifier or `0xc0000374` event in the guest's Application log and naming every one it finds. The flag comes off the machine before the log is read, so a run leaves it as it found it whatever the probes did. Whether the corruption the spec looked for is reproducible here is what the run answers, and it needs the pinned layers to answer it at all

**Checkpoint**: commit.

## Phase 7: Polish and merge

- [ ] T031 docs: `docs/testing.md` (the D3D layers and the `--d3d` run), `docs/kb/07-usermode.md` (what the two layers require, and that the 32-bit ICD is 061's), `docs/roadmap.md` if a capability line changes
- [ ] T032 Coverage and fuzz smoke; `AV_GATE_JOBS=6 AV_GUEST_IMAGE=... tools/gate.sh`; merge; push

## Dependencies

- Phase 1 blocks T012's push-descriptor and template rows, and T018's push-descriptor feature.
- Phase 2 blocks T009 and is what FR-003/FR-004 deliver here.
- T013 blocks T015 and T017; T014 and T016 block the guest run's pushes (T029).
- Phase 5 is independent of Phases 3-4 and can be built in parallel with them.
- Phase 6 needs T014 and T016 for the DLLs, and Phase 3 for a device to exist.

## Implementation strategy

US1 is the MVP: with Phases 1-3, DXVK creates a device on rvkgpu. US2 follows
on the same mechanisms. US3 is separable and is the only part with no
application behind it yet, so it lands last of the code. The guest run proves
all three, and is the only place the layers themselves ever run.
