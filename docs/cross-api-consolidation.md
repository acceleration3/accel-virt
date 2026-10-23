# Cross-API consolidation: plan and status

The rule this work follows: a mechanism two or more graphics APIs (Direct3D
9/10/11/12, OpenGL, OpenCL, Vulkan) or binaries need is one shared class, and a
fix goes to every API at once, never to one.

This document has two parts. **Status** says what became of each item:
done (with its commits on `host-memory-source`), partly done, deferred or
skipped, and why. **The plan** below it is the audit as written on 2026-10-16,
before any of the work. It is kept unchanged as the record of what was
planned and why, so its line references point at the tree of that day
(930a7959), not today's.

## Status

### Before the waves

Three refactors ran alongside the audit and set the ground the waves built on.

| Work | Commits |
|---|---|
| One ordered transport: held commands live in the transport, a lease lives as long as its command's batch, one `run_held` barrier, send locks in `HostChannel` | daf849b7, 8c693582, 106bdb73 |
| Fixed caps on application-controlled counts replaced by growth: `GrowableArray`, `KeyedSlots`, `ClaimedRows` in core, then every table | 356b4e1d, 7e7730e7, cea56a9e, 08adcd48, f3c6508d, 7132c71d, 9838e13c, f74bb44a, 9ecaccf3, 7e9dd00f, 4c81d8c5, f7fc3718, 8fcab9fe, 45962948, 6d285c5f |
| Application memory: a process's own pages as host GPU memory, for D3D12 existing and write-watched heaps, `CL_MEM_USE_HOST_PTR`, `VK_EXT_external_memory_host` and `GL_AMD_pinned_memory` | 36363338, 69f1a52a, 7c28a44c, aaca5d85, 1b2ee8ec, 298f1433, 30780175, 8542a2a4, 7069c3b0, f907c356 |

### Bugs found by the audit

| Bug | Fixed by |
|---|---|
| B-1 OpenGL's pacing failures logged as `avvk:` | ca6c1220 |
| B-2 `HostMemorySource::report` walked its pool unlocked | 36363338, then 2c9c15e3 (`ReportGate`) |
| B-3 `VulkanBackend` remembered one instance and one device | 959ba194 |
| B-4 sRGB D3D11 textures shared to OpenCL became plain buffers | 17bcd08d |
| B-5 D3D9 slice pitch: guest and host disagreed | ff07ab41 |
| B-6 D3D query results could tear | b644fbcc |
| B-7 `note_dropped` flooded the log | 9832b460 |
| B-8 no driver read the host fault code (no DEVICE_RESET, no GUILTY_CONTEXT_RESET) | a9b23d39, 91029900, 22da1119 |

The dead D3D12 host fence family is deleted (baeab76c).

### Mechanisms

| Item | Status | Commits | Notes |
|---|---|---|---|
| M1.1 Keyed slot index | done | 356b4e1d, 7e7730e7 | `KeyedSlots` under every keyed table |
| M1.2 Host object registry | done | d3b0b9dd, 959ba194, 28a70168 | `ClGlMirrors` and `GuestDevices` stay on `KeyedSlots`: no kinds, parents or counts to share |
| M1.3 Guest dedupe index, id minting | done | 89586eb6 | The Vulkan dedupe index stays in `HandleRegistry`, its only user |
| M1.4 D3D device blocks, record stamps | done | 0b273b4b | |
| M1.5 Translation hook shape | done | ac3756b4 | OpenCL and Vulkan share the shape, not one class: null is a valid Vulkan handle. D3D waits until its wire types tag ids with a kind |
| M1.6 Per-session teardown | done | 9da14781 | |
| M2.1 Guest allocation owners | done | 1f4f61cd, 25b6781b | |
| M2.2 Import bookkeeping | done | d07ab22d | |
| M2.3 `guest_bytes` | done | 79e9298a | `GuestBytes` |
| M2.4 Host Vulkan memory, dma-buf ownership | done | 457b11a7 | Deferred: `HostImageExport` keeps its own image description (memory type, pitch); switching risks NVIDIA's 256 MB BAR heap and needs GPUs to check |
| M2.5 Small memory items | done | 2e51a0f5, 055ab1c8 | Also `GlErrorFlags`, a latent hang on a lost context |
| M3.1 Guest waits on host completion | done | bf7af4ab, e2de4d0a | Deferred: `glGetQueryObject*(GL_QUERY_RESULT)` still blocks a host worker |
| M3.2 Spin budget | done | 921080eb | D3D11's per-query GetData patience stays: it spans calls |
| M3.3 Published results | done | baeab76c, 9f786491, b644fbcc | Step 4 (published status per API) not done; it was optional |
| M3.4 GPU timeline | done | e47a1d94 | |
| M3.5 Locked reply window | done | daf849b7 | Done by the ordered transport |
| M3.6 Host polls and wake | done | fadc6200, 8e6dc92b; working tree | DXVK patch 0009 gives D3D11 its wake. OpenCL pacing and Vulkan submit timing now read `MonotonicClock` too |
| M4.1 Arena carving | done | 08f5ab17 | OpenCL staging stays a span of its own: a lease taken outside a command could be retired by another thread's batch |
| M4.2 Transfer piece sizing | done | a81fadce | |
| M4.3 Pitched extent, row copier | done | ff07ab41 | |
| M4.4 Staged CPU maps | done | 1aa55c45 | D3D9's lock state is not folded: a field per subresource, and its unlock needs D3D9's own ranges |
| M4.5 Buffer slices beyond D3D11 | skipped | | Not a consolidation: it needs new host work (a D3D9 wire command and DXVK patch, a real OpenGL `gpu_progress`) and GPU checks |
| M4.6 `BgraRows` | done | working tree | GL front-buffer reads, GL/Vulkan's present queue, D3D11 readback and the device's present copier use the same bounded row conversion |
| M5.1 One DXGI format row | done | 17bcd08d | |
| M5.2 `FourCc` | done | 2fc25587 | |
| M5.3 Presentable format | done | 679a4867 | |
| M5.4 Window-slot formats | done | 0eb4f91c | |
| M5.5 Format caps in bulk | done | 6aeb782f, 0eddf4ea | D3D12 and, under the every-API rule, D3D9 too: one query per adapter each |
| M6.1 Chain slots | done | 3d3294e2 | |
| M6.2 Present pacing | done | 1d57889d | |
| M6.3 Window extent | done | c829925e | |
| M6.4 Present queue | done | 4d878c92, 2a16f884 | Vulkan presents asynchronously. `FramesInFlight` not moved: the queue's slots already bound how far an application runs ahead |
| M6.5 Host D3D present points, renewal order | done | c0ecdd50, 4185a917 | The `DxvkVulkanDevice`/`D3d9VulkanLane` connect logic is not folded |
| M6.6 D3D guest present scaffolding | done | 473c56fb | |
| M7.1 Shared allocation data | done | 8efb7796 | No hostile tests or fuzz target yet (no-tests order) |
| M7.2 Vulkan imports through `OpenedObjects` | done | a5a4599d | |
| M7.3 `SharedRowsTexture` | done | 1b133387 | |
| M7.4 D3D share state, D3D import | partly | f8b3a059 | `D3dShares`/`D3dShareState` done. D3D import through `ExportableImages::adopt` not built: no guest can reach it, the D3D driver never sends the open |
| M7.5 Copy mirrors | not done | | A write-only mirror skipping its copy-in was tried and dropped: a partial kernel write would hand stale bytes back to GL. Correctness kept |
| M7.6 One Vulkan sharing path | done | 7b637635 | Also closes fd forgery through `vkImport/GetFenceFdKHR` and friends |
| M8.1 Loss latch | done | a9b23d39, 91029900, 22da1119, 4de0170a | |
| M8.2 Adapter facts, memory, caps | done | d6756429, 52b3c79d, 260b034b, 07dd38e6 | |
| M8.3 Link generations | done | aa2312b8 | |
| M8.4 Extensions, version capping | done | 5a12cf5e, 9ebb7ce0 | `VK_EXT_external_memory_host` and `GL_AMD_pinned_memory` stay unreported until a test names them |
| M8.5 Host D3D loss and renewal | done | 6e2f40d3 | The current `VulkanBackend::lose_device` already calls its adopting device's `VulkanDeviceTable::lose` |
| M8.6 Error spelling | not done | | Checked: about 13 sites, each `no_memory ? <API's OOM> : <its own error>`; no switch copies to merge |
| M9.1 Driver logging | done | 0337d6e3, 73f47ef9 | Not done: the QEMU device module still types `virtio-gpu-av:` at about 30 sites |
| M9.2 Refused calls | done | 9832b460, a0d32fcb | |
| M9.3 Lock-free rows | done | 7e7730e7 | `ClaimedRows` |
| M9.4 One lock contract | done | 49dbba89, cff6320a, 2c9c15e3, 5ae27870, 5ca0522b; working tree | `CallingThread` names GL and D3D callers through one Windows implementation. D3D9/11's `ReentrantCallLock` is portable over `ThreadLock&` and `CallingThread&` |
| M9.5 Settings | partly | 6990c3b8 | Host settings read once per root are parked on `w5-logs-beyond-scope` (b2c192ea) |
| M9.6 D3D call trace, clock | done | 9faf2594; working tree | `PollClock` and `PollBudget` now live in core/platform; `MonotonicClock` and `PerformanceClock` implement the same contract, used by host polls and D3D9/11 guest residency waits |
| M9.7 FNV-1a | done | b5e4a00e | |
| M9.8 Driver seams | done | 2f1b76e8 | |

### Fixes after the waves, found by the GPU matrix

| Fix | Commit |
|---|---|
| A session's dynamic buffers leave the slice store before its table lets them go | 1eb3e247 |
| `av_device` links `ThreadLockHold`; without it QEMU's module did not load | 5ca0522b |
| avgpu reports `CrossAdapterResource`, which D3D12 existing heaps need | 3e185d22 |
| A stopped worker abandons its ring; the main loop no longer waits out a compile backlog | 5a519333 |
| D3D12 pipelines compile on threads of their own | 9872ecba |
| A lent run maps its dma-buf from the first page, so NVIDIA lends too | 6d00ea9c |

### Verification

The follow-up working-tree changes below are implementation-only, on the user's
instruction: no unit-test or fuzz updates. The verification results following
this paragraph describe the committed waves, not a new GPU run.

- Every commit above builds the production targets, and `tools/check_design.py`
  and `tools/scope_marks.py` pass.
- Unit tests were not updated during this work, on the user's order, and many no
  longer compile: `ctest` and `tools/gate.sh` need a test pass before they build
  again.
- GPU matrix: run `consolidated-3` passed every suite on the RX 9070 XT, the
  Radeon 610M and the Iris Xe. Its one failure, the RTX 3070 Ti's
  `existing_heap` step, is fixed by 6d00ea9c, checked on that GPU alone.
- Run `consolidated-4`, on the final code, passed every suite on both AMD GPUs.
  Its accel half (Iris Xe, RTX 3070 Ti) went unread: the host was down.

### Follow-up consolidation

- `BgraRows` owns a pitched pixel span, validates both rectangles before any
  write and supports unaligned and exact in-place red/blue conversion without
  touching row padding. Present scratch allocation and GL front-buffer offsets
  also reject arithmetic that cannot be addressed.
- `SortedSearch` owns a view of a growable array ordered by a numeric key. The
  lower-bound, upper-bound, predecessor and exact-match searches are shared by
  D3D12 GPU address translation, host OpenCL ranges, guest OpenCL SVM mirrors,
  GL live and retired syncs, guest suballocation ranges, host blob placements
  and the miniport's GPU virtual address space. It caches no array address,
  allocates nothing and handles zero and all-ones keys. Overlap, retirement,
  allocation and API-specific refusal policies remain with their owners.
- Host OpenCL pacing and Vulkan submit timing use the existing `MonotonicClock`
  instead of separate `clock_gettime` conversions. The shared `PollClock` /
  `PollBudget` contract now serves host GPU polls and guest residency waits.
- GL thread slots, D3D shared-row requests, call tracing and reentrant device
  locks use `WindowsCallingThread`; trace-host thread identification stays
  separate because calling traced driver code there would recurse.

Follow-up verification: native production libraries (`av_core`, `av_device`,
`av_executor`, `av_icd`, `av_gl_icd`, `av_cl_icd`, `av_d3d_umd`), x64 and x86
Windows graphics DLLs and `avgpu.sys` build. `tools/check_design.py` and
`tools/scope_marks.py` pass. No test or fuzz changes are included, and no new
GPU matrix run is claimed.

### Second follow-up: geometry and Direct3D present ordering

- `MipExtent` replaces the repeated per-axis mip shifts in the D3D11 guest,
  host D3D11, D3D12 copies and OpenCL's D3D11 sharing descriptions. Shifts past
  a scalar's width are defined, and every mip dimension remains at least one.
- `SubresourceLayout` shares mip/layer/plane numbering across those callers
  and D3D9 cube/2D texture writes. Zero and overflowing subresource counts are
  rejected before division or indexing. D3D12 keeps its own plane-count policy.
- `PresentOrder` shares D3D9/11's host-batch follow and post-present reuse gate.
  D3D9 primary allocations now use the same context ordering as D3D11 flips;
  point-number delivery remains API-specific (D3D9 escape, D3D11 private data).
- D3D9's CPU primary fallback no longer uploads an earlier completed flip. It
  stages the current frame using the existing `D3d9FramePool`, waits within a
  deadline outside the D3D11 device lock, and publishes that frame or fails.
  This removes the separate delayed-copy ring and its unbounded busy wait.
  `QueueWork::wait`, shared by D3D9 and D3D12 composition, now flushes the
  borrowed runtime queue before inserting an external timeline wait, so the
  wait cannot overtake commands still on its recording thread. The D3D9 GPU
  image-copy path otherwise remains unchanged. These are code-level corrections,
  not a claim that the reported game symptom has been reproduced or resolved.

---

## The plan, as written on 2026-10-16

Repo: `external/accel-virt`, branch `host-memory-source` @ `930a7959`. Read-only audit: no file changed, nothing built.
All paths are under `src/` unless they start with `tests/`, `tools/` or `docs/`. `file:line` refers to that commit.
Every finding was read from code; the eight bugs in §2 were re-checked a second time by hand.

Goal (user): *every mechanism that could be shared between the graphics APIs is one shared class used by all of them.*

Placement rules applied (CLAUDE.md, docs/architecture.md):
- `src/core/support` — needed by more than one *binary* (guest UMDs + executor, or + miniport/device).
- `src/proto/<area>` — a layout or value both sides of a boundary must agree on (proto includes only core).
- `src/icd/common` — ≥2 guest user-mode drivers use it; its Windows half goes to `src/um/common` (also ≥2 users).
- `src/guest/transport` — the av transport and host memory as a process holds it.
- `src/executor` (root) — shared by ≥2 executor backends; `executor/sharing` for cross-API shares.
- Decisions never go in a `shim/` directory (exempt from coverage).
- One class per header, no static members, no speculative interfaces: a virtual seam needs ≥2 real implementations.
- No migration or back-compat code (user memory): wire/layout changes break and regenerate.

---

### 0. Contents

1. What is already shared (leave alone)
2. Bugs found on the way (fix regardless of refactor)
3. What the three in-flight branches leave behind
4. Mechanism catalogue (M1–M10), each with copies, difference, must-differ, proposal, order, risk, tests
5. Mechanisms that must stay per-API
6. Decisions taken where the per-area audits disagreed
7. Hot files (touched by several items: serialize)
8. Waves and lanes for parallel agents

---

### 1. Already one shared mechanism — no work

