---

description: "Tasks: rvkumd.dll - the user-mode driver and DXGI"
---

# Tasks: rvkumd.dll - the user-mode driver and DXGI

**Input**: Design documents from `/specs/055-umd-foundation/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/umd_interface.md, contracts/dxgi_seam.md, contracts/private_data.md, quickstart.md

**Tests**: Required, and written first (constitution IV). Every new class gets its test before its body, `invariants_hold()` checked after every mutation, and hostile-input tables plus a fuzz target for anything reading bytes it did not write - which here includes every structure the Direct3D runtime hands the driver.

**Organization**: setup and the amendments first, then the wire envelope, then one phase per user story, then the two phases nothing in this environment can finish. Each `**Checkpoint**` is a commit once `ctest --test-dir build -j4 -LE tools` passes there.

## Format: `[ID] [P?] [Story] Description`

- **[Story]**: US1 (the runtimes load rvkumd.dll), US2 (DXGI and DXCore see a real GPU), US3 (DXGI presents through the UMD), US4 (the KMD distrusts the UMD's private data)

---

## Phase 1: Setup - the headers, the rules and the archive

- [ ] T001 [P] `tools/fetch_windows_kit.sh`: a `WDK_UM_HEADERS` list beside the SDK's `UM_HEADERS`, taking `d3d10umddi.h`, `d3d12umddi.h` and `d3dumddi.h` from the WDK package's `c/Include/10.0.26100.0/um/`, and `dxgiddi.h` plus its closure (`dxgitype.h`, `dxgiformat.h` if not already present) from the SDK's; the `PINNED` marker gains a token so an existing kit re-fetches. The three WDK headers are 1.6 MB of a 120 MB directory, so they are named, not taken whole
- [ ] T002 [P] `tools/header_scan.py`: `RECORD_DIRECTORY` becomes `RECORD_DIRECTORIES = ("src/core/ddi/", "src/icd/d3d/ddi/")` and the membership test becomes a tuple test; `tests/tools/test_header_scan.py` gains a case per directory (a record in each is exempt, a record outside both is not, a non-`Record` name in a record directory is not)
- [ ] T003 `.specify/memory/constitution.md` (PATCH) and `CLAUDE.md`: the list of process-lifetime state gains `rvkumd.dll`'s root behind `OpenAdapter10_2` / `OpenAdapter12` and `UmdBinding`'s pointer to it; the DDI-record adaptation says "a DDI record directory" rather than `src/core/ddi/` and names both. `README.md`'s layout lists `src/icd/d3d/` and `src/um/d3d/`
- [ ] T004 [P] `tools/re/binaries.txt` gains `d3d11.dll ae2777002f5a0ff543b5b9bec5124746559fd01d6c7c0c216d3194632f084502` (10.0.26100.9278, the binary research.md §1 read the export names out of)
- [ ] T005 `CMakeLists.txt`: an `av_d3d_umd` archive from `src/icd/common/*.cpp` and `src/icd/d3d/*.cpp` beside `av_icd` and `av_gl_icd` - its own archive, as the DLLs are apart - and a link branch ahead of the existing `elseif` so `tests/unit/icd/d3d/` links it and not the Vulkan ICD's

**Checkpoint**: the kit has the headers, the gates know the new record directory, and an empty archive builds. Commit.

---

## Phase 2: US4 - the envelope the KMD refuses (Priority: P1)

**Goal**: the KMD's refusal path for user-mode private data is proven before any field exists.

**Independent test**: a table of malformed envelopes is refused with a status, nothing is written, and a fuzz target survives the smoke run.

- [ ] T006 Write `tests/unit/proto/resources/test_umd_private_data.cpp` first: encode/decode round trip for each kind; refusal of every length below 16, a wrong magic, a version other than 1, a kind of 0 and of 4, a non-zero reserved field, a `length` below 16, a `length` past the buffer, and a `length` whose sum with a body offset overflows; a byte-flip sweep of a valid record (`HostileSweep`)
- [ ] T007 `UmdPrivateData` in `src/proto/resources/umd_private_data.{hpp,cpp}` per contracts/private_data.md; `tests/fuzz/fuzz_umd_private_data.cpp` with a committed corpus in `tests/corpus/fuzz_umd_private_data/`, and its name in the fuzz target's ICD-link allowlist if it needs one
- [ ] T008 `Adapter::create_allocation` in `src/core/memory/` refuses a private-data blob this decoder refuses, with `STATUS_INVALID_PARAMETER` and one log line naming the offending value; test first in `tests/unit/core/memory/test_allocation.cpp` and through the DDI table in `tests/unit/scenarios/`

**Checkpoint**: hostile private data is refused and logged once. Commit.

---

## Phase 3: US1 - the runtimes load rvkumd.dll (Priority: P1) 🎯 MVP

**Goal**: `OpenAdapter10_2` and `OpenAdapter12` open the adapter and hand back complete function tables, driven on Linux by a fake runtime.

**Independent test**: `FakeD3dRuntime` resolves both exports, finds every table slot non-null, and gets the documented answer from each.

### The mirror records

- [ ] T009 [P] Write `tests/unit/icd/d3d/test_umd_ddi_records.cpp` first: each record's size and every field offset asserted against the values `data-model.md` tabulates, and a decode of a record whose pointer fields are null refused rather than dereferenced
- [ ] T010 The ten mirror records in `src/icd/d3d/ddi/`, one header each, public fields with the WDK's names (data-model.md, "The DDI mirror records")
- [ ] T011 `src/um/d3d/umd_record_layout.cpp`: `static_assert` per record and per field against the real `d3d10umddi.h`, `d3d12umddi.h`, `d3dumddi.h` and `dxgiddi.h`, compiled into the DLL cross-build as `src/km/ddi_record_layout.cpp` is into the driver

### The seams and the rig

- [ ] T012 [P] Write `tests/unit/harness/icd/test_umd_rig.cpp` and `test_fake_d3d_runtime.cpp` first, then `tests/harness/icd/umd_rig.{hpp,cpp}` and `fake_d3d_runtime.{hpp,cpp}` (data-model.md, "The harness"): the rig stands a process up over `MockPlatform`, `MockKmt`, two recording locks and a recording log; the fake plays `d3d11.dll` and `D3D12Core.dll`, checks every slot is non-null, journals every call, and can refuse or corrupt what it hands in
- [ ] T013 [P] Write `tests/unit/icd/d3d/test_ddi_version_list.cpp` first (the count-then-fill protocol: a null array answers the count; a short array is `E_OUTOFMEMORY` with the count written back; an exact array is filled; a longer one is filled and the count written back; `serves()` for the word it has and for every neighbouring word), then `DdiVersionList` in `src/icd/d3d/ddi_version_list.{hpp,cpp}`

### The adapter

- [ ] T014 Write `tests/unit/icd/d3d/test_adapter_identity.cpp` first (LUID, ids and both memory figures from `MockKmt`; a refused LUID or video-memory query reports zero, logs once, and does not fail the open), then `AdapterIdentity` in `src/icd/d3d/adapter_identity.{hpp,cpp}`
- [ ] T015 Write `tests/unit/icd/d3d/test_umd_connection.cpp` first (open, faulted, close; `invalid_state` on a second open; the first refusal gives back everything made), then `UmdConnection` in `src/icd/d3d/umd_connection.{hpp,cpp}` over `AdapterSearch`, `GuestConnection` and `KmtTransport` of `SubApi::vulkan`
- [ ] T016 Write `tests/unit/icd/d3d/test_umd_adapter.cpp` and `test_adapter_table.cpp` first (an open records the runtime handle, the callbacks pointer and the interface; a close with a live device is `E_FAIL`; a handle the table never gave answers null and is never dereferenced; the pool's occupancy is the table's invariant), then `UmdAdapter` and `AdapterTable` in `src/icd/d3d/`
- [ ] T017 Write `tests/unit/icd/d3d/test_umd_root_open.cpp` first, then `UmdRoot::open_adapter10_2` / `open_adapter12` and `UmdBinding` in `src/icd/d3d/`: a null argument is `E_INVALIDARG`; null callbacks are `E_INVALIDARG`; an `Interface` the list does not serve is `E_NOINTERFACE`; no rvkgpu adapter is `E_FAIL`; the connection is opened by the first open that needs it and closed by the last close; the allocation-failure sweep over every site answers `E_OUTOFMEMORY`, leaves `live_allocations() == 0`, `open_objects() == 0` and one log line
- [ ] T018 `src/icd/d3d/umd_entry_points.{hpp,cpp}`: the table thunks, one line each, resolving the handle and calling the object; `UmdRoot` fills both adapter tables with them. `tools/check_design.py` and `tools/scope_marks.py --apply` pass

### The DLL

- [ ] T019 `src/um/d3d/umd_library.{hpp,cpp}`, `rvkumd_exports.cpp` and `rvkumd.def`: the `IcdLibrary` pattern - a `constinit` library in the exports' translation unit holding one `ObjectStorage<Seams>` (`ProcessHeap`, two `SrwLock`s, `FileLog`, `GdiKmt`, `UmdRoot`, `ProcessTrace`), the two exports one line each, a DLL entry that does nothing, `_purecall` and `_fltused`
- [ ] T020 `src/um/CMakeLists.txt`: the `rvkumd` DLL target beside `rvkvk` and `rvkgl` - `src/icd/common`, `src/icd/d3d`, `src/guest/kmt`, `src/guest/transport`, `src/proto`, `src/core/support`, `src/core/trace`, `src/um/trace`, `src/um/icd`, `src/um/d3d`, `gdi_kmt.cpp`, `adapter_search.cpp`, `stack_probe.cpp`; `/ENTRY:rvkumd_entry /DEF:.../rvkumd.def`; imports gdi32, kernel32, ntdll only. `CMakeLists.txt` gains `rvkumd.dll` to the external project's byproducts and the `rvkumd_image` test (`--dll --only-exports --require-export OpenAdapter10_2 --require-export OpenAdapter12`)
- [ ] T021 `src/km/rvkgpu.inf`: `UserModeDriverName` becomes the four-entry `REG_MULTI_SZ` of contracts/umd_interface.md, and `rvkumd.dll` joins `[Rvkgpu_Files]`; the comment at line 40 that says the value is a placeholder until feature 055 is replaced by what it now means

**Checkpoint**: a fake runtime opens the adapter both ways and every table slot answers. Commit.

---

## Phase 4: US1 and US2 - the device object model and what is reported (Priority: P1)

**Goal**: the runtime's memory holds a cookie, the driver's pool holds the device, and the caps the adapter answers are the ones it owns.

- [ ] T022 Write `tests/unit/icd/d3d/test_device_cookie.cpp` and `test_device_table.cpp` first: `calc_private_device_size` is `sizeof(DeviceCookie)` for every `Interface`, `Version` and `Flags`; a block with the right magic and a pointer the table gave resolves; a scribbled magic, a pointer never given, a pointer of a destroyed device and a null block each answer null and dereference nothing; the sum of the adapters' device counts is the pool's size. Then `DeviceCookie` and `DeviceTable` in `src/icd/d3d/`
- [ ] T023 Write `tests/unit/icd/d3d/test_device_removal.cpp` first (the first cause is the one kept; later ones are counted and dropped; `check()` logs once, naming the entry that noticed; a removed device answers its code and asks the transport nothing), then `DeviceRemoval` in `src/icd/d3d/device_removal.{hpp,cpp}`
- [ ] T024 Write `tests/unit/icd/d3d/test_umd_device.cpp` first, then `UmdDevice` and `UmdRoot::create_device` / `destroy_device` / `close_adapter`: the runtime's handles and both callback pointers are recorded (pointers, never copies); a create on an adapter handle this driver did not give writes nothing; a create answers `E_NOTIMPL` after the object model and the seam are built, and the object is given back so nothing leaks; destroy is idempotent for a cookie this driver did not write; `close_adapter` with a live device is `E_FAIL`
- [ ] T025 Write `tests/unit/icd/d3d/test_umd_caps.cpp` first, then `pfnGetCaps` for both runtimes: the identity caps answer from `AdapterIdentity` into a buffer of exactly the right size, a short `DataSize` is `E_INVALIDARG` and nothing is written, a null `pData` with a non-zero `DataSize` is `E_INVALIDARG`, and a table names every other `D3D10_2DDICAPS_TYPE` and `D3D12DDICAPS_TYPE` this driver refuses with `E_INVALIDARG`
- [ ] T026 `UmdRoot::report(StateWriter&)` and `invariants_hold()`: `umd/0` with the adapter and device counts, then a line per adapter and per device; `busy=true` and no wait when the lock is held elsewhere. Test asserts the exact dump, as the ICDs' tests do

**Checkpoint**: devices are made and unmade through the runtime's own memory, and nothing untrue is reported. Commit.

---

## Phase 5: US3 - the DXGI seam (Priority: P1)

**Goal**: every one of the 21 slots is filled, and every one is tested.

- [ ] T027 Write `tests/unit/icd/d3d/test_dxgi_base_table.cpp` first: the revision follows `Interface`; a null callbacks pointer and a null functions union are each `E_INVALIDARG` before anything is written; an `Interface` naming a revision this driver does not serve is `E_NOINTERFACE`; a filled table has no null slot; the callbacks *pointer* is what is kept, and moving the runtime's table is seen by the next call
- [ ] T028 `DxgiBaseTable` in `src/icd/d3d/dxgi_base_table.{hpp,cpp}` and the 21 thunks in `umd_entry_points.cpp`, each one line
- [ ] T029 Write `tests/unit/icd/d3d/test_dxgi_entries.cpp`: one case per slot, asserting the answer contracts/dxgi_seam.md names - for the ten that answer `S_OK` with an all-zero capability, that the zeroes reached the runtime's buffer and nothing past it was written; for the eleven that answer `E_NOTIMPL`, that the device was not removed by the refusal. A slot reached on a removed device answers the removal code instead
- [ ] T030 `tests/fuzz/fuzz_open_adapter.cpp` with a committed corpus: the bytes drive a `D3D10DDIARG_OPENADAPTER` and a `D3D12DDIARG_OPENADAPTER` over the rig - every pointer either null or the rig's own, every scalar from the input - and no input crashes, leaks or leaves the root's invariants false

**Checkpoint**: the DXGI table is complete and judged slot by slot. Commit.

---

## Phase 6: the full-stack proof on Linux

- [ ] T031 `tests/loopback/test_fullstack_umd.cpp`: `FakeD3dRuntime` over a `UmdRoot` built on `DdiKmt`, so both opens, the identity queries and the device lifecycle cross the real driver and device core; the run ends with `kmt().open_objects() == 0` and `device().device().contexts() == 0`, and the log is dumped to stderr
- [ ] T032 Coverage: `cmake -S . -B build-coverage -DAV_COVERAGE=ON`, `ctest --test-dir build-coverage -j4 -LE tools`, `coverage_gate` green with no new `AV_UNTESTABLE` mark that is not justified here

**Checkpoint**: the whole 055 surface at 100% branches through the real driver. Commit.

---

## Phase 7: blocked on feature 053 - NOT STARTED

Nothing below may be started until `053-cross-api-sharing` merges. Written now
so the join is a known shape rather than a discovery (research.md §8).

- [ ] T033 [US1] One transport link per process, shared with the Vulkan, OpenGL and OpenCL drivers (FR-007, spec US1.4). Needs 053's device-global object identity: today each DLL has its own root and its own rvk context, and an allocation made through one is not nameable through another. `UmdConnection` is injected into `UmdRoot`, so the change is the object passed in - a shared connection owned by whichever DLL loads first, found by a named section or a named object - plus the lifetime rule for a process where rvkvk unloads and rvkumd does not
- [ ] T034 [US1] A test that two runtimes in one process (D3D11 and D3D12) get one transport and two devices (spec, Edge Cases). Meaningless until T033: today they already share one `UmdConnection`, but that connection is not the ICDs'
- [ ] T035 [US4] The allocation and resource private-data records themselves, nesting in 055's envelope. Their fields describe shared resources, whose identity is 053's; 056 and 057 own the formats (contracts/private_data.md)

---

## Phase 8: blocked on a real guest - NOT STARTED

- [ ] T036 [US1] FR-002: the minimum DDI set derived by reverse engineering the calls `d3d12core.dll` and `d3d11.dll` make, each recorded with the function it was read from, and replayed by `FakeD3dRuntime`. The workspace is `tools/re/build_workspace.sh` over `tools/re/import.py`; research.md §4 records the two things that trip it up (a Python with `pyghidra`, and a project path with no dot-prefixed element). research.md §12 holds the D3D12Core half (the open's table validation, the cleanup through `pfnCloseAdapter`, and the two pipeline-level caps it asks first); this task reads the D3D11 runtime's sequence and which `pfnFillDDITable` types D3D12Core asks for, and turns both into the fake's recorded sequence
- [ ] T037 [US1] In the guest: the package installs, the software key's `UserModeDriverName` reads back as the four-entry list naming files that exist in the driver store, and a probe creates a D3D11 and a D3D12 device on the rvkgpu adapter by LUID, recording which DDI entries were called (SC-001). Needs a guest boot
- [ ] T038 [US2] In the guest: `IDXGIFactory6::EnumAdapterByGpuPreference` and DXCore list rvkgpu with its description, ids, LUID and memory, and not as a software adapter; the figures match what `vulkaninfo` reports for the same LUID (SC-002). Needs a guest boot
- [ ] T039 [US3] In the guest: a windowed and a fullscreen swapchain present frames that reach the host with DWM on WARP (SC-004). Needs a guest boot **and** feature 056's present
- [ ] T040 [US1] `tools/guest/run_guest_suite.py --d3d`: push `rvkumd.dll` with the package, run the D3D probes, pull the UMD's log into `diag/`. Needs a guest boot
- [ ] T041 Device loss end to end: a TDR in the guest makes every later call answer `DXGI_ERROR_DEVICE_REMOVED`, `GetDeviceRemovedReason` names the cause, and a new device on the recovered adapter works (spec, Edge Cases). Needs a guest boot
- [ ] T042 DWM restart: the UMD instance in the new DWM process opens the adapter cleanly and the old process's state is gone on the host too (spec, Edge Cases). Needs a guest boot

---

## Phase 9: owned by feature 061 - NOT STARTED HERE

Recorded so 061 has the list rather than rediscovering it (plan.md, "What
feature 061 will need from this").

- [ ] T043 An x86 arm of the guest user-mode build: there is none in this build at all
- [ ] T044 `UserModeDriverNameWow` with the same four entries naming the 32-bit `rvkumd.dll`
- [ ] T045 The mirror records size- and offset-checked on x86 as well as x64 - every pointer field moves every later offset
- [ ] T046 The table thunks as genuinely `__stdcall`, with decorated exports in a second `.def`; on x64 there is one calling convention and the portable pointers *are* the ABI, which is what makes 055's Linux tests possible and what stops being true at 32 bits
- [ ] T047 `tools/check_guest_image.py` learns an `i386` machine mode
