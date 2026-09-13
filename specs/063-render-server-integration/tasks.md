---

description: "Tasks: the generators, vendored"
---

# Tasks: the generators, vendored

**Input**: Design documents from `/specs/063-render-server-integration/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/generated_code.md, quickstart.md

**Tests**: Required, and written first. Constitution IV.

**Organization**: by user story, in priority order. Each `**Checkpoint**` is a commit once `ctest --test-dir build -E tools_tests` passes there.

## Format: `[ID] [P?] [Story] Description`

- **[Story]**: US1 (the generator is ours), US2 (the registries are pinned), US3 (what the generated code must satisfy)

---

## Phase 1: US2 - The registries are pinned inputs (Priority: P1)

**Purpose**: nothing can be generated until there is something to generate from, and it has to arrive the way every other external input does.

- [X] T001 [US2] Write `tests/tools/test_fetch_khronos_registries.py` first, in the style of the other fetch-tool tests: a fetch into an empty tree writes the registries and a marker; a second fetch is a no-op that says so; a hash that does not match unpacks nothing, removes the partial download and names the registry; an unreachable source leaves a previously verified tree untouched
- [X] T002 [US2] `tools/fetch_khronos_registries.sh`, modelled on `tools/fetch_qemu.sh`: `vk.xml` and `vulkan_core.h` from Vulkan-Headers, `gl.xml` from OpenGL-Registry, `cl.xml` and the OpenCL headers, each at a **tag**, each verified by SHA-256, unpacked under `third_party/khronos/` with a `VERSION` marker; no path outside this repository appears in any tracked file (`self_contained`)
- [X] T003 [US2] `.gitignore` covers `third_party/khronos/`, and `README.md` gains the fetch beside the Windows kit's

**Checkpoint**: the inputs arrive, verified. Commit.

---

## Phase 2: US1 - The generator is ours (Priority: P1) 🎯 MVP

**Goal**: the generator lives here, runs here, and a gate proves the committed tree is what it writes.

**Independent Test**: regenerate from a clean checkout and get no diff; edit a generated file and watch the gate name it.

- [X] T004 [US1] Vendor `rvk_codegen` into `tools/codegen/rvk_codegen/` - the Vulkan package and its `opengl` and `opencl` siblings - with its `pyproject.toml` reduced to this repository's linter settings. Record in `tools/codegen/README.md` where it came from (render-server at `89936e50b6145e75af3553e27d5c8a251787dd62`), that it is stdlib-only, and that it is now this repository's to edit
- [X] T005 [US1] The profiles become this repository's: `tools/codegen/profiles/{vulkan,opengl,opencl}.toml` and their id files, taken as they are with a header comment saying what they steer and that editing one is a wire change
- [X] T006 [P] [US1] Write `tests/tools/test_check_generated.py` first: a committed tree that matches passes; one that differs fails naming the file; missing registries skip with the reason; a generated directory that holds a file the generator would not write fails too
- [X] T007 [US1] `tools/check_generated.py`: runs each generator with `--check` against its committed directory, reports per API, and is wired into `ctest` as `generated_sources` beside `design_rules`
- [X] T008 [US1] Generate the Vulkan tree into `src/proto/generated/vulkan/` and commit it, with `--header` pointed at the fetched `vulkan_core.h` so the emitted `pNext` handling matches the header the output compiles against

**Checkpoint**: one API's wire is generated, committed and gated. Commit.

---

## Phase 3: US3 - What the generated code must satisfy (Priority: P1)

**Goal**: the emitted sources compile under this repository's flags, and the gates judge machine-written code by rules it can keep.

- [X] T009 [US3] Change the `serialize` emitter to emit this repository's includes and types: `proto/wire/wire_reader.hpp` and `proto/wire/wire_writer.hpp` instead of `rvk/protocol/buffer*.hpp`, no `rvk/instr/log.hpp`, and a decode that returns the reader's status rather than trusting a length (contracts/generated_code.md, section B)
- [X] T010 [US3] `tools/check_design.py` and `tools/check_coverage.py` exclude `generated/` directories by one named rule each (`header_scan.is_generated`), with their own tests asserting that the same class outside such a directory is judged as ever. One bug found doing it: both gates walk with `sorted(os.walk(...))`, which materialises the walk, so pruning `dirs[:]` would have come too late to skip anything - the exclusion filters by path instead
- [X] T011 [US3] The generated Vulkan wire compiles into `av_core` and `rvk_device` under the full warning set, freestanding, with no exceptions or RTTI. Three things the build said that the plan had not:

  - the generated code includes the API's own header, which arrives with the fetch, so a clean checkout would have stopped building. The generated sources join the build only when `third_party/khronos/include` is there, and the configure line says which state it is in;
  - **only the wire compiles today.** The generator also writes the guest's dispatch and ICD entries and the host's server dispatch, and those name types no feature has built yet - a transport (043), an ICD connection (045), a backend registry (042). They are generated and committed all the same, because the gate compares the whole tree, and each joins a build when its feature arrives;
  - neither the kernel miniport nor the no-CRT guest suite can compile `<cstdint>`, and nothing generated belongs in either: a graphics API's commands are decoded on the host
- [X] T012 [US3] Write `tests/fuzz/fuzz_generated_decode.cpp` and its corpus: a frame from outside into the generated decoder, with the corpus seeded from a real encode of each of a handful of commands, and the fuzz smoke gate passing
- [X] T013 [US3] A unit test that the generated encode and decode round-trip a command and that a truncated frame is refused rather than read past, in `tests/unit/proto/generated/test_generated_vulkan.cpp`

**Checkpoint**: generated code is code this project can stand behind. Commit.

---

## Phase 4: The other two APIs

- [ ] T014 [P] Generate and commit the OpenGL tree, with its emitter changed the same way, and the gate covering it
- [ ] T015 [P] Generate and commit the OpenCL tree, likewise
- [ ] T016 The two emitters that use `std::vector` - `server_dispatch` and `icd_entries` - are dealt with where their output lands: either changed to this repository's fixed-capacity types, or emitted into a host-only directory that says why more is allowed there. Record which and why in `contracts/generated_code.md`

**Checkpoint**: three APIs, one generator, one gate. Commit.

---

## Phase 5: What the feature promised

- [X] T017 `docs/protocol.md` and the id files agree, and the gate fails while they do not (FR-007)
- [X] T018 `docs/kb/11-implementation-plan.md` stops describing `librvk_codec` as something render-server contributes, and says what this project generates instead
- [ ] T019 The specs for features 042 and 043 are corrected: they describe render-server's runtime as theirs to consume, and this project writes both against the generated wire
- [ ] T020 `AV_GUEST_IMAGE=... tools/gate.sh` passes; merge to `master`; push

---

## Dependencies & Execution Order

- **US2 (Phase 1)** blocks everything: no registry, nothing to generate.
- **US1 (Phase 2)** is the MVP and can stop there: a vendored generator, one
  API generated and gated, is already worth having.
- **US3 (Phase 3)** is what makes the output usable rather than merely present.
- **Phase 4** repeats Phase 3's work for two more registries and can run in
  parallel once the pattern is set by Vulkan.
- **Phase 5** is the tidying the rest of the repository needs to stop pointing
  at the old plan.

### Parallel opportunities

- T006 is a test file written against a tool that does not exist yet.
- T014 and T015 are two APIs whose trees never touch.

---

## Implementation Strategy

**MVP**: Phases 1 and 2 - the registries arrive verified, the generator is ours,
and the Vulkan tree is committed and gated. **Increment 2**: Phase 3, which is
what lets features 042 and 043 build on it. **Increment 3**: Phases 4 and 5.

---

## Notes

- Never hand-edit a file under `generated/`: fix the emitter and regenerate.
  The gate exists to make that rule enforceable rather than remembered.
- A regeneration that changes a protocol id is a wire change, and
  `docs/protocol.md` changes in the same commit.
- The generator is stdlib-only Python and must stay that way; a dependency would
  make the gate unrunnable on a machine that has not installed it.