| Mechanism | Shared class | Users |
|---|---|---|
| Guest pointer judging | `icd::HandleTree<Node>` (icd/common/handle_tree.hpp:37), `ObjectPool::find` | Vulkan `HandleRegistry`, CL `ClObjectTable`, D3D `AdapterTable`/`DeviceTable` |
| Library root singletons | `icd::RootBinding<Root>` (icd/common/root_binding.hpp:18), `LazyDriver<Seams>` + `RunOnce` (um/common/lazy_driver.hpp) | all four graphics DLLs; `VendorDriver` for ADL/NVAPI |
| Reply-slot waiting | `ClientTransport::send_sync` → `KmtTransport::await` (`await_slot` guest/transport/kmt_transport.cpp:736, `fetch_answer` :691), `ReplyReader`, `HostWatch`, `SyncCallScope` | all APIs |
| Host reply writing | `SessionBackend::run(..., WireWriter &reply, ...)` (executor/session_backend.hpp) | all backends |
| Executor host-thread wake | `av::WakeSignal` (executor/wake_signal.hpp) | D3D12 fence wake, CL event signals, `DeviceTenants` |
| Guest service thread | `icd::ServiceThread`/`ServiceLoop`, `icd::OnceLatch` | CL event watcher, GL present loop, … |
| Guest host memory | `HostAllocation` (guest/transport), `HostChannel::open_memory` (icd/common/host_channel.cpp:103), `KmtBlobArena` | Vulkan device memory, D3D12 heaps, D3D11 slice store |
| Host memory | `HostMemorySource`, `SourceMemory`, `ImportTable`, `GuestRegion`, `GuestMemoryTypes`, `HostMemoryRequest` | all backends |
| Transfer slicing | `icd::Pieces`, `icd::RowPieces`, `icd::RangeTable`, `FramedBlock`, `SharedBytes` | D3D, GL, CL (Vulkan `CommandBatch` uses FramedBlock) |
| Texel layout | `TexelLayout` (proto/resources) + `DxgiFormatLayout`, `D3d9FormatLayout` (proto/d3d) | um/d3d, executor D3D9/11/12, CL transfer chunks, GL back buffer |
| Host shares | `HostShares`, `SessionShares` (executor/sharing), `SessionBackend::export/import/release_object` | every backend |
| Guest NT-handle sharing | `icd::SharedHandles`, `icd::OpenedObjects`, `SharedObjectCall` | GL, CL (Vulkan only partly — see M7.2) |
| Host dma-buf → GL | `GlHostImport` (executor/shim) | OpenGL and OpenCL backends |
| Present staging (host) | `FramePool<Frame>`, `FrameClaim`, `StagedFrame`, `SharedSurfaces` | D3D9/11/12 |
| Guest pacing/window seams | `icd::FramePacer`, `WindowSurface`(+`GdiWindowSurface`), `WindowChains`(+`DxgiWindowChains`) | GL, Vulkan |
| DXBC parsing | `DxbcContainer`, `DxbcDigest`, `DxbcSignatureReader` | D3D11, D3D12 |
| State-dump gate (guest) | `icd::ReportGate` | all ICD roots |
| Lost-device tenants | `DeviceTenants` (executor/device_tenants.hpp) | D3D11, D3D12 host devices |

---

### 2. Bugs found on the way (verified; independent of the refactor)

These are cheap and should not wait for the waves; each becomes the first failing test of the item named.

| # | Bug | Evidence | Fixed by |
|---|---|---|---|
| B-1 | OpenGL's pacing failures are logged as `avvk:` — `FramePacer` in icd/common hard-codes the Vulkan prefix; the test asserts the wrong text | icd/common/frame_pacer.cpp:25; tests/unit/icd/common/test_frame_pacer.cpp:21 | M9.1 (inject `DriverLog&`) |
| B-2 | Data race: `HostMemorySource::report` walks `memories_` without `mutex_` from the dump thread while `allocate`/`release` mutate it under `mutex_` | executor/host_memory_source.cpp:222-233 vs :130-150; caller executor/executor_root.cpp:369 | M9.4 (`ReportGate` over a `PosixLock`) — or a one-line trylock now, as `HostShares::report` does (executor/sharing/host_shares.cpp:432) |
| B-3 | `VulkanBackend` remembers one `instance_` and one `device_`; a context that made two devices/instances and died without destroying them leaks the first or destroys the instance under a live device | executor/vulkan/vulkan_backend.cpp:219, 326-328, 877-893 | M1.2 (`HostObjectRegistry::release_all`) |
| B-4 | sRGB D3D11 textures shared to OpenCL become a plain buffer: guest CL table and `ExportableImageInfo` accept DXGI 29/91, host `DxgiClFormat` does not, so `image_over` silently answers the buffer | icd/opencl/dxgi_image_format.cpp:17-58; executor/opencl/dxgi_cl_format.cpp:18-30; executor/opencl/opencl_backend.cpp:983-986 | M5.1 (one `DxgiFormat` row with a "host views as CL image" column) |
| B-5 | D3D9 slice pitch: guest accepts `slice_pitch >= (rows-1)*row_pitch + line`, host demands `>= row_pitch*rows`; a guest-legal write between those is refused by the host | icd/d3d/texture_upload.cpp:49 vs executor/d3d/d3d9_texture_write.cpp:27 | M4.3 (`PitchedExtent`, one rule) |
| B-6 | `QueryResults::read` checks the slot's count, copies, never re-checks: a host rewriting the slot mid-copy (id%512 wrap, re-End) tears the result | um/d3d/query_results.cpp:47-55 | M3.3 (`PublishedSlotReader` seqlock) |
| B-7 | `GlIcdRoot::note_dropped` logs every dropped command, no once-guard: a per-frame overflow floods the log | icd/opengl/gl_icd_root.cpp:651-655 | M9.2 (`RefusedCalls::dropped`) |
| B-8 | The transport's host fault code is never read by any driver; D3D always answers DEVICE_REMOVED (never RESET), GL always `GL_UNKNOWN_CONTEXT_RESET` | `KmtTransport::host_fault_code` (guest/transport/kmt_transport.cpp:366) has only guest/suite as caller; icd/d3d/device_removal.hpp:3-5 | M8.1 (`LossLatch`) |

Also dead code: the whole D3D12 host fence family (`CreateFence12`, `SignalFence12`, `WaitFence12`, `FenceValue12`, `PublishFence12`, `D3d12FencePublisher`, `D3dObjectKind::fence12`). No guest code sends any of them (`grep Fence12 um icd` is empty; the UMD reports no native fences, um/d3d/d3d12_device.cpp:355-366). `on_FenceValue12` also blocks unbounded (executor/d3d/d3d12_session.cpp:831). Delete per constitution VIII (M3.3 step 1).

Rule violations seen: free functions in icd/opencl/cl_extension_policy.cpp:9-26 and icd/opencl/cl_device_report.cpp:52-90; the `IcdConnection` test sits at tests/unit/icd/opengl/test_gl_connection.cpp instead of tests/unit/icd/common/; decision logic (device renewal) in executor/shim; `um/common/windows_wake_signal.*` has one user (um/opengl); `D3d12HostDevice::renew` repeats unlock per branch (executor/shim/d3d12_host_device.cpp:128-146, against the RAII rule); `ExtensionPolicy` in icd/common is Vulkan-only (`VkExtensionProperties`); stale comment in icd/common/range_table.hpp:1-8 naming removed classes.

---

### 3. What the in-flight branches leave behind

The three worktrees (`~/.cache/claude-worktrees/{av-growable,av-ordered-transport,re4b}`) had no commits or diffs yet at audit time. Their scope is taken from the brief.

#### growable-tables (fixed caps → growable containers in core/support)
It removes the *caps*, not the duplicated *logic*. Left behind:
- Two hash indexes with different probing and delete (Vulkan backward-shift at executor/vulkan/handle_registry.cpp:147, D3D tombstones at executor/d3d/d3d_object_table.cpp:56-67), and two lazy-mmap stores (`handle_registry.cpp:101`, `d3d_object_table.cpp:32`). CL's registry is still an O(n) scan (executor/opencl/cl_handle_registry.cpp:124). `ClGlMirrors` keeps a third private open-addressing index (executor/opencl/cl_gl_mirrors.hpp:22-56) → M1.1.
- Kind checks, parent cascade, refcount and kind-ordered release written per API → M1.2.
- Linear guest lookups (icd/vulkan/handle_registry.cpp:41-130, `SharedHandles`/`OpenedObjects` row scans icd/common/opened_objects.cpp:18-37) → M1.3.
- Two lock-free append-only tables (`KeyedLog::first` core/log/keyed_log.cpp:13-30, `ClInfoAnswers` icd/opencl/cl_info_answers.hpp:46-105). **They need chunked, address-stable growth**: a lock-free reader cannot follow a realloc. → M9.3.
- Three name-pointer lists and two word lists for extensions → M8.4.
- Adapter caps disagree: 16 in `AvgpuAdapters`/`InstanceSpan`/`VendorAdapters`, 8 in `UmdConnections`/`AdapterSessions`/`ClDevices` (guest/kmt/avgpu_adapters.hpp:28, icd/vulkan/instance_span.hpp:24, icd/d3d/umd_connections.hpp:475, um/d3d/adapter_sessions.hpp:29, icd/opencl/cl_devices.hpp:226) → M8.2.
- **Ask growable-tables to skip** what this plan deletes: `AdoptedBlobs`, `ImportedMemories`, `DeviceMemory::held_` (M2.1/M2.2), `HostExportedFds` (M7.6), `D3d12FencePublisher` (M3.3). It must also not keep the "registry full ⇒ lost" paths (vulkan_backend.cpp:119, opencl_backend.cpp:429).
- Fixed caps the audits found that growable-tables should cover:
  - `HostExportedFds::max_fds=64` cites a removed constant (host_exported_fds.hpp:41).
  - `UniformShapes::capacity=1024` (icd/opengl/uniform_shapes.hpp:20).
  - D3D9 `trace_most=20000` (um/d3d/d3d9_device.cpp:26).
  - `max_swapchains` 16 / `max_surfaces` 32 (icd/vulkan/presentation.hpp:41, surface_table.hpp:28).
  - `DxgiWindowChains` `max_windows` 16 / `max_devices` 4.
  - `GlPresentLoop::max_frames` 32, `WindowFramebuffers::max_windows` 8.
  - `D3d11Device::max_guest_surfaces` 256.
  - `TransportFence::max_outstanding` 64.
  - `ClEventSignals` 128 / `ClEventWatcher` 256.
  - `QueryResultPublisher::pending_capacity` 256.
  - `IcdConnection::max_links` 4, `AdapterSessions::generations` 4.
  - `DeviceTenants::capacity` 64, `KeyedLog::max_keys` 256.

#### ordered-transport (holding/batching, blob-lease lifetime, send ordering)
It owns `KmtTransport`, `BatchedTransport`, `CommandBatch`, `D3dCommandStream`, `PublishGate`, `UnsentBatch`, `HostChannel`, `LockedTransport` and leases. Left behind:
- **Reply wait**: stays in `KmtTransport` (already shared).
- Five copies of the perf-counter spin budget inside `KmtTransport` plus CL/D3D copies → M3.2.
- `D3dDeviceChannel` re-implements `LockedTransport` line for line (icd/d3d/d3d_device_channel.cpp:117-170 vs icd/common/locked_transport.cpp) → M3.5.
- `StreamTimeline` (D3D) vs GL `end_frame` on the same transport-fence triple → M3.4.
- Arena carving: CL's private `staging_` arena, D3D's lease limit, query tail and slice books, all hand-cut → M4.1.
- Per-API transfer piece sizes: GL's `glIsBuffer` lease-retire hack (icd/opengl/buffer_mapping.cpp:221-231), D3D `make_room_for` → M4.2.
- Arena-tail layout (error word, query slots) and every host→guest signal path → M3.3.
- The "flush what is held before another API/process names the object" barrier exists at three sites (D3D11 key publish um/d3d/d3d11_device_objects.cpp≈190, CL `GlShareSource::run_held_calls` icd/opencl/gl_share_source.hpp:27-31, D3D12 heap import commit d29edbe7). **This belongs inside ordered-transport** as one "publish barrier"; flag it to that agent rather than re-plan it.
- Five copies of "open an av context on an adapter" (`GuestConnection::open` + `HostChannel::open` + `LockedTransport`): icd/vulkan/connection.cpp:26-43, icd/common/icd_connection.cpp:221-236 + GL/CL `build`, um/d3d/d3d_session.cpp:79-92, um/d3d/adapter_sessions.hpp:124-135 → M8.3.

#### RE4 agent (application-pointer memory on one mechanism)
It will touch `SessionBackend::adopt`, `ImportedMemories`/`AdoptedBlobs`, `executor/shim/vulkan_memory_import.cpp`, the vkd3d heap import, `MappedRegion::Memory::application`, `icd/common/extension_policy.cpp:10` (`withheld` VK_EXT_external_memory_host), probably `icd/vulkan/memory_type_table.cpp` and `HostMemorySource`. Left behind:
- Import bookkeeping kept three times → M2.2.
- dma-buf fd ownership and the Vulkan import primitive done five ways → M2.4.
- Two Vulkan external-memory function tables → M2.4.
- Staged CPU maps written four times → M4.4.
- Everything in M2.2, M2.4, M4.4, M8.4-ExtensionRows and M8.2-AdapterMemory must start **after** RE4 merges.

---

### 4. Mechanism catalogue

Format per item: **Copies** · **Differ** · **Must differ?** · **Proposal** (class — directory — one-sentence responsibility) · **Order** · **Risk** · **Tests** (existing → add).

#### M1. Object tables, ids, translation, session teardown

##### M1.1 Keyed slot index (storage under every host and guest table)
- **Copies**:

  | Class | Storage | Lookup | Delete |
  |---|---|---|---|
  | Vulkan `HandleRegistry` (executor/vulkan/handle_registry.hpp:44) | lazy mmap | open addressing, multiplicative hash | backward shift |
  | D3D `D3dObjectTable` (executor/d3d/d3d_object_table.hpp:81) | lazy mmap | open addressing, Knuth hash | tombstones, refuses past 3/4 load (.cpp:107) |
  | CL `ClHandleRegistry` (executor/opencl/cl_handle_registry.hpp:24) | fixed 2048, held ×64 by value in `ExecutorRoot` | linear | swap-remove |
  | `ClGlMirrors` | private index | open addressing | — |

  The same shape (linear key→slot, fixed, `forget_device`) also appears in `HostTemplateTable`, `HostExportedFds`, `ImportTable`, `GuestDevices`, guest `SharedHandles`/`OpenedObjects`.
- **Differ**: hash, delete scheme, storage source. **Must differ?** No.
- **Proposal**: `KeyedSlots<Entry>` — `core/support/keyed_slots.hpp` — *Finds the slot a 64-bit key occupies among entries kept in memory from a `BlockSource`.* Backward-shift delete (Vulkan's algorithm), free list, growth by doubling; slots stay address-stable (entries separate from the index) so D3D's view side-index `note_viewed` (d3d_object_table.hpp:121-127) still works. Core, because guest tables use it too.
- **Coordination**: if growable-tables builds a growable *keyed* container, that *is* `KeyedSlots`; otherwise build it on growable-tables' array.
- **Order**: class → CL registry → `ClGlMirrors` → Vulkan → D3D (hottest, ~100 hand lookups: d3d_backend.cpp 54, d3d12_session.cpp 23, d3d12_session_recording.cpp 20, d3d12_session_tiles.cpp 9, d3d9_session.cpp 5).
- **Risk**: medium (D3D lookup cost per command, RE4's 196k objects).
- **Tests**: test_handle_registry, test_cl_handle_registry, test_d3d_object_table, test_cl_gl_mirrors, fuzz_executor_{vulkan,opencl,opengl} → add `test_keyed_slots` (key 0, all-ones, colliding keys, delete mid-probe-chain, growth at every doubling, slot stability across delete).

##### M1.2 Host object registry semantics (kind, parent cascade, refcount, ordered release)
- **Copies**:
  - Vulkan: kind + parent + child count + cascade `forget_children`/`sweep_orphans` (handle_registry.cpp:184).
  - CL: kind + saturating refcount + kind-ordered `release_all` (cl_handle_registry.cpp:71-112).
  - D3D: kind ranges per sub-API (`remove_kinds` d3d12_session.cpp:146, d3d9_session.cpp:50) + one COM ref per entry.
- **Must differ?** Release *action* differs (vkDestroy*/clRelease*/IUnknown::Release); the bookkeeping does not.
- **Proposal**: `HostObjectRegistry<Kind>` — `executor/host_object_registry.hpp` — *Holds the host objects one session's guest may name, by the number the guest names each with.* Entry `{key, kind, parent slot, references, object, u64 payload}`. Operations: `remember`, `find(key,kind)`, `retain`/`release`, `forget` (cascades), `forget_kinds(first,last)`, `release_all(ObjectReleaser&)` deepest-kind first. `ObjectReleaser` is a virtual with three real implementations (CL, D3D, Vulkan). `HostTemplateTable` folds in as Vulkan entries with payload = bytes, parent = device, removing `forget_device` (vulkan_backend.cpp:614-615). `GuestDevices` keeps its D3D selection/removal state but its storage moves onto it. `SyncTokens`/`RetiredSyncTokens` stay GL (bounded retirement is GL policy) and may use `KeyedSlots`.
- **Order**: after M1.1 — CL → Vulkan (fixes B-3) → D3D.
- **Risk**: medium; teardown order vs `HostShares`: imports must still be released by `HostShares` first (opencl_backend.cpp:508-509).
- **Tests**: test_vulkan_backend_lifetime (cascade cases :304-529), test_opencl_backend, test_d3d_backend, test_d3d12_session, test_d3d9_session, test_host_template_table → add: two VkDevices + two instances closed without destroy (fails today); 10k CL mems (fails today at 2048); `release_all` order per kind.

##### M1.3 Guest dedupe index and guest id minting
- **Vulkan dedupe**: `icd::HandleRegistry::existing/child/instance_of/live_on/batches/memories` walk the whole pool (icd/vulkan/handle_registry.cpp:41-130); `wrap` calls `existing` per returned handle, so `vkAllocateCommandBuffers` is O(n²).
  - Proposal: an optional `KeyedSlots` index inside `HandleTree` (`HandleTree::find_named`), keyed by `(kind, host, connection)`.
  - Tests: test_handle_tree, test_icd_handle_registry, test_loader_handle, test_cl_object_table.
- **Id minting**, three counters:
  - D3D atomic u32 skipping 0 (um/d3d/d3d_session.cpp:187-193); after 4G creates the host refuses the duplicate insert (d3d_object_table.cpp:107).
  - GL sync tokens atomic u64 (icd/opengl/gl_icd_root.cpp:625).
  - GL external names non-atomic u32 (icd/opengl/gl_external_objects.cpp:61-71).
  - Proposal: `IdMint` — `icd/common/id_mint.hpp` — *Hands out non-zero ids in increasing order to any thread.* D3D stays u32 on the wire (a wire widening is out of scope; note it).
  - Tests: test_gl_icd_root, test_gl_external_objects → add test_id_mint (zero skipped, wrap).
- **GL context handles** (`GlContextTable` slot+16-bit generation in 256 slots, icd/opengl/gl_context_table.hpp:27,50-52): one user, so leave it. After growable-tables, the pool address could serve as DHGLRC (like `AdapterTable`) *if* opengl32 rejects stale HGLRCs itself; check with the GL conformance run. Low priority.

##### M1.4 D3D runtime-block readers and record stamps (D3D-only, but three copies)
- `d3d11::DeviceHandle` and `d3d12::D3d12DeviceHandle` are line-for-line twins (um/d3d/device_handle.cpp:41-54, um/d3d/d3d12_device_handle.cpp:13-27).
  - Proposal: `DeviceBlock<Device>` — `icd/d3d/device_block.hpp` — *Reads and writes the device address kept at `DeviceTable::extension_offset` of a runtime-owned block.* The generator for `um/d3d/generated/d3d11_device_thunks.cpp` (175 uses) and `d3d12_thunks.cpp` (121) changes too.
- "magic + id" header three ways: `DeviceCookie` (icd/d3d/device_cookie.hpp:232), D3D11 records (um/d3d/d3d11_records.hpp:27-104, read raw at d3d11_device.cpp:110-117), D3D12 `RecordHeader` (um/d3d/d3d12_records.hpp:28-33). Tag checks are ad hoc in 7 files.
  - Proposal: `RecordStamp` — `icd/d3d/record_stamp.hpp` (DDI-record-style) — *The tag and guest id at the head of a runtime-allocated object block.*
- Tests: test_umd_device_model, test_umd_ddi_records → add test_device_block. Risk low.

##### M1.5 Guest↔host translation hook shape
- **Copies**:
  - Vulkan: weak free functions in the generated archive (proto/generated/vulkan/vk_handle_default.cpp), overridden in vulkan_backend.cpp:911-1000. They reach the backend through a **`thread_local VulkanBackend *inside`** (vulkan_backend.cpp:27, set :293-295): process-lifetime state outside the constitution's list.
  - CL: pure virtuals `handle_for/made/retained/released` on `clproto::ServerDispatch` (proto/generated/opencl/cl_dispatch_server.h:56-63).
  - D3D: no hook, about 100 hand `find(id, kind)` calls.
- **Proposal**: emit CL's shape for Vulkan from tools/codegen/av_codegen (virtual `handle_for`, `made`, `made_through`, `ended`, `children_ended` on `vkproto::ServerDispatch`); the guest side gets an identity implementation; delete the `thread_local`. D3D adopts generated `handle_for` later, once its wire types tag id fields with a kind.
- **Must differ**: the *minting direction*. D3D is guest-minted (batched creates, no round trip); Vulkan/CL are host-minted; GL names are the host's own, unchecked (icd/opengl/gl_external_objects.hpp:4-11). Converging is a protocol change, out of scope.
- **Tests**: test_vulkan_dispatch, test_vulkan_backend_lifetime (`outside_a_dispatch_an_ending_forgets_nothing` :529), fuzz_executor_vulkan, fuzz_generated_decode, test_fullstack_vulkan_icd. Risk medium.

##### M1.6 Per-session teardown on the executor
- **Today**: `ExecutorRoot::close` (executor/executor_root.cpp:265-284) releases imports, ends `SessionShares`, then calls `shut_down()` on **all four** backends of the slot (held by value ×64 each, executor_root.hpp:176-180), bound or not.
- **Proposal**: `virtual void shut_down() = 0` on `SessionBackend`; `close` calls it only on `backend_in(slot, apis_of_[slot])` (read before :277 zeroes it). Each backend's "objects the guest never destroyed" step becomes `HostObjectRegistry::release_all` (M1.2). GL keeps context-close plus the `SyncTokens::take_any` loop (GL semantics: the context frees its objects).
- **Tests**: test_executor_root, test_context_session, test_*_backend, test_device_tenants, test_fullstack_device_loss, test_loopback_hostile → add "closing a slot of API X never touches the other three backends". Risk low (virtual), medium (release order).

#### M2. Memory: allocation, imports, host-memory source

##### M2.1 Guest: who owns a host allocation
- **Copies**: `icd::DeviceMemory` keeps its own `HostAllocation *held_[4096]` with linear `find/slot_of` (icd/vulkan/device_memory.hpp:76-79, .cpp:71-87), while `HostChannel` already keeps the same objects (`memories_`) and has `find_memory` (icd/common/host_channel.cpp:131) with no production caller. Range checks: Vulkan `map` checks (device_memory.cpp:105-114), D3D12 `map_heap` does not (um/d3d/d3d12_device_resources.cpp:369-377).
- **Proposal**:
  - `HostChannel::open_memory(bytes, request, owner, made)` + `HostChannel::close_owned(owner)` (owner = opaque u64, the VkDevice).
  - `HostAllocation::view_at(offset, bytes)`: *the process's view of a range of the allocation, bounds-checked*.
  - Delete `DeviceMemory::held_/slot_of/find/count_/max_allocations` and the `Connection::find_memory` wrapper.
- **Order**: HostChannel/HostAllocation → Vulkan → D3D12 `map_heap`.
- **Risk**: low (test two devices on one connection).
- **Tests**: test_icd_memory, test_connection, test_host_channel, test_host_allocation, test_fullstack_host_memory → add `close_owned_leaves_other_owners`, `view_at_refuses_wrap`.

##### M2.2 Executor: what a backend made of an import (bookkept three times)
- **Copies**: `ImportTable` already records `object()`/`wire_handle()` per blob. On top of it:
  - `AdoptedBlobs` (executor/vulkan/adopted_blobs.hpp; used at vulkan_backend.cpp:624, 738, 828).
  - `ImportedMemories` (executor/imported_memories.hpp; used at d3d_backend.cpp:254/267/1733, d3d12_session.cpp:414).

  Both are linear scans capped at `max_imports`.
- **Must differ?** The host import call does (GuestRegion pointer / vkd3d heap / DXVK buffer / Vulkan dma-buf); the lookup does not.
- **Proposal**: `ImportTable::named(uint64_t wire_handle)` — *the live import the guest names by a wire handle*. The session hands its `ImportTable&` to the backend at construction. Delete `AdoptedBlobs` and `ImportedMemories`.
- **Order**: after RE4 + growable-tables. `named` → Vulkan → D3D.
- **Risk**: medium (`abandon` idempotence vulkan_backend.cpp:825-835).
- **Tests**: test_import_table (absorbs test_adopted_blobs/test_imported_memories cases), test_context_session, test_vulkan_backend(_device), test_d3d_backend, test_buffer_slice_device, test_d3d12_session_device, test_fullstack_host_memory.

##### M2.3 "Bytes at a guest offset of the region" (five overrides)
- **Copies**: generated hooks `guest_bytes`/`guest_bytes_out` (d3d, d3d9, d3d12 proto/generated/*_dispatch_server.h:29-30), CL `lent_for` (cl_dispatch_server.h:74), GL table callback (opengl_backend.cpp:64). Each override is one line, `region_.at(...)`: d3d_backend.cpp:336-346, d3d12_session.cpp:205-215, d3d9_session.cpp:77-87, opengl_backend.cpp:329-333, opencl_backend.cpp:383-387.
- **Proposal**: codegen emits one `GuestRegion&` injected into every `ServerDispatch` base with one non-virtual `guest_bytes(offset, bytes)`; delete the five overrides (tools/codegen/av_codegen emitters + regenerated proto/generated/*, tools/generated_sources.txt).
- **Order**: after M2.2 (same headers). Risk low-medium.
- **Tests**: per-backend decoder tests, test_loopback_hostile, fuzz decoders.

##### M2.4 Host Vulkan memory import/export, dma-buf ownership, exportable images
Several audits found pieces of this; it is one item for one agent.
- **dma-buf import primitive** (`VkImportMemoryFdInfoKHR` + allocate + "close fd only if not made") in five places:
  - `VulkanMemoryImport::import_dmabuf` (executor/shim/vulkan_memory_import.cpp:30-52) and `import_shared_memory` (:144-170, dups instead of taking).
  - `VulkanBuffer::import` (vulkan_buffer.cpp:24-50).
  - `VulkanImage::import_dma_buf/bind` (vulkan_image.cpp:31-55).
  - `ExportableImages::adopt_laid_out` and the vkd3d path (exportable_images.cpp:60-140).
- **Ad-hoc `close(fd)`**: 15 sites across executor (d3d12_session.cpp:420, vulkan_backend.cpp:729, host_shares.cpp ×3, vulkan_timeline.cpp ×2, …).
- **Two Vulkan device-call surfaces**: `VulkanMemoryImport` resolves via `vkGetDeviceProcAddr` on every call (vulkan_memory_import.cpp:16,23,115,132,177), while `VulkanDeviceTable` loads the same entries once (`memory_fd`, `dma_buf_types`, `create_semaphore`, `import_semaphore_fd`, `semaphore_fd`, `bind_buffer`).
- **Exportable image made three times**:
  - `VulkanImage` (vulkan_image.cpp:30-70).
  - `ExportableImages`' raw VkImage+VkDeviceMemory (exportable_images.cpp:146-276).
  - `HostImageExport` builds `VkImageCreateInfo` by hand with its own DXGI→Vk table (host_image_export.cpp:19-28, 48-90). Its `describe` (:137-150) duplicates `ExportableImages::export_image`.
- **Proposal**:
  - `DmaBuf` — `executor/dma_buf.hpp` — *Owns one dma-buf descriptor until a host import takes it.*
  - `VulkanDeviceTable::import_dma_buf(DmaBuf&, size, type, chain, VkDeviceMemory&)`: the one import primitive.
  - `VulkanMemoryImport` keeps its decisions but calls through a `VulkanDeviceTable` loaded once per guest `VkDevice` (held by `VulkanBackend`).
  - `ExportableImage` holds a `VulkanImage` (gaining modifier + `row_layout`).
  - `HostImageExport` makes its image through `ExportableImages::make_rows` (TRANSFER usage as a parameter).
  - One describer, `ExportedImageLayout` — `executor/shim/exported_image_layout.hpp` — *Describes one exportable image as a dma-buf importer must be told it.* It is translation, so shim is right.
  - Optional (device side): `ExportedImageMessage` (proto/renderer) embeds `ExportedLayout` (proto/export) instead of repeating its six fields.
- **Must differ**: binding target (buffer/image/memory); `ExportableImages`' TextureHold retire/collect and tiled-modifier choice; `HostImageExport`'s one persistent image + D3D11 query.
- **Order**: after RE4. `DmaBuf` → table primitive → callers VulkanImage → VulkanBuffer → ExportableImages → VulkanMemoryImport → HostImageExport/`ExportedImageLayout` → D3D12 heap import + HostShares on `DmaBuf`.
- **Risk**: medium-high. All shim (outside the coverage gate), so prove on all GPUs: local AMD, test Arc, accel Iris+NVIDIA; RADV/NVIDIA modifier quirks.
- **Tests**: test_compositor_memory_device, test_vulkan_backend_device, test_buffer_slice_device, test_d3d12_session_device, test_d3d_{external_export,gl_interop,vulkan_interop}_device, test_fullstack_shared_objects, test_fullstack_external_objects, gpu-test.sh matrix → add `dma_buf.closes_unless_taken`.

##### M2.5 Small memory items
- **GL buffer over imported memory, twice**: `GlPackBuffer::make` (executor/opengl/gl_pack_buffer.cpp, DSA) and `GlSharedBuffer::make` (executor/shim/gl_shared_buffer.cpp, bind). `GlPackBuffer` keeps its policy and makes/destroys through `GlSharedBuffer`. Tests: test_opengl_backend_device, CL-GL sharing device tests, gl_samples_loopback. Risk low-medium (driver DSA behaviour; all GPUs).
- **`ClReadAhead`** mmaps its own 8 MiB (executor/opencl/cl_read_ahead.cpp:17,38) instead of holding `StagingBytes` (staging_bytes.cpp:23-34). CL-only. Tests: test_cl_read_ahead, test_staging_bytes.
- **`HostSvmRanges` (host) and `SvmTable::covering` (guest)**: both linear range sets. Leave to growable-tables (a growable range set).

#### M3. Completion, waits, published results

##### M3.1 Guest wait on host completion within a deadline
- **Copies**:
  - Vulkan: `wait_for_fences` (icd/vulkan/icd_root.cpp:1682) and `wait_semaphores` (:1702) are byte-identical `CounterWait` loops sending ≤500 ms slices as the host's timeout.
  - CL: `ClBlockingWait::until_complete` (icd/opencl/cl_blocking_wait.cpp:61) is `CounterWait` plus a non-blocking `clGetEventInfo` poll, a 1 ms spin, then driver wake or `sleep_briefly`.
  - **GL has no slicing**: `client_wait_sync` forwards the app's timeout including `GL_TIMEOUT_IGNORED` (icd/opengl/manual_calls.cpp:586 → proto/generated/opengl/gl_dispatch_server.cpp:2338). `glFinish` is an unbounded sync call (gl_icd_entries.cpp:1129). `glGetQueryObject*(GL_QUERY_RESULT)` blocks the host worker (gl_dispatch_server.cpp:9721).
  - **Vulkan `vkGetQueryPoolResults` with `WAIT_BIT`** blocks the host worker unbounded (not overridden in icd/vulkan).
  - The 500 ms/TdrDelay reason (icd/common/sliced_wait.hpp:1-13) applies to GL and Vulkan queries exactly as to fences.
- **Proposal**: `HostCompletionWait` — `icd/common/host_completion_wait.hpp` — *Drives one guest wait on host work to its end within a deadline.* It owns a `CounterWait` and runs `next(slice) → probe.ask(slice) → done/failed/pending → pause.between(slice)`. Two virtual seams, each with ≥2 real implementations:
  - `CompletionProbe`: VkFence, VkSemaphore, VkQueryPool (in icd/vulkan); ClEvents (icd/opencl); GlSync, GlQuery (icd/opengl).
  - `WaitPause`: `HostBlockedPause`, `SpinThenSleepPause(Kmt&)`, `DriverWakePause(Kmt&, ClEventWatcher&)`.

  GL details: after an expired slice, a later `ALREADY_SIGNALED` must become `CONDITION_SATISFIED`; only the first slice carries `GL_SYNC_FLUSH_COMMANDS_BIT`. `glFinish` becomes fence sync → wait → delete, and `glFinish`/`glGetQueryObject*` move to `HANDWRITTEN` in tools/codegen/av_codegen/opengl/emit.py (~:2085; special cases at :2259, :2337).
- **Order**: class → Vulkan fold (identical behaviour) → CL → GL `glClientWaitSync` → GL `glFinish`/queries → Vulkan query pool.
- **Risk**: low for steps 1–3. Steps 4–6 change wire behaviour (`glFinish` 1 → ≥3 round trips): measure Minecraft and Resolve on all three GPU hosts.
- **Tests**: test_counter_wait, test_sliced_wait, test_icd_waits (:118-174 asserts 3×≤500 ms), test_cl_waits, test_cl_event_watcher, test_manual_calls (:302 asserts the GL timeout forwarded as one command: must change), fullstack vk/gl/cl → add test_host_completion_wait (each outcome × pause; zero timeout asks once; forever never stops; counter with no frequency), GL forever-wait slices, `ALREADY_SIGNALED`-after-expiry, query pool never sends `WAIT_BIT`.

##### M3.2 Guest spin budget on the performance counter
- **Copies**:
  - `KmtTransport`: `room_for_fence` :518 (N=1000), `replied_within_spin` :782, `room_within_spin` :802 (N=20), `consumed_within_spin` :835 (N=500), `arrives_eventually` :813.
  - `ClBlockingWait` :70/82.
  - `HostWatch` (own clock fields).
  - `um/d3d/paging_queue.cpp:63` (QPC, `Sleep(0)`, 2 s, untested shim).
- **Proposal**: `CounterSpin` — `guest/transport/counter_spin.hpp` — *Answers whether a spin of a fixed fraction of a second on the guest's counter is still within budget.* `SpinThenSleepPause` (M3.1) is built on it.
- **Order**: after ordered-transport. Risk low.
- **Tests**: test_kmt_transport, test_host_watch, test_loopback_transport → add test_counter_spin.

##### M3.3 Host results published into guest memory (seqlocked slots)
- **Copies**:
  - D3D11 query results: `QueryResultPublisher` writes data → size → `sfence` → release-store count (executor/d3d/query_result_publisher.cpp:120); `QueryResults::read` acquire-loads count → memcpy (um/d3d/query_results.cpp:47). `slot_bytes=128`/`data_at=16` are **declared on both sides**, not in proto.
  - GL error word: `GuestRegion::publish` (executor/guest_region.cpp:63, release-store, **no sfence**) ← `OpenGlBackend::publish_errors` (opengl_backend.cpp:219); read by `KmtBlobArena::host_errors` (guest/transport/kmt_blob_arena.cpp:81) → `ReportedGlErrors`. The word's layout (`first_error=0x0500`, `kinds=7`, `published=0x80<<56`) is declared twice (icd/opengl/reported_gl_errors.hpp, executor/opengl/gl_error_tally.hpp), and the executor hard-codes `sizeof(uint64_t)` (opengl_backend.cpp:229-231).
  - D3D query slots are cut off the end of the whole arena (d3d_session.cpp:99, query_results.cpp:17-26), so slot 511's last 8 bytes *are* the error word. No live collision (no host writes D3D's error word; largest D3D11 payload 88 B < 104 B), but the boundary is implicit.
  - D3D12 fence words: dead (see §2).
- **Proposal**:
  - `PublishedSlotLayout` — `proto/submission/published_slot_layout.hpp` — *Says where each value the host publishes lives in a session's arena tail.* (error word, query slots, slot size, data offset).
  - `GlErrorWord` — `proto/submission/gl_error_word.hpp` — *Encodes the GL error counts the host publishes in one word.*
  - `PublishedSlots` — `executor/published_slots.hpp` (over `GuestRegion`) — *Writes one published value so the guest never sees it half-written.* (payload, `sfence`, sequence last). `GuestRegion::publish` becomes its one-word case and gains the `sfence`.
  - `PublishedSlotReader` — `guest/transport/published_slot_reader.hpp` — *Reads one published value only when its sequence matches before and after the copy.* (fixes B-6).
  - Later and optional, wire changes one API at a time: D3D9 `GetQueryData9` (a round trip per poll today, um/d3d/d3d9_device.cpp:645), GL query availability, CL event status, Vulkan fence status.
- **Order**: delete the D3D12 fence family (tools/codegen/registry/d3d12.toml:389-429, d3d12_fence_publisher.*, handlers, tests) → layout + `GlErrorWord` → `PublishedSlots`/`Reader`, porting `QueryResultPublisher`/`QueryResults` and the GL error word → optional users. docs/protocol.md "Error word" changes in the same commit.
- **Risk**: low (delete), medium (arena-tail layout is protocol).
- **Tests**: test_query_result_publisher, test_guest_region, test_kmt_blob_arena, test_reported_gl_errors, test_gl_error_tally, test_d3d_backend_device, test_opengl_backend_device → add: layout keeps the error word outside the slots; the reader refuses a torn slot; hostile slot id/size.

##### M3.4 Guest GPU timeline over the transport fence
- **Copies**: `StreamTimeline` (icd/d3d/stream_timeline.cpp:17-44: `published_fence()+1`, `max(gpu_completed, waited)`, flush + `await_gpu`) vs GL `GuestGlContext::end_frame` calling the same three `KmtTransport` functions (icd/opengl/guest_gl_context.cpp:222-232).
- **Proposal**: `TransportTimeline` — `icd/common/transport_timeline.hpp` — *Answers how far the host GPU got with a channel's batches.* Built over ordered-transport's single "held commands" type. `GpuTimeline` stays as the test seam. Prerequisite for buffer slices on GL (M4.5).
- **Order**: after ordered-transport. Risk low.
- **Tests**: test_stream_timeline, test_frames_in_flight, test_buffer_slices, test_fullstack_opengl_icd.

##### M3.5 Locked reply window
- **Copies**: `D3dDeviceChannel`'s `ClientTransport` half (icd/d3d/d3d_device_channel.cpp:117-170) copies `icd::LockedTransport` (same `Phase{idle,writing,answering}`, `let_go`, `ScopeExit`, `end_call`); the only addition is `stream_.speak_as(device_)`.
- **Proposal**: `D3dDeviceChannel` holds a `LockedTransport` over a small `D3dDeviceSelection final : ClientTransport` in icd/d3d that calls `speak_as` then forwards.
- **Order**: after ordered-transport (it owns `LockedTransport`/`D3dCommandStream`; better still, hand it to that agent).
- **Tests**: test_d3d_device_channel, test_locked_transport, D3D11 multi-device multi-thread probes.

##### M3.6 Host bounded polls and GPU-completion wake (executor)
- **Polls**: `PollBudget` (executor/poll_budget.cpp) counts polls and is used by shared_surfaces.cpp:88, d3d11_host_device.cpp:162/292. Two loops are hand-rolled on `CLOCK_MONOTONIC`:
  - D3D12 `drain` (d3d12_session.cpp:283-323: 2 ms spin, 50 µs naps, 5 s "hung").
  - D3D11 `await_gpu_unlocked` (d3d_backend.cpp:1803: 20 µs × 100000).

  More hand-rolled deadlines exist in gpu_clock_hold.cpp:113,205, opencl_backend.cpp:596,615, device_tenants.cpp:86, vulkan_submit_timing.cpp:100.
  - Proposal: grow `PollBudget` into time form `PollBudget(spin_ns, pause_ns, deadline_ns)` with a clock seam; move every loop onto it. Keep the D3D12 hang verdict text and timing.
- **Wake**: `D3d12FenceWake` (executor/d3d/d3d12_fence_wake.cpp) is an eventfd pair + pthread + `poll` + `WakeSignal`; only `SetEventOnCompletion` (:47) is D3D12-specific. **D3D11 has no wake**: `D3dBackend::poll` returns `progress11_.pending()` (d3d_backend.cpp:198), so the worker spins up to 100 ms (device/threading/worker.cpp:206).
  - Proposal: `EventfdWake` — `executor/eventfd_wake.hpp` — *Raises a session's `WakeSignal` each time its eventfd is written.* D3D12 keeps only `watch(fence, value)`; the second user is `D3d11BatchProgress` via `ID3D11Fence::SetEventOnCompletion`.
  - **Blocker**: native DXVK's `SetEvent` is a stub returning FALSE (third_party/dxvk/src/util/util_win32_compat.h:43; d3d11_fence.cpp:118-127) → new `tools/dxvk-patches/0009-native-set-event-eventfd.patch`.
- **Must differ**: CL's generation-guarded callbacks (CL cannot unregister). Vulkan/GL `gpu_progress` 0/0 (their guests wait explicitly).
- **Risk**: low (extract), medium (DXVK patch; D3D11 latency/CPU, likely a win).
- **Tests**: test_poll_budget, test_d3d12_fence_wake, test_d3d11_batch_progress, test_d3d12_session_device, test_d3d_backend_device, test_shared_surfaces → add test_eventfd_wake, a deadline test with a clock seam; DWM idle CPU on all hosts.

#### M4. Transfers, staging, maps, geometry

##### M4.1 Arena carving
- **Copies**:
  - CL re-lends its arena into `blocks_` + `staging_` (icd/opencl/guest_cl_context.cpp:106-108, `staging_reserve` .hpp:84).
  - D3D lends up to `lent_bytes=24MiB`, takes the query tail (um/d3d/query_results.cpp:20-27), puts `BufferSlices` books at `view+lent_bytes` (d3d_session.cpp:133), checked against the query offset (:109).
  - GL takes the whole arena.
- **Proposal**: `ArenaSpans` — `icd/common/arena_spans.hpp` — *Carves a channel's arena into the fixed spans its users own.* Leases first, then named fixed tails; refuses overlap; places the tail by `PublishedSlotLayout` (M3.3), so the error word is never lent. Then CL's `ClStagingBlock` takes ordinary leases and `staging_` disappears.
- **Order**: after ordered-transport and M3.3. Risk medium (an off-by-one corrupts silently).
- **Tests**: test_cl_arena_blocks, test_cl_staging_block, test_cl_transfer_chunks, test_buffer_slices, test_kmt_blob_arena, test_client_blob_arena, test_fullstack_opencl_icd → add `never_lends_error_word`, `refuses_overlap`.

##### M4.2 Transfer piece sizing
- **Copies**:
  - GL: `BufferPieces` `staging/reply - 4096` (icd/opengl/buffer_pieces.hpp:31-32); `BufferMapping` hard-codes 48K/192K/4M (buffer_mapping.hpp:40-44) and forces lease retirement with a dummy `glIsBuffer` (buffer_mapping.cpp:221-231); `BackBufferPresenter` 8M/48K (back_buffer_presenter.hpp:58-59).
  - CL: the same `-4096` (cl_transfer_chunks.hpp:56-57), `ClStagingBlock` 4M/64K (cl_staging_block.hpp:29-30).
  - D3D: `arena_piece` 16M, `make_room_for` (d3d_command_stream.hpp:54,61,110).
- **Proposal**: `TransferRoom` — `icd/common/transfer_room.hpp` — *Gives the bytes one piece of a bulk transfer carries on a channel.* Built from the `ChannelShape`; `inline_write()`, `inline_read()`, `lent_piece(left, most)` retiring leases through the transport, not an API command.
- **Order**: after ordered-transport, with M4.1 (same agent). Risk medium.
- **Tests**: test_buffer_pieces, test_buffer_mapping, test_back_buffer_presenter, test_cl_transfer_chunks, test_fullstack_transport/opengl_icd/opencl_icd.

##### M4.3 Pitched region extent and the row copier (guest and host)
- **Copies** (about ten):
  - Guest: `TextureUpload::bytes/readable` (icd/d3d/texture_upload.cpp:34-50), `PitchedRows` (icd/d3d), `ClImageRegion::valid/row_offset/offset_of` (icd/opencl/cl_image_region.cpp:15-92), `ClBufferRect`, `ClStagedRows::pack/unpack`, `PixelStore::span` (icd/opengl/pixel_store.cpp:54-86).
  - Host: `D3d9TextureWrite::fits` (executor/d3d/d3d9_texture_write.cpp:21-32), D3D11 UpdateSubresource inline formula (d3d_backend.cpp:1672-1677), `D3d12Footprint::holds` (d3d12_footprint.cpp:28-40), `ClBouncedRect::read` (executor/opencl/cl_bounced_rect.cpp:38-62).
  - Real divergence: B-5.
- **Must differ**: each API's refusal rules (GL alignment/skips, CL zero pitch = tight, D3D block elements). The extent math and the row copy do not.
- **Proposal** (proto/resources, beside `TexelLayout`):
  - `PitchedExtent` — *Gives the bytes a box of a texel layout spans at given row and slice pitches.* `bytes()`, `pitches_hold()`, `fits(w,h,d)`, all overflow-checked.
  - Move `PitchedRows` there as the one row copier.
  - Per-API classes keep only their own refusals.
- **Order**: class + hostile table + fuzz → host copies → guest copies. Risk low-medium (reads guest sizes).
- **Tests**: test_texture_upload, test_pitched_rows, test_cl_image_region, test_cl_buffer_rect, test_cl_staged_rows, test_pixel_store, test_d3d9_texture_write, test_d3d12_footprint, test_d3d12_copy_extent, test_cl_bounced_rect, test_texel_layout → add tests/unit/proto/resources/test_pitched_extent.cpp, tests/fuzz/fuzz_pitched_extent.cpp, a D3D9 multi-slice case at the B-5 boundary.

##### M4.4 Staged CPU maps (private copy written back at unmap)
- **Copies**:
  - CL `take_mapping/take_image_mapping` (icd/opencl/guest_cl_context.cpp:208-243, `MappedRegion`; comment :219-222 says "OpenGL's staged maps do the same").
  - GL `BufferMapping` (diff against `BufferShadows` or the explicit flush span).
  - D3D9 `make_shadow` + `unlock_subresource` (um/d3d/d3d9_device_resources.cpp:266-282, 349-368).
  - D3D11 `ResourceRecord.shadow` via `WatchedShadows` (Windows write-watch) + `WriteMaps` (icd/d3d/write_maps.hpp) + `upload_written` (d3d11_device_map.cpp:108-121).
  - Four open-map tables: `WriteMaps` 256, `MappedBuffers` 64, CL `mappings_` 4096 (linear scan ×4), D3D9 per-subresource `locked`.
- **Must differ**: map-flag policy (docs/mapped-buffers-plan.md §2: DISCARD/NOOVERWRITE/INVALIDATE, persistent maps, explicit flush). How writes are found also differs: write-watch is a Windows shim, the diff is portable.
- **Proposal** (icd/common):
  - `StagedMapping` — *One live map's private copy and the range it stands for.*
  - `OpenMappings` — *The live maps of one context, found by object and subresource or by pointer.*
  - `WrittenRuns` — *The merged runs of a staged copy to send back.* Gap merge (GL 256), run cap (D3D11 32), whole-copy fallback.

  Run producers stay per API (GL diff/flush marks; D3D11 write-watch). D3D11 texture shadows stay (doc §4).
- **Order**: after RE4 (CL `Memory::application`, GL pinned) + ordered-transport + growable-tables. Risk **high** (Unity late writes, GL persistent-map flush points).
- **Tests**: test_write_maps, test_mapped_buffers, test_buffer_mapping, test_buffer_shadows, test_mapped_region, test_cl_svm_paths, probes named in the plan (Subnautica, HL2, DWM) → add test_written_runs (table-driven).

##### M4.5 Buffer slices beyond D3D11 (mapped-buffers-plan steps 6–7)
- Only D3D11 uses `BufferSlices`/`ContextSlices`/`GpuTimeline`/`StreamTimeline`; D3D9 and GL still copy.
- `BufferSlices` and `GpuTimeline` move to icd/common when GL adopts them. That needs M3.4 and a real GL `gpu_progress` (today 0/0, opengl_backend.cpp:197-202). D3D9 needs `BindBufferSlice` in d3d9_session over DXVK interop.
- Order: last memory item (after M4.4, M3.4). Risk high.
- Tests: test_buffer_slices, test_context_slices, test_buffer_slice_device + new D3D9/GL device tests.

##### M4.6 Red/blue swap of 32-bit rows
- **Copies**: `Swapchain::to_bgra` (icd/vulkan/swapchain.cpp:229-239, word at a time); `FrontBuffer::read` (icd/opengl/front_buffer.cpp:58-73, byte loop; the swapchain comment records the byte loop cost a fifth of a game's render thread); `D3d11HostDevice` readback (executor/shim/d3d11_host_device.cpp:309-320, byte loop).
- **Proposal**: `BgraRows` — `core/support/bgra_rows.hpp` — *Copies rows of four-byte pixels, swapping red and blue when asked.* (three binaries).
- **Tests**: test_front_buffer, test_swapchain → add test_bgra_rows (pitch ≠ width×4). Risk low.

#### M5. Formats

##### M5.1 One canonical DXGI format row
- **Copies**, nine tables mapping DXGI to something:

  | Table | Location |
  |---|---|
  | layout | proto/d3d/dxgi_format_layout.cpp:13-31 |
  | typeless→UNORM | proto/d3d/dxgi_resolve_format.cpp:15-26 |
  | family/typeless/depth | icd/d3d/dxgi_format_table.cpp:60-68,186-215 |
  | DXGI↔D3DDDIFORMAT | icd/d3d/present_format.cpp:37-71 |
  | DXGI→VkFormat + sRGB sibling + bytes | proto/vulkan/exportable_image_info.cpp:41-64 |
  | DXGI→VkFormat (X8 differs) | executor/shim/host_image_export.cpp:19-28 |
  | DXGI→DRM fourcc | executor/shim/drm_fourcc.cpp:18-31 |
  | DXGI→CL (guest, 41 rows) | icd/opencl/dxgi_image_format.cpp:17-58 |
  | DXGI→CL (host, 11 rows) | executor/opencl/dxgi_cl_format.cpp:18-30 |

  Plus inline VkFormat→DXGI (icd/vulkan/chained_image.cpp:28-39), the red/blue-swap decision (d3d11_host_device.cpp:309-310), and `format_count=192` declared four times (icd/d3d/dxgi_format_table.hpp:32, um/d3d/d3d11_host_caps.hpp:27, um/d3d/d3d12_host_caps.hpp:36, executor/shim/d3d11_capabilities.hpp:20).
- **Differ**: texel bytes stated twice; sRGB sibling coded four times; typeless family twice; X8 disagreement; the CL guest/host difference is real but invisible (B-4).
- **Proposal**: `DxgiFormat` — `proto/d3d/dxgi_format.hpp` — *Describes one DXGI_FORMAT value by its row in the one canonical DXGI table.* Columns: element bytes and block w/h, typeless family head, linear/sRGB sibling, depth, D3DDDIFORMAT, VkFormat (raw u32), CL order/type (raw u32), "host views as CL image", DRM fourcc, `count=192`.

  `DxgiFormatLayout`, `DxgiResolveFormat`, `DxgiFormatTable` family logic, `DrmFourcc` (deleted), `ExportableImageInfo` pairs, `DxgiImageFormat`, `DxgiClFormat` and `HostImageExport::vk_format` all read it.
- **Order**: class with comparison tests against every old table *before* deleting them → layout → resolve/family → `PresentFormat` (M5.3) → DRM → ExportableImageInfo (+X8 row) → CL pair → `format_count`. Risk medium (row values must not change; family logic gates MSAA checks).
- **Tests**: test_dxgi_format_layout, test_dxgi_resolve_format, test_dxgi_format_table, test_present_format, test_exportable_image_info, test_dxgi_cl_format, test_dxgi_image_format, test_d3d_{cl,gl,vulkan}_interop_device, test_d3d_external_export_device → add `every_row_matches_dxgi_format_layout_bytes`, `srgb_sibling_is_symmetric`, `a_format_the_cl_icd_maps_and_the_host_exports_is_viewed_as_an_image_or_marked_not`, `d3d_cl_interop_device.an_srgb_texture_opened_by_opencl_is_an_image_or_stage_one`.

##### M5.2 FourCC packing (four copies)
- executor/shim/drm_fourcc.cpp:10, proto/d3d/d3d9_format_layout.cpp:23, icd/d3d/d3d9_format_ops.cpp:9, device/display/scanout_format.cpp:9 (two do not cast through `unsigned char`).
- **Proposal**: `FourCc` — `core/support/four_cc.hpp` (constexpr) — *Packs four characters into the little-endian code DRM and D3D9 name formats by.*
- **Tests**: test_d3d9_format_layout, test_d3d9_format_ops, test_scanout_format → add test_four_cc.

##### M5.3 Presentable surface format, guest and host
- **Copies**:
  - `PresentFormat::take_ddi` (icd/d3d/present_format.cpp:37-49, 6 formats).
  - Host `D3d9FlipShares` (executor/d3d/d3d9_flip_shares.cpp:27, 2 formats; :75).
  - `D3d11Device`'s private `dxgi_format_of` (um/d3d/d3d11_device_present.cpp:472-480): **X8R8G8B8 → B8G8R8A8 here but B8G8R8X8 in `PresentFormat`**.
  - The "frame the engine can take" predicate written 5× (d3d_backend.cpp:1843, d3d9_session_resources.cpp:437, d3d9_flip_shares.cpp:75, d3d12_frame_pool.hpp, um/d3d/d3d9_device_present.cpp:140).
- **Proposal**: `PresentFormat` moves to proto/d3d as `D3d9PresentFormat` — *Names a presentable surface's format both ways, as DXGI and as D3DDDIFORMAT.* Used by the guest, `D3d9FlipShares` and D3D11's present. Decide X8 deliberately, with a test. The engine-takes predicate becomes `StagedFrame` pools' `takes(format)` (M6.5).
- **Tests**: test_present_format (moved to tests/unit/proto/d3d/), d3d9 device tests.

##### M5.4 Window-slot formats (Vulkan/GL ↔ DXGI window chains)
- **Copies**: `offered_formats` (icd/vulkan/surface_properties.cpp:10-11), `slot_formats` + `slot_binds=0x8|0x20` (icd/vulkan/chained_image.cpp:20-38), `shows_bgra()` (icd/vulkan/swapchain.cpp:241-246), `DxgiWindowChains` SlotLayout→DXGI + BindFlags (um/common/dxgi_window_chains.cpp:86-87, 348; must match chained_image.cpp:22-24 byte for byte), GL `srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8` twice (chained_frames.cpp:57, back_buffer_presenter.cpp:238).
- **Proposal**: `SlotFormat` — `icd/common/slot_format.hpp` — *Names one window slot's channel order with the DXGI format it is shared as.* Built from `SlotLayout`; the DXGI value comes from `DxgiFormat` (M5.1). Bind flags are a `SlotFormat` member.
- **Tests**: test_chained_image, test_surface_properties → add test_slot_format asserting `DxgiWindowChains` makes exactly what the guest describes (nothing pins it today).

##### M5.5 D3D12 format caps: 191+ round trips at device open
- **Copies**: D3D11 bulk `QueryCaps11` (um/d3d/d3d11_host_caps.cpp:28-36) and D3D9 bulk `QueryFormats9`, but D3D12 makes one sync `QueryFormat12` per format plus per MSAA candidate (um/d3d/d3d12_host_caps.cpp:55-77; host d3d12_session.cpp:355).
- **Proposal**: `DxgiFormatTable::learn(const d3dproto::FormatCaps*, count)` replaces both record loops; D3D12 gets a bulk `QueryCaps12` (codegen toml).
- Risk medium (wire + caps gate the runtime). Tests: test_dxgi_format_table, D3D12 device-open tests.

#### M6. Presentation and swapchains

##### M6.1 Window chain slots opened as shared memory (GL ↔ Vulkan)
- **Copies**: `ChainedImage` (icd/vulkan/chained_image.cpp:58-131, 1 slot) and `ChainedFrames` (icd/opengl/chained_frames.cpp:20-125, 2 slots) both do `WindowChains::open` → per slot `SharedObjectCall::open(id, memory, bytes)` → release per slot → `chains.close`.
- **Must differ**: the import (VkImage vs GL texture), the slot count, the row order.
- **Proposal**: `ChainSlots` — `icd/common/chain_slots.hpp` — *Holds one window's chain slots opened as shared memory through one connection.* `open(wire, window, luid, w, h, SlotLayout, count)`, `release`, `close`, `fits`. GL textures must be deleted before the memory is released (chained_frames.cpp:116-124).
- **Tests**: test_chained_frames, test_chained_image, test_d3d_{vulkan,gl}_interop_device → add test_chain_slots (partial open releases only what it opened; refused open holds nothing; second window closes the first chain).

##### M6.2 Present pacing (swap interval / present mode over FramePacer)
- **Copies**: Vulkan (icd/vulkan/swapchain.cpp:173 IMMEDIATE drop, :188/:210 FIFO wait); GL `pace()`/`admits()` (icd/opengl/guest_gl_context.cpp:350-365) + `SwapInterval` (max 4).
- **Proposal**: `PresentPacing` — `icd/common/present_pacing.hpp` — *Paces one presenter's frames to the interval it asked for.* Owns the `FramePacer` + interval; `ask(blanks)`, `admits()`, `after_present()`. `SwapInterval` is deleted; Vulkan maps FIFO→1, IMMEDIATE→0 once.
- **Tests**: test_frame_pacer, test_swap_interval, test_swapchain, `gl_icd_root_set_swap_interval.a_swap_waits_for_the_blanks_its_interval_asks_for` → add interval-2 and drop-before-readback cases.

##### M6.3 Window state before a present (gone / minimized / resized)
- **Copies**: `Swapchain::window_state` (swapchain.cpp:215-224; also surface_properties.cpp:29), GL `WindowVisibility::showing` (window_visibility.cpp:14-21), `BackBufferPresenter::prepare` (:43-73), `WindowFramebuffers::drop_gone` (:167-174), `WindowTable::drop_gone` (:47).
- **Proposal**: `WindowExtent` — `icd/common/window_extent.hpp` — *Judges one window's client area against the extent a presenter holds.* `look()` → {gone, empty, resized, same}. `WindowVisibility` is deleted.
- **Tests**: test_window_visibility, test_swapchain, test_back_buffer_presenter, test_window_framebuffers → add test_window_extent.

##### M6.4 Present queue / async present and frames in flight
- **Copies**:
  - GL: `GlPresentLoop` + `HandedFrame` + `icd::WakeSignal` (icd/opengl/wake_signal.hpp; Windows half um/common/windows_wake_signal), waiting each frame's `BatchMark` (gl_present_loop.cpp:231-260); three routes in `BackBufferPresenter::present` (:105-130).
  - Vulkan: synchronous in `vkQueuePresentKHR` (swapchain.cpp:166-192; `PresentDevice::copy` 5 s fence; CPU swizzle + top-down blit).
  - `FramesInFlight` (icd/opengl, 50 ms) is GL-only because Vulkan is synchronous.
- **Must differ**: the readiness primitive (BatchMark vs VkFence through the wire). The queue, slots, settle, show = chain-present-or-blit do not. Vulkan's synchronous present is a simplification, not a requirement.
- **Proposal**: `WindowPresentQueue` — `icd/common/window_present_queue.hpp` — *Shows handed frames on their windows in the order they were handed.* Two seams with two implementations each: `PresentSource` (GL presenter, Vulkan swapchain) and `FrameReady` (BatchMark, fence wait). `HandedFrame` gains a row order. The thread event is renamed `ThreadWake` (it collides in name with executor `av::WakeSignal`) and moves to icd/common with its Windows half staying in um/common — both now have two users. `FramesInFlight` moves to icd/common once Vulkan is async.
- **Order**: move/rename with GL only → Vulkan hands frames, still synchronous → Vulkan async + `FramesInFlight`.
- **Risk**: medium-high (`held_` image reuse after acquire, throttle). Gate on probes on all three hosts.
- **Tests**: test_gl_present_loop, test_back_buffer_presenter, test_swapchain, test_present_device, test_presentation, test_frames_in_flight; probes present_work, wpf_present_work, loader_vulkan.

##### M6.5 Host D3D present points, staged frames and the producer→DXVK order
- **Present point handling**, one mechanism written three times:
  - D3D11 `on_PresentPoint` (executor/d3d/d3d_backend.cpp:1822-1850).
  - D3D9 `on_PresentPoint9` (d3d9_session_resources.cpp:424-452).
  - D3D12 `on_PresentResource12` (d3d12_session.cpp:774-785).

  Each does "point 0 = flip, else presented → stage → hold". Proposal: `SharedSurfaces::stage(point, Staging, FrameStager&)`, with `FrameStager` virtual ×3 and the format predicate as the pool's `takes(format)`.
- **StagedFrame implementations**: three (executor/shim/d3d9_frame_pool.cpp:139-169, d3d11_frame_pool.cpp:123-157, d3d12_frame_pool.cpp:265-) repeat claim/`out()`, `mapped_`, five view assignments, release = unmap + give back. Loss is handled three ways. Proposal: `StagedFrame` (executor/d3d/staged_frame.hpp) owns `FrameClaim`, the view fill and an atomic `forsaken`; it leaves `map_when_copied`/`unmap` virtual.
- **Producer→DXVK ordering timeline**, twice:
  - `D3d9FlipShares::Lane` (executor/d3d/d3d9_flip_shares.cpp:86-198) over `VulkanTimeline`.
  - `D3d12Presentation` (executor/shim/d3d12_presentation.cpp:65-109) over a shared `ID3D12Fence`.

  D3D9 takes `device11_->lock` around `vulkan().wait` (:97-99), D3D12 does not; settle which is right.
  - Proposal: `ComposeOrder` — `executor/shim/compose_order.hpp` (translation over DXVK) — *Orders the GPU's Direct3D 11 queue after one producer's presents.*
- **Related connect logic**: `DxvkVulkanDevice` and `D3d9VulkanLane` repeat interop → table → `BorrowedQueue` → `QueueWork` (executor/shim/dxvk_vulkan_device.hpp, d3d9_vulkan_lane.hpp). Fold into `ComposeOrder`'s owner if it falls out naturally.
- **Must differ**: each flip route (`D3d11HostDevice::show`, `D3d9FlipShares::flip`, `D3d12Presentation::share`) because the producers differ.
- **Risk**: low (stage), medium (`ComposeOrder`: lock order can deadlock DWM).
- **Tests**: test_shared_surfaces, test_frame_pool, test_frame_claim, test_d3d_backend_device:487,549, test_d3d9_session_device:423-568, test_d3d12_session_device:972,1013, test_d3d_vulkan_interop_device → add test_staged_frame, "an unsuitable format never leaves a blt waiting" ×3 APIs.

##### M6.6 D3D guest present scaffolding (D3D9/11/12)
- **Copies**:
  - Present-point counter skipping 0, ×3 (um/d3d/d3d11_device_present.cpp:172, d3d9_device_present.cpp:144, d3d12_device_present.cpp:94).
  - `KernelSurface` allocation + resident + key + refusal log, ×2.5 (d3d11 :25-66; d3d9 :52-108; D3D12 differs via `pfnAllocateCb` :28-71).
  - Follow wrapper ×3 with different logging (d3d11 :232-243 logs once, d3d9 :35-39 never, d3d12 d3d12_device.cpp:345 logs).
  - Kernel context ×2.
  - Readback into allocation pages ×2.
  - Two near-identical multiplane presents inside D3D11 (:303-382).
- **Must differ**: how the point travels (D3D9 escape `NextPointRecord`; D3D11/12 private data).
- **Proposal**:
  - `PresentCount` — `icd/d3d/present_count.hpp` — *Numbers one surface's presents, never 0.* Portable, so it gains coverage.
  - `PresentedAllocations` — `um/d3d/presented_allocations.hpp` (WDK types) — *Makes the kernel allocations a device presents from.* Covers `allocate`, `make_context` and `follow` with the once-only log.
  - The multiplane pair becomes one template over the arg type (two instantiations).
- **Tests**: test_kernel_surface, test_present_format, test_swap_chain_buffer → add test_present_count (wrap 2^32-1 → 1). um/d3d is untested: prove with the D3D device tests and the guest suite.

#### M7. Sharing and interop

##### M7.1 Shared allocation private data (descriptor + shared-object record) hand-assembled at seven sites
- **Sites**:
  - miniport/memory/allocation_table.cpp:44-72.
  - miniport/scheduling/device_table.cpp:87-92,120-121.
  - icd/d3d/kernel_surface.cpp:60-69,107-114.
  - icd/vulkan/icd_root.cpp:843-870 (`publish`).
  - icd/common/shared_handles.cpp:84-96 (`adopt`).
  - um/common/dxgi_window_chains.cpp:44,366-393 (`identify`, which redoes `adopt`).
- **Proposal**: `SharedAllocationData` — `proto/resources/shared_allocation_data.hpp` — *Owns the bytes of a shareable allocation's private driver data: a descriptor followed by its shared-object record.* `encode`, `decode`, `object_id`, `size`, `assigned`, `bytes`, `encoded_size`.
- **Order**: class + hostile tests + fuzz → miniport → KernelSurface → SharedHandles → Vulkan publish → window chains.
- **Risk**: medium (the miniport parses hostile UM bytes: 100% branch coverage + fuzz target).
- **Tests**: test_allocation_descriptor, test_shared_object_record, miniport allocation_table/device_table tests, test_shared_handles, test_kernel_surface, test_external_objects, test_fullstack_shared_objects; probes shared_object_probe, tools/guest/probes/shared_id_probe.cpp → add test_shared_allocation_data + fuzz target.

##### M7.2 Vulkan re-implements `OpenedObjects`' import
- **Copies**: `OpenedObjects::open` (adopt → page round → refuse duplicate → `SharedObjectCall::open` → row; icd/common/opened_objects.hpp:60-75) serves GL and CL. Vulkan repeats it in `IcdRoot::object_of` (icd/vulkan/icd_root.cpp:903-919), `shared_allocation`/`imported_allocation` (:1175-1260), rows in `ExternalObjects`, release at :921-936.
- **Must differ**: re-importing its own publication through a fresh fd (`own_process_allocation` :812; semaphores :1036-1077) — and that path goes away in M7.6.
- **Proposal**: Vulkan cross-process imports go through `OpenedObjects`; `ExternalObjects` keeps only exported rows plus a Vk-object→made lookup.
- **Order**: after M7.1. Risk medium.
- **Tests**: test_external_objects, test_external_handles, test_opened_objects, test_fullstack_shared_objects, test_d3d_vulkan_interop_device.

##### M7.3 D3D11 "rows" texture shared by NT handle, twice in um/
- **Copies**: um/common/dxgi_window_chains.cpp:340-393 and um/opencl/windows_d3d11_bridge.cpp:158-190,~250 (create under `SharedRowsScope` → `CreateSharedHandle`). Shared-surface size is computed three ways (icd/d3d/kernel_surface.cpp:36-52, icd/opencl/cl_icd_root_d3d11.cpp:148-150, dxgi_window_chains.cpp:391).
- **Proposal**: `SharedRowsTexture` — `um/common/shared_rows_texture.hpp` — *Makes a D3D11 texture whose host memory is laid out as rows, with the NT handle that names it.* Its identity and size come from `SharedAllocationData` (published size, never recomputed).
- **Tests**: test_cl_d3d11_sharing, test_fullstack_opencl_icd (`a_d3d11_resource_s_bytes_reach_the_kernel_and_come_back_changed`), test_d3d_cl_interop_device, GL/Vk window probes.

##### M7.4 `HostShares` carries two protocols, and D3D cannot import
- **Copies**: `HostShares` has a descriptor family (host_shares.hpp:76-88) and a COM family (:95-155: `publish_resource`, `open_resource`, `view`, `keep_view`, `shown`, `lose_direct3d11/12`, `restore_resource`, `take_retired`, …). `D3dBackend::import_object` returns false (d3d_backend.cpp:322-334), so D3D11/12 cannot open a Vulkan/GL/CL publication, although `ExportableImages::adopt(fd, desc)` exists.
- **Proposal**: `D3dBackend::import_object` goes through `ExportableImages::adopt`. The COM row state moves into `D3dShareState` — `executor/sharing/d3d_share_state.hpp` — *Holds the Direct3D objects one share keeps on the GPU's Direct3D devices.* `HostShares` no longer names `IUnknown`.
- **Risk**: high (DWM, D3D11 lock order, `CarriedShare` device-loss carrying, D3D12 presentation fence).
- **Tests**: test_host_shares, test_session_shares, test_share*, test_shared_surfaces, test_d3d_device_loss_device, all test_d3d_*_interop_device, a guest DWM run.

##### M7.5 Copy mirrors: align behaviour, do not merge
- CL–D3D11 stage 1 (guest `ClD3d11Objects`, skips copy-in for WRITE_ONLY) vs CL–GL copied (host `GlMirror`, executor/opencl/copied_gl_sharing.cpp:49,114, **copies in even for WRITE_ONLY**). Different binaries and reach (COM in guest vs share group on host), about 10 lines each: keep both.
- Give `GlMirror` a `reads_source` flag. Test: `copied_gl_sharing_device.a_write_only_object_is_not_read_from_gl_at_acquire`.

##### M7.6 Two cross-device sharing paths for Vulkan
- **Copies**: feature-047 fds (guest `own_process_allocation` icd_root.cpp:812, `host_semaphore_fd` :1026; host `HostExportedFds`) vs feature-053 `HostShares` open. A semaphore published by *another* process cannot be imported at all (icd_root.cpp:1062-1066).
- **Proposal**: open names the importing device; `HostShares` lets a publisher's session import into a different device of its own; a semaphore open imports a payload into an existing semaphore. Delete `HostExportedFds`, `own_process_allocation`, `host_semaphore_fd`.
- **Risk**: medium-high (open record wire change).
- **Tests**: test_fullstack_shared_objects, test_vulkan_backend_device, test_external_objects → add `a_semaphore_another_process_published_is_imported`; delete test_host_exported_fds.

#### M8. Adapters, capabilities, loss, errors

##### M8.1 Device loss: detection, latch, spelling
- **Copies**:

  | API | Detection | Latch/log |
  |---|---|---|
  | D3D9/12 | failed send → `lost` (um/d3d/d3d9_device.cpp:233-237, d3d12_device.cpp:123-130) | `DeviceRemoval` |
  | D3D11 | about 15 inline `set_error(D3DDDIERR_DEVICEREMOVED)` (d3d11_device.cpp:348,353,459,528,634; d3d11_device_present.cpp:106-294; d3d11_device_map.cpp:76) | **no latch**; logs every call |
  | GL/CL | `ContextLink::lost(channel)` (icd/common/context_link.cpp:53-59) in each `wire()` | own |
  | Vulkan | `Connection::faulted` → `IcdRoot::lost` (icd_root.cpp:331-338), about 20 entry points | own |

  **Two triggers**: D3D checks `faulted() || host_faulted()` (um/d3d/d3d_session.cpp:64-70); GL/CL/Vulkan only `HostChannel::faulted()` (host_channel.cpp:163-167), so an idle context learns of a fault at its next send. The fault code is unused (B-8).
- **Proposal**:
  - `HostChannel::lost()` + `fault_code()` (after ordered-transport, which owns host_channel).
  - `LossLatch` — `icd/common/loss_latch.hpp` — *Keeps the first fault cause a context was lost to.* It logs once per entry point through the root's `KeyedLog`; the cause is the transport fault code. Each API keeps a one-line spelling: D3D code 7 → DEVICE_REMOVED else DEVICE_RESET; GL code 7 → UNKNOWN else GUILTY_CONTEXT_RESET; Vulkan `VK_ERROR_DEVICE_LOST`; CL `CL_OUT_OF_RESOURCES`. `DeviceRemoval` is replaced.
- **Order**: latch + accessor → D3D9/12 → D3D11 (one `D3d11Device::lost(entry)`) → GL, CL → Vulkan.
- **Risk**: medium (D3D11 DWM recovery).
- **Tests**: test_gl_device_loss, test_cl_device_loss, icd/vulkan test_device_loss, test_umd_adapter, test_umd_invariants, test_fullstack_device_loss, avtest `loss` with tests/gpu_fault → add per Khronos API: after a fence-page fault the next call answers lost without a send.

##### M8.2 Adapter facts and memory budget read four ways
- **Copies**:
  - D3D `AdapterIdentity::learn` (icd/d3d/adapter_identity.cpp:25-59): its fields are read by nothing but `report()`, and its fixed name disagrees with the Windows name the other APIs show.
  - CL `describe_adapter` (icd/opencl/cl_icd_root.cpp:181-195).
  - Vendor `VendorAdapters::describe` (icd/vendor/vendor_adapters.cpp:27-51).
  - Vulkan `Connection::open` name read (icd/vulkan/connection.cpp:33-38).
  - Memory budget read in Vulkan `MemoryTypeTable::build` (:52-56), CL `free_memory` (cl_icd_root.cpp:384), vendor `query_segment_sizes`.
  - The LUID-list loop twice: icd/vulkan/icd_root.cpp:471-479, icd/opencl/cl_devices.cpp:20-36.
  - PCI vendor constants twice each: AMD `0x1002` (cl_device_report.hpp:72, adl_root.hpp:52), NVIDIA `0x10DE` (cl_device_report.hpp:73, nvapi_root.hpp:53), virtio `0x1AF4` (avgpu_adapters.hpp:31, cl_device_report.hpp:71).
- **Proposal**:
  - `AdapterFacts` — `icd/common/adapter_facts.hpp` — *Holds the facts Windows gives for one avgpu adapter, each with whether it is known.* (missing-fact policy stays per API).
  - `AdapterMemory` — `icd/common/adapter_memory.hpp` — *Reads one adapter's local and non-local budget and usage on demand.*
  - `AvgpuAdapters::luids(...)`.
  - PCI constants in `guest/kmt/pci_identity.hpp`.
  - Delete D3D `AdapterIdentity`.
  - Drop the unread host identity wire fields (`QueryCaps9` vendor/device/… read and dropped um/d3d/d3d9_host_caps.cpp:51-53; `Caps11.vendor_id/device_id/dedicated_video_memory_mb` d3d_wire_types.h:256-258 filled executor/shim/d3d11_capabilities.cpp:26; D3D12's d3d12_host_device.cpp:57,266). The capset `GpuIdentity` is the one source.
  - One adapter cap: `AvgpuAdapters::max_adapters` everywhere, or growable.
- **Host side**: the VkPhysicalDevice UUID read is written three times (executor/shim/physical_device_identity.cpp:13-22, physical_device_list.cpp:22-28, loader_host_apis.cpp:68-76) → use `PhysicalDeviceIdentity`.
- **Order**: AdapterFacts (vendor → CL → Vulkan; delete D3D's) now; AdapterMemory after RE4 (memory_type_table.cpp); caps after growable-tables.
- **Tests**: test_vendor_adapters, test_cl_device_report, test_cl_devices, test_physical_device_view, test_memory_type_table, test_host_apis, test_host_gpu_set → add test_adapter_facts (each refused fact).

##### M8.3 "Lost link left to its holders, newcomers get a fresh one" twice
- **Copies**: `IcdConnection` (icd/common/icd_connection.cpp:112-147, GL+CL) and `AdapterSessions<Caps>::session` (um/d3d/adapter_sessions.hpp:62-66,145-166, **untested** um).
- **Must differ**: Vulkan (one context per instance; host handles are scoped to it).
- **Proposal**: `LinkGenerations<T>` — `icd/common/link_generations.hpp` — *Holds the T new users join, and the lost Ts kept for their remaining users.* Two instantiations (`GuestConnection`, `D3dSession`); moves D3D's rotation into tested code. Move the misplaced test to tests/unit/icd/common/test_icd_connection.cpp.
- **Order**: after ordered-transport; same agent as M8.1 (shared files guest_gl_context.cpp, guest_cl_context.cpp, context_link.cpp).
- **Risk**: high for D3D (DWM makes the new device before releasing the removed one).
- **Tests**: IcdConnection tests, test_context_link, test_umd_connections, avtest loss `recreated`, test_fullstack_device_loss.

##### M8.4 Extension reporting and version capping
- **Policies**: three policies share only the row type. `ExtensionPolicy` (icd/common, Vulkan-only), `ClExtensionPolicy`, `GlExtensionPolicy` each re-implement row find-by-name/host spelling (extension_policy.cpp:28-41, gl_extension_policy.cpp:109-124, CL `answered_by`), carried checks, never-report lists and invariants.
  - Proposal: `ExtensionRows` — `icd/common/extension_rows.hpp` — *Matches one driver's reportable-extension table against a host's names.* Owns rows, carried set, withheld set, version cap. The three policies become formatters (Vulkan count/array with `VK_INCOMPLETE`, CL string + `cl_name_version`, GL per-name), and the Vulkan one moves to icd/vulkan. `tools/check_extension_policy.py` keeps reading the same tables.
  - Golden-list tests per API first (byte-identical output). After RE4 (`withheld[]`) and growable-tables (row caps).
- **Name lists**: three name-pointer lists (executor/vulkan/extension_list, icd/vulkan/host_extension_list, icd/opengl/gl_extension_list) and two word lists (executor/opencl/cl_extension_list, CL policy `offered_by/same_word`).
  - Proposal: `ExtensionNames` — `core/support/extension_names.hpp` — *A set of name pointers, each kept once.* `ExtensionWords` — `core/support/extension_words.hpp` — *A space-separated name string edited word by word.* Built on growable-tables' container.
- **Version caps**: four copies (CL `cl_device_report.cpp:52-90` free functions ×2 uses, `ClPlatform::cap_version` cl_platform.cpp:81-100, GL `cap_version` guest_gl_context.cpp:500-519, Vulkan numeric physical_device_view.cpp:20,36). GL does not cap `GL_SHADING_LANGUAGE_VERSION` (CL caps its C version).
  - Proposal: `VersionText` — `icd/common/version_text.hpp` — *Reads the leading major.minor after a prefix and caps it at the implemented version.*
- **Tests**: test_extension_policy, test_cl_extension_policy, test_gl_extension_policy, test_icd_extensions, test_reported_extensions, `ctest -R check_extension_policy`, test_extension_list, test_cl_extension_list, test_host_extension_list, test_gl_extension_list, test_cl_device_report, test_cl_platform, test_guest_gl_context → add test_version_text, golden-list tests.
- **Identity divergence to decide** (policy, not duplication): GL reports the host's `GL_RENDERER`/`GL_VENDOR` (guest_gl_context.cpp:475), while Vulkan/CL report the Windows adapter name.

##### M8.5 Host D3D11/D3D12 device loss and renewal (decisions in shim)
- **Copies**: `lost()`, `declare(why)`, `renew` (lock → vacate → remake → `tenants_.renew()` → log) and test `lose()` near-identical in executor/shim/d3d11_host_device.cpp:353-451 and d3d12_host_device.cpp:99-166. `VulkanBackend::lose_device` resolves `vkAvLoseDeviceAV` itself (vulkan_backend.cpp:861-869) instead of `VulkanDeviceTable::lose` (vulkan_device_table.cpp:77).
- **Must differ**: D3D9 (one host device per guest device, no tenants). GL (loss via `GL_CONTEXT_LOST`, opengl_backend.cpp:226-228).
- **Proposal**: `HostDeviceRenewal` — `executor/host_device_renewal.hpp` (portable, covered) — *Declares one shared host device lost once, then remakes it when its tenants have left.* Owns `DeviceTenants`; remake through `RenewableDevice` virtual (D3D11 + D3D12).
- **Tests**: test_device_tenants, test_d3d_device_loss_device → add test_host_device_renewal (renew with tenants left, vacate timeout, remake failure).

##### M8.6 Status → API error spelling, GL error word, refusal logs
- **GL error word**: in M3.3 (`GlErrorWord`).
- **Status → API error**: inline at ~22 sites (Vulkan icd_root.cpp:425,1672; CL cl_external_memory.cpp:88-90, cl_icd_root.cpp:639, cl_icd_root_info.cpp:100,111; D3D umd_root.cpp:229, d3d_session.cpp:91, d3d11_device.cpp:634, …), all `no_memory`→OOM, `invalid_state`→lost, else site-specific.
  - Optional: `ErrorSpelling` — `icd/common/error_spelling.hpp` — *Spells a Status as one API's error, given that API's out-of-memory and lost codes.* Low value; do after M8.1.
- **Must differ**: `executor/d3d/command_refusal` removes one D3D device rather than the session (a D3D session hosts many devices).
- **Refusal logging**: M9.2.

#### M9. Logging, locks, knobs, digests, roots

##### M9.1 Driver-prefixed logging (guest and executor)
- **Guest**: 81 literal `"avvk: "/"avgl: "/"avcl: "/"avumd: "` writes across ~40 files, plus throwaway `DriverLog(log, "avumd: ")` per call (icd/d3d/adapter_identity.cpp:31,37,57; device_removal.cpp:47; icd/vulkan/swapchain.cpp:132; um/d3d/d3d_session.cpp:134). Fixes B-1.
  - Proposal (convention, no class): each root owns its `DriverLog`/`KeyedLog` and injects `DriverLog&` (not `LogSink&`) into every component, icd/common ones included.
- **Executor**: nine classes own `StdioLog sink_(stderr)` + `DriverLog` (context_session.hpp:128, d3d_backend.hpp:273, d3d9_session.hpp:209, d3d12_session.hpp:223, opengl_backend.hpp:128, shim/d3d11_host_device.hpp:299, shim/d3d12_host_device.hpp:110, shim/exportable_images.hpp:123, shim/loader_host_apis.hpp:91). Plus a temporary in opencl_backend.cpp:1001-1004 and bare `fprintf` in vulkan_submit_timing.cpp:64.
  - Proposal: `ExecutorRoot` owns the one `StdioLog`; every owner takes `LogSink&` (precedent: `HostShares` takes `DriverLog&`, host_shares.hpp:61).
- **Order**: FramePacer now (B-1); the guest sweep per ICD and the executor sweep as the last wave (they touch nearly every file).
- **Tests**: test_frame_pacer (+GL-prefix case), test_driver_log, test_keyed_log, per-root log tests; executor tests inject a recording `LogSink`.

##### M9.2 Refused-call logging policy
- **Copies**:
  - "not implemented, once": D3D9 `not_implemented` (um/d3d/d3d9_device.cpp:239-243), D3D11 (d3d11_device.cpp:134-140), D3D12 `unsupported` (d3d12_device.cpp:105-109), CL `ClRefusalLog::note` (cl_refusal_log.cpp:13-18).
  - "failed, once per code": D3D12 `answered` (:95-103), CL `note_failure` (:20-29); but D3D9 logs the first 32 failures overall (d3d9_device.cpp:29,182-190).
  - GL dropped payload never deduplicated (B-7).
  - Hand-rolled once bools: `GlIcdRoot::profile_noted_`, `FramePacer::failures_`.
- **Proposal**: `RefusedCalls` — `icd/common/refused_calls.hpp` — *Writes each refused entry point, with each code it answered, to its driver's log the first time.* `unserved(entry)`, `failed(entry, code)`, `dropped(entry, bytes)`, over the root's `KeyedLog`. `ClRefusalLog` keeps only its CL-specific lines. The lost-device line belongs to `LossLatch` (M8.1).
- **Tests**: test_keyed_log, test_cl_refusal_log, test_cl_icd_root_refusals, test_once_latch → add test_refused_calls.

##### M9.3 Lock-free append-only rows
- **Copies**: `KeyedLog::first` (core/log/keyed_log.cpp:13-30, `fetch_add` claim) and `ClInfoAnswers` (icd/opencl/cl_info_answers.hpp:46-105, CAS claim, "full stays exactly full").
- **Proposal**: `ClaimedRows<Row, Capacity>` — `core/support/claimed_rows.hpp` — *Appends rows that lock-free readers see only once each is fully written.* Needs chunked address-stable growth if growable-tables makes these grow.
- **Tests**: test_keyed_log, test_cl_info_answers → add test_claimed_rows.

##### M9.4 Thread locks: one contract for guest and executor
- **Copies**: the guest has `icd::ThreadLock`/`ThreadLockHold`/`ReportGate` with `SrwLock` + LockMarks. The device has `Host::create_mutex` with LockMarks. **The executor uses raw pthread in 7 classes with three idioms and no LockMarks** (stalls invisible to avinspector):
  - `HostMemorySource` (host_memory_source.hpp:96).
  - `HostShares` (private `Lock`, host_shares.cpp:16-37).
  - `DeviceTenants` (+cond).
  - `GpuClockHold` (+cond).
  - `SharedSurfaces`.
  - `D3d11HostDevice` (:290).
  - `D3d12HostDevice` `renewing_` (unlock per branch, d3d12_host_device.cpp:128-146).
- **Proposal**:
  - Move `ThreadLock`/`ThreadLockHold` to `core/platform/` and `ReportGate` to `core/trace/` (namespace `av`).
  - `PosixLock : ThreadLock` — `device/shim/posix_lock.hpp` — *A pthread mutex that marks the calling thread's trace slot as it waits, holds and lets go.* Plus `PosixCondition` beside it for the two condition users.
  - All seven executor classes use it; `report()`s use `ReportGate` (fixes B-2).
- **Order**: the move is include churn across ~30 guest files + tests/harness/icd/recording_thread_lock, so it lands alone, right after ordered-transport merges → `PosixLock` → HostMemorySource (after RE4) + HostShares → the rest.
- **Risk**: medium for `D3d11HostDevice` (lock entangled with the DXVK immediate context).
- **Tests**: test_thread_lock_hold, test_report_gate, test_locked_transport, test_host_memory_source (+report-while-held says busy), test_host_shares, test_device_tenants, test_gpu_clock_hold.
- **Also (avumd)**: the calling-thread id is read four ways (GL `ThreadSlot::thread_id`, um/d3d/umd_library.cpp:55,62, d3d11_device_objects.cpp:130, d3d11_call_trace.cpp:82).
  - `CallingThread` — `icd/common/calling_thread.hpp` — *Names the thread making the current call.* Windows half in um/common.
  - Move `ReentrantCallLock` to icd/d3d over `ThreadLock&` + `CallingThread&` so it is tested.
- Style debt: 38 (CL) + 15 (GL) manual `acquire()/release()` pairs → `ThreadLockHold`.

##### M9.5 Config and environment knobs
- **Host**: one reader, `EnvSetting`, but constructed at the point of use (a global, against constitution III):
  - device/context/ring_consumer.cpp:22.
  - device/shim/host_trace.cpp:70-93 (hand digit parse :84-88).
  - executor/shim/d3d_runtime.cpp:26,59,60,204-211.
  - executor/vulkan/vulkan_submit_timing.cpp:18.
  - The QEMU C glue repeats AV_PERFETTO→prctl (device/qemu/virtio-gpu-av.c:124-127).
- **Guest**: no reader.
  - `AV_D3D11_TRACE` (um/d3d/d3d11_call_trace.cpp:29).
  - `AV_D3D9_TRACE` (um/d3d/d3d9_device.cpp:104), where any value, even "0", means on, contradicting the host's '1' rule.
  - Flag files under `C:\ProgramData\av`, a path spelled in 4 files.
- **Proposal**:
  - `SettingText` — `core/support/setting_text.hpp` — *Reads one setting's text by the project's on/number rules.* `EnvSetting` gains `number()` over it.
  - `GuestSetting` — `um/common/guest_setting.hpp` — *Reads one of the process's AV_ environment variables by the same rule the host does.*
  - Roots read once and inject values.
  - One owner for the diagnostics directory in um/common.
  - A knob table in docs/testing.md.
- **Tests**: test_env_setting → add test_setting_text.

##### M9.6 D3D call tracing (D3D9 and D3D11 copies; D3D12 none)
- **Copies**: `D3d11CallTrace` (um/d3d/d3d11_call_trace.*: env window, file, context numbering, QPC clock) vs D3D9's hand-rolled trace inside `D3d9Device` (d3d9_device.cpp:104,136-150,194-206). The QPC→µs conversion is also in d3d11_device_pipeline.cpp:613, paging_queue.cpp:68, gdi_kmt.cpp:926, windows_trace_host.cpp:23.
- **Proposal**:
  - `D3dCallTrace` (renamed, stays um/d3d, one owner) — *Writes the runtime's calls into this driver, within the window its setting asks for, to the process's call file.* D3D9 and D3D12 use it through `DeviceCall::traced`.
  - `PerformanceClock` — `um/common/performance_clock.hpp` — *Reads the process's performance counter in microseconds.*
- Proved by the guest run plus tools/d3d11_report.py / d3d12_report.py.

##### M9.7 FNV-1a digest
- **Copies**: `ClBinaryDigest` (executor/opencl/cl_binary_digest.hpp:16-33) and `GuestPageList::fingerprint` (proto/resources/guest_page_list.cpp:68-76; keep its value identical). core/trace/scope_name.hpp stays constexpr and exempt.
- **Proposal**: `Fnv1aDigest` — `core/support/fnv1a_digest.hpp` — *Folds bytes or words into a 64-bit FNV-1a value.*
- **Tests**: test_cl_binary_digest → test_fnv1a_digest, test_cl_binary_envelope, test_guest_page_list (pin today's values).
- **Not shared, on purpose**: program/pipeline binary stamping (CL stamps UUIDs because rusticl accepts corrupt binaries; GL/Vk drivers validate their own headers). A shared class would have one user.

##### M9.8 Driver seams boilerplate
- **Copies**: heap + `FileLog(L"avxx")` + `GdiKmt` + `ProcessTrace` + `initialize(){return kmt.initialize();}`, ×4 (um/vulkan/icd_library.cpp:8-21, um/opencl/cl_icd_library.cpp:10-24, um/d3d/umd_library.cpp:13-25, um/opengl/gl_icd_library.cpp).
- **Proposal**: `DriverSeams` — `um/common/driver_seams.hpp` — *Owns the heap, log, D3DKMT and trace every graphics driver's root is built over.* Unify `ReportGate` section names ("root"/"clicd"/"umd") to the driver name.
- Proved by the guest image run only (um).

#### M10. Already-considered and rejected (record so nobody re-audits)
- `CapsReply` (D3D) vs `ClInfoRequest` (CL): contracts differ (zero-fill + prefix vs refuse short + report size).
- `ClInfoAnswers` for Vulkan: would be a new feature, not a dedupe.
- `D3d11BatchProgress` vs `D3d12QueueProgress`: share about 10 lines over different fence types.
- `UnansweredTransport`: CL-only at runtime; Vulkan does the same at generation time (tools/codegen/profiles/vulkan.toml:399-420). Move it from `av::icd` to `av::clicd` namespace.
- Sibling-driver private export resolution (`WindowsGlShareSource::resolve`, `SharedRowsScope`): two lines each.
- `DisplayHeads::say_once` (device binary): leave it.

---

### 5. Mechanisms that must stay per-API (and why)

| Mechanism | Why it differs |
|---|---|
| Id minting direction (D3D guest-minted; VK/CL host-minted; GL host names) | Batched D3D creates have no round trip; converging is a protocol redesign |
| GL objects need no registry | A bad GL name is a GL error, not a dereference (icd/opengl/gl_external_objects.hpp:4-11) |
| GL session teardown = context close | The GL context frees its objects |
| `SyncTokens`/`RetiredSyncTokens` bounded retirement | Resolve never deletes GLsyncs: GL-only policy |
| CL event callback generations | CL cannot unregister a callback |
| Vulkan one context per instance (no `LinkGenerations`) | Host handles are scoped to one context |
| D3D kernel-mediated waits (monitored fences, `BatchFollower`, `KernelContextGate`) | dxgkrnl owns them |
| `command_refusal` removing one D3D device | One D3D session hosts many devices |
| D3D9 host device per guest device (no tenants) | DXVK D3D9 device model |
| Flip routes (D3D11 `show`, D3D9 `flip`, D3D12 `share`) and the kernel present DDIs | Each producer device and runtime DDI differs |
| WGL pixel formats, Vulkan WSI handles | API-specific objects |
| Map-flag policy, write-detection method | docs/mapped-buffers-plan.md §2/§4 |
| CL–D3D11 (guest) vs CL–GL (host) copy mirrors | Different binaries and reach (COM in guest vs host share group) |
| Program binary stamping | CL lacks driver header validation |
| Capability answers themselves | DXVK/vkd3d/rusticl/Mesa answer differently (only the transport is shared) |

---

### 6. Decisions taken where the area audits overlapped

1. `ChainSlots` (presentation) = the window-chain item from the interop audit: one item, M6.1, owned by the presentation lane.
2. `SlotFormat` reads DXGI values from `DxgiFormat` → M5.4 runs after M5.1.
3. Icd thread event: renamed `ThreadWake` (avoids two `WakeSignal`s) and moved to icd/common **when** Vulkan joins `WindowPresentQueue` (M6.4). Until then `um/common/windows_wake_signal` violates the ≥2-users rule; that resolves itself in M6.4, so it is not moved to um/opengl first (no churn).
4. Lost-device logging goes to `LossLatch` (M8.1), not `RefusedCalls` (M9.2).
5. GL error word layout (`GlErrorWord`) is part of the published-tail work (M3.3), same agent.
6. `ArenaSpans` (M4.1) places the tail by `PublishedSlotLayout` (M3.3) → M3.3 first.
7. The host Vulkan memory work from three audits (DmaBuf, import primitive, `VulkanMemoryImport` over `VulkanDeviceTable`, exportable-image unification) is one item, M2.4, for one agent.
8. `HostExportedFds` is deleted by M7.6, so M1.2 folds only `HostTemplateTable` into the Vulkan registry.
9. `AdoptedBlobs`/`ImportedMemories` are deleted by M2.2 (via `ImportTable::named`), not folded into `HostObjectRegistry`.
10. The "publish barrier" (flush held commands before another API/process names an object) is handed to ordered-transport, not planned here.
11. Executor time-bounded loops: one `PollBudget` (M3.6). The `PosixCondition` waits (M9.4) are separate (they block on a condition, not poll).

---

### 7. Hot files — items that touch them must run in this order

| File | Items (in order) |
|---|---|
| executor/d3d/d3d_backend.cpp | M3.6 (`await_gpu_unlocked`) → M4.3 (UpdateSubresource formula) → M6.5 (`on_PresentPoint`) → M2.2/M2.3 (adopt, region hook) → M1.2/M1.1 (object table) → M7.4 (`import_object`) → M9.1 executor sweep |
| executor/d3d/d3d12_session*.cpp | M3.3 (delete fence family) → M3.6 (`drain`) → M5.5 (bulk caps) → M6.5 → M2.2/M2.3 → M2.4 (fd close :420) → M1.1/M1.2 → M9.1 |
| executor/shim/d3d11_host_device.cpp | M3.6 (PollBudget) → M8.5 (renewal) → M4.6 (BgraRows readback) → M9.4 (PosixLock) → M9.1 |
| executor/shim/d3d12_host_device.cpp | M8.2 wire fields (`measure`) → M8.5 → M9.4 → M9.1 |
| executor/vulkan/vulkan_backend.cpp | M8.5 (`lose_device` via table) → M1.6 → M2.2 → M2.4 → M1.1/M1.2 (fixes B-3) → M1.5 (codegen hooks) → M7.6 |
| executor/opencl/opencl_backend.cpp | M7.5 → M1.1/M1.2 → M2.3 → M9.1 |
| executor/opengl/opengl_backend.cpp | M3.3 (`publish_errors`) → M2.5 (pack buffer) → M2.3 → M9.1 |
| executor/executor_root.* | M1.6 → M9.1 |
| executor/sharing/host_shares.* | M2.4 (DmaBuf) → M9.4 → M7.6 → M7.4 |
| executor/host_memory_source.* | RE4 → B-2 one-liner → M9.4 |
| icd/vulkan/icd_root.cpp | M7.1 (`publish`) → M3.1 (waits) → M8.2 (`span_adapters`) → M7.2 → M8.1 (`lost`) → M7.6 → M9.1/M9.2 |
| icd/vulkan/swapchain.cpp | M6.2 → M6.3 → M5.4 (`shows_bgra`) → M4.6 (`to_bgra`) → M6.4 |
| icd/opengl/guest_gl_context.cpp | M8.4 (`cap_version`) → M6.2 (`pace`) → M3.4 (`end_frame`) → M8.1/M8.3 (`wire`) → M9.1 |
| icd/opengl/gl_icd_root.cpp | M1.3 (IdMint) → M6.2 → M9.2 (B-7) → M9.1 |
| icd/opencl/cl_icd_root*.cpp | M8.2 (`describe_adapter`) → M7.3 (twin size) → M8.4 → M9.2 → M9.1 |
| um/common/dxgi_window_chains.cpp | M7.1 (`identify`) → M5.4 → M6.1 → M7.3 |
| um/d3d/d3d11_device*.cpp | M6.6 → M8.1 → M9.2 → M9.6 → M4.4 |
| um/d3d/d3d9_device*.cpp | M9.5 (env) → M9.6 → M9.2 → M6.6 → M8.1 → M4.4 |
| icd/common/host_channel.* | ordered-transport → M2.1 → M8.1 (`lost`, `fault_code`) |
| guest/transport/kmt_transport.cpp, kmt_blob_arena.* | ordered-transport → M3.2 → M3.3 |

---

### 8. Waves and lanes

A **lane** is one agent's ordered list of items; lanes in the same wave own disjoint files (§7 resolves the hot ones). Every item ends with `tools/gate.sh` (design, symbol tests, kernel cross-build, 100% branch coverage of core/device/proto, fuzz smoke). Items that touch shim/GPU code also run gpu-test.sh on all three hosts (local AMD, test Arc, accel Iris+NVIDIA), one agent per host. Per user memory, ask before any long suite (full conformance, coverage).

#### Wave 1 — start now, alongside the three in-flight branches (no overlap with their files)

| Lane | Items | Owns |
|---|---|---|
| 1A Formats | M5.2 FourCc → M5.1 DxgiFormat (fixes B-4) → M5.3 D3d9PresentFormat | proto/d3d/*, proto/vulkan/exportable_image_info.*, core/support/four_cc.*, executor/shim/drm_fourcc.* (delete), executor/shim/host_image_export.cpp (table only), icd/opencl/dxgi_image_format.*, executor/opencl/dxgi_cl_format.*, icd/d3d/{dxgi_format_table,present_format,d3d9_format_ops}.*, device/display/scanout_format.cpp, the four `format_count` headers, executor/d3d/d3d9_flip_shares.cpp (:27,:75 only), um/d3d/d3d11_device_present.cpp (`dxgi_format_of` only) |
| 1B Shared allocation data | M7.1 (fuzz target included) | proto/resources/shared_allocation_data.*, miniport/memory/allocation_table.cpp, miniport/scheduling/device_table.cpp, icd/d3d/kernel_surface.*, icd/common/shared_handles.*, icd/vulkan/icd_root.cpp (`publish` only), um/common/dxgi_window_chains.cpp (`identify` only) |
| 1C Geometry | M4.3 PitchedExtent/PitchedRows (fixes B-5) | proto/resources/{pitched_extent,pitched_rows}.*, icd/d3d/{texture_upload,pitched_rows}.*, icd/opencl/{cl_image_region,cl_buffer_rect,cl_staged_rows}.*, icd/opengl/pixel_store.*, executor/d3d/{d3d9_texture_write,d3d12_footprint}.*, executor/opencl/cl_bounced_rect.*, d3d_backend.cpp UpdateSubresource formula (after 1D's edit merges, or 1D skips that file until 1C lands) |
| 1D Host polls/wake | M3.6 (PollBudget time form; EventfdWake; DXVK patch 0009 + D3D11 watch) | executor/poll_budget.*, executor/eventfd_wake.*, executor/d3d/{d3d12_fence_wake,d3d11_batch_progress,shared_surfaces}.*, d3d12_session.cpp `drain`, d3d_backend.cpp `await_gpu_unlocked`/`poll`, shim/d3d11_host_device.cpp (PollBudget sites), gpu_clock_hold.cpp, device_tenants.cpp, tools/dxvk-patches/ |
| 1E Host D3D present + renewal | M6.5 (`SharedSurfaces::stage`, `StagedFrame` base, `ComposeOrder`) → M8.5 HostDeviceRenewal | executor/d3d/{staged_frame,d3d9_session_resources,d3d9_flip_shares}, `on_PresentPoint`/`on_PresentResource12` functions, executor/shim/{d3d9,d3d11,d3d12}_frame_pool.*, shim/d3d12_presentation.*, shim/compose_order.*, shim/{d3d11,d3d12}_host_device.* (renewal functions; after 1D's PollBudget edit), executor/host_device_renewal.* |
| 1F Guest D3D small | M1.4 DeviceBlock + RecordStamp → M6.6 PresentCount/PresentedAllocations | icd/d3d/{device_block,record_stamp,present_count}.*, um/d3d/{device_handle,d3d12_device_handle,d3d11_records,d3d12_records}.*, um/d3d/d3d*_device_present.cpp (except 1A's one function), tools/gen_d3d*_ddi.py, um/d3d/generated/* |
| 1G Guest present | M6.2 PresentPacing → M6.3 WindowExtent → M6.1 ChainSlots (after 1B) → M5.4 SlotFormat (after 1A) | icd/common/{present_pacing,window_extent,chain_slots,slot_format}.*, icd/opengl/{swap_interval,window_visibility,chained_frames,back_buffer_presenter(prepare)}.*, icd/opengl/guest_gl_context.cpp (`pace/admits` only), icd/opengl/gl_icd_root.cpp (swap interval only), icd/vulkan/{swapchain,chained_image,surface_properties}.*, um/common/dxgi_window_chains.cpp (slot format, after 1B) |
| 1H Executor teardown + small | M1.6 `SessionBackend::shut_down` → M2.5 (GlPackBuffer over GlSharedBuffer; ClReadAhead over StagingBytes) → M7.5 GlMirror write-only | executor/session_backend.hpp, executor_root.*, the four backends' `shut_down` decl/def, executor/opengl/gl_pack_buffer.*, executor/shim/gl_shared_buffer.*, executor/opencl/{cl_read_ahead,gl_mirror,copied_gl_sharing}.* |
| 1I Adapter facts + errors word | M8.2 part 1 (AdapterFacts, pci_identity, delete D3D AdapterIdentity, drop unread host identity wire fields, host UUID reads, `lose_device` via table) → M3.3 step 1 (delete D3D12 fence family) → M3.3 steps 2–3 for the GL error word only (`GlErrorWord`, `PublishedSlots` over GuestRegion with sfence) | icd/common/adapter_facts.*, guest/kmt/pci_identity.hpp, icd/vendor/vendor_adapters.cpp, icd/opencl/cl_icd_root.cpp (`describe_adapter`), icd/vulkan/connection.cpp (name read), icd/d3d/adapter_identity.* (delete), umd_adapter.*, umd_root.cpp, d3d codegen toml/generated, executor/shim/{d3d11_capabilities,physical_device_list,loader_host_apis}.cpp, d3d12_host_device.cpp (`measure`, before 1E), d3d9_session.cpp (`on_QueryCaps9`), um/d3d/d3d9_host_caps.cpp, d3d12 fence files, proto/submission/{gl_error_word,published_slot_layout}.*, executor/{guest_region,published_slots}.*, executor/opengl/{gl_error_tally,opengl_backend.cpp publish_errors}, icd/opengl/reported_gl_errors.* |
| 1J Logs/knobs/digest/seams | B-1 FramePacer prefix, B-7 note_dropped → M9.2 RefusedCalls → M9.5 SettingText/GuestSetting → M9.6 D3dCallTrace + PerformanceClock → M9.7 Fnv1aDigest → M9.8 DriverSeams | icd/common/{frame_pacer,refused_calls}.*, icd/opencl/cl_refusal_log.*, um/d3d/{d3d9_device,d3d11_device,d3d12_device}.cpp (refusal + trace functions only; after 1F), um/d3d/d3d11_call_trace.*, um/common/{guest_setting,performance_clock,driver_seams}.*, device/host/env_setting.*, core/support/{setting_text,fnv1a_digest}.*, device/shim/host_trace.cpp, executor/opencl/cl_binary_digest.* (delete), proto/resources/guest_page_list.cpp, um/*/…_library.cpp |
| 1K Guest completion waits | M3.1 steps 1–4 (HostCompletionWait; Vulkan fold; CL; GL glClientWaitSync), then 5–6 (glFinish/queries handwritten, vkGetQueryPoolResults) | icd/common/host_completion_wait.*, icd/vulkan/icd_root.cpp (wait functions only; after 1B's `publish`), icd/vulkan/local_entries.*, icd/opencl/cl_blocking_wait.*, icd/opengl/{manual_calls,manual_entries}.cpp, tools/codegen/av_codegen/opengl/emit.py, proto/generated/opengl/* |

Within wave 1, these pairs share a file. Merge in the order shown; the second agent rebases:
- 1D before 1C on d3d_backend.cpp.
- 1D before 1E on d3d11_host_device.cpp.
- 1I before 1E on d3d12_host_device.cpp.
- 1B before 1G and 1K on dxgi_window_chains.cpp and icd_root.cpp.
- 1A before 1G on SlotFormat.
- 1F before 1J on um/d3d devices.

Every other lane pair is disjoint.

Bug fix B-2 (HostMemorySource report race) touches RE4's file, so hand it to the RE4 agent as a one-liner.

#### Wave 2 — after growable-tables merges
| Lane | Items |
|---|---|
| 2A Registries | M1.1 KeyedSlots (or adopt growable-tables' keyed container) → M1.2 HostObjectRegistry: CL → ClGlMirrors → Vulkan (fixes B-3; folds HostTemplateTable) → D3D |
| 2B Guest indexes | M1.3 HandleTree dedupe index + IdMint (IdMint's D3D call site after ordered-transport) |
| 2C Extensions | M8.4 ExtensionNames/ExtensionWords + VersionText; ExtensionRows waits for RE4 |
| 2D Adapter caps | M8.2 caps and `AvgpuAdapters::luids` (cl_devices, icd_root `span_adapters`, umd_connections, adapter_sessions) |
| 2E Lock-free rows | M9.3 ClaimedRows (needs growable-tables' chunked growth decision) |

#### Wave 3 — after ordered-transport merges
| Lane | Items |
|---|---|
| 3A Lock contract (solo, short, first) | M9.4 step 1: ThreadLock/ThreadLockHold → core/platform, ReportGate → core/trace (include churn; nothing else in flight on guest files while it lands) |
| 3B Transport leftovers | M3.2 CounterSpin → M3.4 TransportTimeline → M3.5 D3dDeviceChannel over LockedTransport (or give M3.5 to the ordered-transport agent before it merges) |
| 3C Loss | M8.1 HostChannel::lost/fault_code + LossLatch (fixes B-8) → M8.3 LinkGenerations |
| 3D Arena + pieces | M3.3 step 3 for D3D queries (PublishedSlotReader, fixes B-6) → M4.1 ArenaSpans → M4.2 TransferRoom |
| 3E Guest memory owner | M2.1 HostChannel owner + `view_at` |
| 3F Executor locks | M9.4 steps 2–4: PosixLock, PosixCondition, the seven executor classes (HostMemorySource after RE4) |
| 3G Guest present queue | M6.4 WindowPresentQueue + ThreadWake → Vulkan async → FramesInFlight to icd/common |

#### Wave 4 — after RE4 merges (and wave 2A for the registries)
| Lane | Items |
|---|---|
| 4A Imports | M2.2 ImportTable::named (delete AdoptedBlobs, ImportedMemories) → M2.3 codegen `guest_bytes` |
| 4B Host Vulkan memory | M2.4 (DmaBuf, import primitive, VulkanMemoryImport over VulkanDeviceTable, exportable images, ExportedImageLayout) |
| 4C Extension rows + adapter memory | M8.4 ExtensionRows (golden lists first) → M8.2 AdapterMemory |
| 4D Vulkan sharing | M7.2 OpenedObjects for Vulkan → M7.3 SharedRowsTexture → (after 4B) M7.6 one Vulkan sharing path |
| 4E Codegen hooks | M1.5 Vulkan generated translation hooks (after 2A's Vulkan registry) |

#### Wave 5 — after waves 3 and 4
| Lane | Items |
|---|---|
| 5A Staged maps | M4.4 StagedMapping/OpenMappings/WrittenRuns → M4.5 buffer slices for D3D9 and GL |
| 5B D3D import + share state | M7.4 D3D `import_object` + D3dShareState (after 4B, 4D) |
| 5C D3D12 bulk format caps | M5.5 |
| 5D Log sweep | M9.1 DriverLog injection: guest per ICD (vk, cl, gl, d3d), executor owners; then CL/GL manual acquire/release → ThreadLockHold; CallingThread + ReentrantCallLock move |
| 5E Optional | M3.3 step 4 (published status for D3D9 queries, GL queries, CL events, Vulkan fences: one API per change, measured); M8.6 ErrorSpelling; M4.6 BgraRows if not taken earlier (touches swapchain.cpp, front_buffer.cpp, d3d11_host_device.cpp: schedule after 1G/1E/3G) |

#### Overlap summary with the in-flight branches
- **growable-tables**:
  - Every fixed cap above (§3).
  - Wave 2 entirely.
  - Ask it to skip `AdoptedBlobs`, `ImportedMemories`, `DeviceMemory::held_`, `HostExportedFds`, `D3d12FencePublisher` (deleted here).
  - Provide chunked address-stable growth for `KeyedLog`/`ClInfoAnswers`.
  - Do not keep "registry full ⇒ lost" (vulkan_backend.cpp:119, opencl_backend.cpp:429).
- **ordered-transport**:
  - Owns `KmtTransport`, `HostChannel`, `LockedTransport`, `D3dCommandStream`, `BatchedTransport`, leases. Wave 3 waits for it.
  - Offer it: M3.5 (D3dDeviceChannel over LockedTransport), the "publish barrier" (three sites), the per-ICD `send_lock`s.
  - Wave-1 lanes 1F and 1J touch um/d3d device files that may also hold `hold(gate_)`/`follow` calls. Leave those calls untouched.
- **RE4**:
  - Wave 4 waits for it.
  - Hand it B-2 (HostMemorySource report race) as a one-liner.
  - Its `withheld[]` edit precedes ExtensionRows.
  - Its VK_EXT_external_memory_host import lands in `vulkan_memory_import.cpp` before M2.4 restructures it.
  - Its CL `Memory::application` and GL pinned memory precede M4.4.
