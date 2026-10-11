# Plan: `HostShares`, one source of truth for every shared object, every API

A plan for one agent session to carry out from start to finish. Each step
ends in a state that builds and passes its tests. Read the whole file before
starting. Where this plan and the code disagree, the code is right: stop,
look, and adapt the step, then say so in the hand-back.

## 1. Goal

A guest shared object is something the guest's kernel driver minted a
**shared id** for (feature 053, `SharedObjectRecord`). It can be:

- a Direct3D 11 texture DWM composes;
- a Direct3D 12 swapchain buffer;
- Vulkan memory or a fence;
- an OpenGL or OpenCL import of any of those.

Today the host keeps track of such an object in up to five places, and no
fewer than three of them have fixed capacities:

| Where | What it knows | Capacity |
|---|---|---|
| executor, per host GPU: `D3dSharedResources` | id → D3D11/D3D12 resource, shown copy, writes | 4096 |
| executor, per host GPU: `InteropTextures` | the exportable memory behind D3D11 shared textures | **1024** (ran out in practice) |
| device, per guest device: `HostSharedObjects` | id → exported fd, kind, size, holders | 256 |
| device, per context: `ContextSharedObjects` | which ids this context published or opened | 64 |
| executor, per session: GL `GlSharedObjects`, CL `ClSharedObjects`, Vulkan `HostExportedFds`/`AdoptedBlobs`/`HandleRegistry` | this session's handles for what it imported | 64 / 64 / 64 / 256 |

Because the knowledge is split, each piece has its own lifetime rules. A
release has to visit several of them, a holder count can disagree with the
objects it counts (see §6), and nothing can say what an id is and who holds
it.

After this plan:

- **On the host**, one subsystem in the executor, `HostShares`, owns every
  shared object of every API. It holds the host API object (or objects), the
  exported descriptor, the exportable image, every holder, and the
  object's state. It has no fixed small capacity. Every API's backend
  publishes, opens and closes through it.
- **On the device**, nothing owns a shared object any more. The device only
  routes guest commands, and the guest's "released everywhere" message, to
  the executor.
- **In the guest**, the kernel driver's `SharedObjectTable` stays the guest's
  source of truth for ids. The guest's per-process tables lose their fixed
  caps of 64, and the driver's queue of released ids loses its cap of 256.

The per-session tables of OpenGL, OpenCL and Vulkan keep only what they
must: the session's own API handle for each object it imported (a GL name, a
`cl_mem`, a `VkDeviceMemory`), so that the backend can destroy it. They no
longer decide anything about sharing.

### Why now

Mirrored windows (remotewin, in the LSW repository) get their frames as
dma-bufs through the export socket, from textures that have exportable
memory. DWM keeps every Windows.Graphics.Capture frame-pool buffer it has
opened until DWM restarts. This was proven on the running system on
2026-10-06:

- The guest releases everything else correctly.
- A second DWM kernel device keeps a KMT open on each buffer, so dxgkrnl
  never destroys it.
- No GPU work was pending.
- Restarting DWM frees them.

So every mirrored window permanently holds about three exportable textures.
When the 1024 slots ran out, every later surface could not be exported, and
mirrors silently fell back to copying pixels.

The memory DWM keeps leaks whether or not a texture had a slot, so the fixed
table protects nothing; it only adds a second failure. The same pattern, a
fixed table in front of resources the guest can create without bound, is in
every row of the table above. DWM's leak itself is out of scope: after this
plan it costs memory, and that memory is visible in the state dump and the
log. It no longer costs correctness or speed.

## 2. Read first

In this repository:

- `CLAUDE.md`. These rules are binding: a class per header, `AV_SCOPE` as
  the first line of every function definition, no standard library, no
  exceptions, `av::Status`, no statics, no Manager/Helper/Utils names, the
  comment style, and tests that judge outcomes.
- `docs/constitution.md`: technology constraints, Principle IV.
- `docs/architecture.md`: the directory map, the layer rules (nothing in
  `device` includes `executor`), and the paths the build and the gates name.
- `docs/testing.md`, especially the fullstack test
  `test_fullstack_shared_objects.cpp` (feature 053) and how tests are
  registered.
- `docs/trace.md`: state dumps and `/dev/shm/av-state-*.sock`.
- `docs/shared-surfaces.md` and `docs/external-export.md`. Note that §1.2 of
  `shared-surfaces.md` is out of date: `D3dBackend::export_object` now
  exports.
- `docs/renderer.md`: the av-renderer process mode. The executor and the
  per-context work run in another process there; the plan must work in both
  modes.
- `src/device/abi/av_executor.h`: the executor ABI (version 15 today).

**Project rule from the user: no migration or backward-compatibility code.**
Device and executor are built together from this repository. When the ABI
changes, raise the minimum and delete the old paths; do not keep a fallback.

## 3. The map of today

All paths are relative to the repository root. This was surveyed on
2026-10-06; verify any line number before you rely on it.

### 3.1 Ownership

- **`Library`** (file-static in `src/executor/shim/executor_ops.cpp`) owns
  `ObjectStorage<LoaderHostApis> apis_` and `ObjectStorage<ExecutorRoot>
  root_`.
- **`LoaderHostApis`** (`src/executor/shim/loader_host_apis.hpp`) owns, per
  host GPU (8 at most):
  - `D3d11HostDevice d3d11_devices_[8]`, which contains:
    - `D3dSharedResources shared_` (4096)
    - `InteropTextures interop_` (1024)
    - `DxvkVulkanDevice vulkan_`
    - `HostImageExport exports_`
    - `D3d11FramePool frames_`
    - `mutex_`, the D3D11 device lock
  - `D3d12HostDevice d3d12_devices_[8]`
  - `D3d9HostDevice d3d9_devices_[8]`
  - `SharedSurfaces surfaces_[8]`
  - `GlDisplay[8]` and `OpenClDevice[8]`
- **`ExecutorRoot`** (`src/executor/executor_root.*`) borrows those through
  `HostApis`, and owns:
  - `ContextSession sessions_[64]`
  - per-session backends `VulkanBackend vulkan_[64]`, `OpenGlBackend
    opengl_[64]`, `OpenClBackend opencl_[64]` and `D3dBackend d3d_[64]`
- **The device** (`src/device/`): `VirtioGpuAv` owns:
  - `LocalContextPlane local_`, which contains `HostSharedObjects shared_`
    (256, own Host mutex) and `DeviceContextTable contexts_`. Each
    `DeviceContext` holds a `ContextSharedObjects shared_` (64).
  - `RendererContextPlane remote_`: in av-renderer mode, `RendererService`
    has its own `LocalContextPlane plane_`.
  - `ExportService exports_`: the external export socket.
- **The miniport**: `Adapter::shared_` is `SharedObjectTable`: an
  `ObjectPool<SharedObject>`, the monotonic `next_id_`, and `released_[256]`
  with a `released_dropped_` count.

### 3.2 Flows

**1. Publish a shared object (Vulkan memory or fence; a D3D11 texture made
for interop).**

1. Guest UMD or ICD: `SharedObjectCall::share(id, kind, size, wire_handle)`.
2. Device: `SharedObjectCommand::run` → `ContextSharedObjects::publish`.
3. That calls `ContextExecutor::export_object`, which goes through the ABI to
   `ExecutorRoot::export_object`, then `ContextSession`, then the backend's
   `export_object`:
   - Vulkan: `VulkanHostImport::export_memory` (DMA_BUF) or
     `export_semaphore` (OPAQUE_FD).
   - D3D: `D3d11HostDevice::export_memory`, which calls
     `InteropTextures::export_memory`.
4. Back on the device, `HostSharedObjects::publish(id, kind, handle_kind,
   size, fd)` stores the descriptor with one holder (the guest's).

**2. Open (another context, maybe another API).**

1. Guest: `SharedHandles::adopt`, then `SharedObjectCall::open(id, kind,
   size)`.
2. Device: `ContextSharedObjects::open` → `HostSharedObjects::open` (+1
   holder, gets the fd).
3. Then `executor import_object(kind, handle_kind, fd, size)`. The importer
   duplicates the fd. Per backend:
   - GL: `GlHostImport::import_memory` or `import_semaphore`, then
     `GlSharedObjects::remember`.
   - Vulkan: `VulkanHostImport::import_shared_memory` or
     `import_shared_semaphore`, then `HandleRegistry`.
   - CL: `GlHostImport`, `GlSharedBuffer::make` and `clCreateFromGLBuffer`,
     then `ClSharedObjects::remember`.
   - D3D: `import_object` refuses.

**3. Direct3D's own sharing (not feature 053's command, the D3D wire).**

- `D3dBackend::on_CreateResource(id, desc, shared_key)`:
  1. `shareable()` → `D3d11HostDevice::make_shareable` →
     `InteropTextures::make`
  2. `SurfaceIdentity::mark`
  3. `D3dSharedResources::publish`
- `on_OpenResource(id, key)`: `find_shown` → `find_shared`. For a D3D12 key,
  `view_of` runs `D3d12HostDevice::export_image` → `Vkd3dImageExport` →
  `InteropTextures::import` → `D3dSharedResources::replace`.
- `D3d12Presentation::share`: `shares(key)` then `publish_shared(key,
  ID3D12Resource)`. These are two separate trips of the D3D11 lock.
- `on_PresentPoint` (point 0): `D3d11HostDevice::show` makes the flip copy.
- `counted()` → `D3dSharedResources::written`.

**4. The export socket (remotewin).** `ExportedGpu::export_surface` →
`LocalContextPlane` → ABI `export_surface(gpu, surface)` →
`ExecutorRoot::export_surface` → `D3d11HostDevice::export_surface` →
`D3dSharedResources::find` + `InteropTextures::export_image`.
`sync_surface` goes the same way to `writes()` + `DxvkVulkanDevice::sync_file`.

**5. Guest-wide release (0x02F1).**

1. Miniport: `AllocationTable::destroy` → `SharedObjectTable::forget` (queue)
   → `Adapter::release_shared` → `SharedObjectRelease::send`.
2. Device: `SharedCommands::answer` → `LocalContextPlane::shared_released`,
   which:
   - calls `HostSharedObjects::close` (or `release_all` for id 0);
   - calls ABI `release_shared(gpu, id)` → `ExecutorRoot::release_shared` →
     `D3dSharedResources::release` + `SharedSurfaces::release`;
   - also calls `ExportService::revoke`.
3. Device reset: `release_shared(gpu, 0)` + `revoke(0)`.

**6. Context teardown.** `ContextSharedObjects::release_all` closes every
slot on `HostSharedObjects`, published or opened. See §6.

**7. Per-session release (a GL or Vulkan import deleted).**
`SharedObjectCall::release` → `ContextSharedObjects::release` → `give_back`
→ `ContextExecutor::release_object` + `HostSharedObjects::close`.

### 3.3 Scoping problem to fix on the way

The guest's ids are minted **per adapter**, which on the host means per
`VirtioGpuAv` device instance. The D3D tables are **per host GPU**. Two guest
devices on one host GPU would mix ids; `docs/shared-surfaces.md` §2.1 notes
this. `HostShares` keys every row by **(domain, id)**, where the domain is
the guest device instance (§4.4).

### 3.4 Locks

- `D3d11HostDevice::mutex_` guards DXVK's immediate context. `D3dBackend`
  holds it from a batch's first D3D11 command until `end_batch`. D3D12 and
  D3D9 commands drop it first.
- Per-session backends run on their context's worker thread only.
- `HostSharedObjects` has its own Host mutex, because any worker reaches it.
- `SharedSurfaces` has its own mutex, never held across an API call.

### 3.5 Big tables in the executor

The executor has no `Platform`, so it cannot use `ObjectPool`. Its idiom is a
reserved, lazily committed mapping: `HandleRegistry::reserve()`
(`src/executor/vulkan/handle_registry.cpp`) uses
`mmap(MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE)` and touches pages only as
rows are used. The guest ICDs do have a `Platform` and `ObjectPool`; see
`src/icd/d3d/device_table.hpp` and `src/icd/common/handle_tree.hpp`.

## 4. Target design

### 4.1 Layers after the change

```
guest   miniport SharedObjectTable ── the guest's truth: ids, and when one is gone everywhere
        ICD/UMD SharedHandles, OpenedObjects, ExternalObjects ── per process, pooled (no cap)
          │  SharedObjectCall share/open/release, D3D wire CreateResource/OpenResource, 0x02F1
device  routing only: SharedObjectCommand, SharedCommands, ExportService (fds it handed out)
          │  executor ABI 16: share_object / open_object / close_object / release_shared / ... (domain-scoped)
executor HostShares (one per host GPU, rows keyed (domain, id)) ── the host's truth, every API
          ├─ Share rows: kind, size, origin, host objects, descriptor, exportable image, holders, state
          ├─ ShareParticipant (each backend kind): export / import / destroy its own API object
          └─ ExportableImages (shim): Vulkan + DXVK calls for exportable D3D11 memory
```

### 4.2 Classes

| Class | Where | Kind | Responsibility |
|---|---|---|---|
| `HostShares` | `src/executor/sharing/host_shares.{hpp,cpp}` | system class, one per host GPU | Every shared object on one host GPU, by (domain, id): publish, open, close, release, show, count writes, export to the socket, collect, report. Its lock guards its rows; see §4.6. |
| `Share` | `src/executor/sharing/share.{hpp,cpp}` | object class | One shared object. Kind (memory, monitored fence, native fence) and size. Origin: the publishing session and its API. The host objects: a D3D11 view (COM), a D3D12 source (COM), or a Vulkan/GL origin handle. The exported descriptor (fd + handle kind), made once when another API first needs it. The `ExportableImage`, the shown copy and the write count, all D3D. The holders. The state. |
| `ShareHolders` | `src/executor/sharing/share_holders.{hpp,cpp}` | object class | Who holds a `Share`: the guest's hold (dropped by 0x02F1), and each session's hold with that session's own handle (wire handle) for it. One session holding twice is refused, as the device refuses it today. |
| `ShareTable` | `src/executor/sharing/share_table.{hpp,cpp}` | object class | Reserved, lazily committed rows for `Share`s and their holders, a free list, and an open-addressed (domain, id) → row index. No small fixed capacity (§4.5). |
| `ShareParticipant` | `src/executor/sharing/share_participant.hpp` | interface | What a backend does for sharing: `export_object(kind, wire_handle, &handle_kind, &fd)`, `import_object(kind, handle_kind, fd, size, &wire_handle)` and `destroy_object(kind, wire_handle)`. Each backend implements it. It replaces the three `SessionBackend` virtuals of the same purpose, which are deleted. |
| `ExportableImages` / `ExportableImage` | `src/executor/shim/exportable_images.*`, `exportable_image.*` | system class / object class (shim) | The Vulkan and DXVK work `InteropTextures` does today, and nothing else. The image's IUnknown private-data hook tells `HostShares` which image retired when DXVK destroys the texture. |

These are deleted with no replacement shim:

- `D3dSharedResources`
- `InteropTextures`
- `HostSharedObjects` and `HostSharedObject` (device)
- `ContextSharedObjects` and `ContextSharedObjectSlot` (device)
- the 14 sharing methods on `D3d11HostDevice`
- the per-session sharing decisions in `GlSharedObjects`, `ClSharedObjects`
  and `HostExportedFds`

The handle bookkeeping a backend still needs (which GL name or `cl_mem` to
destroy) stays in the backend. Move it to a reserved table so that 64 is not
the limit, or keep it inside `ShareHolders`, which already records each
session's wire handle. **Prefer the latter**: it removes the per-session
sharing tables entirely. A backend's `destroy_object` is told the handle by
`HostShares`.

**Is the participant interface virtual?** Yes. The constitution allows
virtual dispatch chosen at runtime, and here there are four real
implementations (D3D, Vulkan, GL, CL) plus a test fake. Give it a protected
non-virtual destructor, and check `tools/check_design.py`.

### 4.3 `HostShares` API

| Method | Called by | Replaces | Behaviour |
|---|---|---|---|
| `publish(domain, id, kind, size, session, ShareParticipant&, wire_handle)` | ABI `share_object` | device `ContextSharedObjects::publish` + `HostSharedObjects::publish` + backend `export_object` | Records the share with the guest's hold and the publishing session's hold. The descriptor is exported from the participant now for Vulkan, or lazily for D3D (the first non-D3D open). An id already published in that domain is refused (`conflict`), as today. |
| `publish_texture(domain, id, desc, misc, session)` | `D3dBackend::on_CreateResource` | `shareable` + `make_shareable` + `D3dSharedResources::publish` | Makes the D3D11 texture: exportable when `misc` has `D3D11_RESOURCE_MISC_SHARED`, otherwise DXVK makes it. Marks it (`SurfaceIdentity`), records it, and returns the texture with the caller's reference. |
| `publish_d3d12(domain, id, ID3D12Resource*, session)` | `D3d12Presentation::share` | `shares` + `publish_shared` | One call. It takes the D3D11 lock itself, because D3D12 commands run unlocked. |
| `open(domain, id, kind, size, session, ShareParticipant&, &wire_handle)` | ABI `open_object` | device `ContextSharedObjects::open` + `HostSharedObjects::open` + backend `import_object` | Checks kind and size (the same hostile-input rules as `HostSharedObjects::open` today), makes the descriptor if it does not exist yet, has the participant import it, and records the session's hold with its wire handle. |
| `open_texture(domain, id, session)` | `D3dBackend::on_OpenResource` | `find_shown` → `find_shared` → `view_of` | The shown copy if any, else the D3D11 view (made on the first open for a D3D12 publish). It records the session's hold, so DWM's opens are counted too. |
| `close(domain, id, session)` | ABI `close_object`; `D3dBackend::on_DestroyObject` for a texture it opened | device `release` / `give_back` + backend `release_object` | Drops that session's hold, and has its participant destroy its handle. The publisher's session hold is dropped here too, but the guest's hold is not. |
| `session_closed(session)` | `ExecutorRoot` when a session is destroyed | device `release_all` | Drops every hold the session had, including a publishing session's. The guest's hold stays until 0x02F1. This settles §6 by construction. |
| `release(domain, id)` / `release(domain, 0)` | ABI `release_shared` | `HostSharedObjects::close`/`release_all` + `D3dSharedResources::release` | Drops the guest's hold (or every guest hold in the domain), then frees whatever no one holds, then runs `collect()`. |
| `show(domain, id, presented)` | `on_PresentPoint` (0) | `D3d11HostDevice::show` | Flip copy. |
| `written(domain, id)` / `writes(...)` | `counted`; the export socket | `D3dSharedResources::written`/`writes` | Copy count. |
| `export_image(domain, id, AvExportedImage&)` / `sync(...)` | ABI `export_surface` / `sync_surface` | `D3d11HostDevice::export_surface`/`sync_surface` | For the export socket. These take the D3D11 lock themselves. |
| `collect()` | `release`; `publish_texture` when retired images wait | `InteropTextures::collect` | Frees images whose texture DXVK destroyed, after the GPU is idle. |
| `report(StateWriter&)` | `ExecutorRoot::report` | — | Per domain: live, released and retired shares; holders; exportable bytes; per kind and origin API. |

A row is freed when it has no holders and no live host object: no session
reference, and DXVK has dropped its texture.

### 4.4 Domain: the guest device

A guest device instance (one `VirtioGpuAv`, one guest adapter) is a
**domain**. The device asks the executor for a domain when it realizes, and
gives it back when it unrealizes:

- ABI `open_domain(gpu_uuid) → domain` (a non-zero `uint32_t`).
- ABI `close_domain(domain)`, which releases every share in it.

Contexts are created with their domain in `AvContextOptions`, so the
executor knows each session's domain. Every device-level call names its
domain: `release_shared`, `export_surface`, `sync_surface`, `export_image`,
`read_image`, `map_frame` and `unmap_frame`. In av-renderer mode, the
renderer's `LocalContextPlane` opens the domain in the renderer process,
where the executor runs.

### 4.5 Storage: no fixed small capacity

`ShareTable` reserves room for `max_shares = 65536` rows, and their holder
records, with `mmap(MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE)`, as
`HandleRegistry` does:

- Pages are committed as rows are written, so an empty table costs nothing.
- The (domain, id) index is open-addressed. Do not scan linearly: DWM opens
  ids on every frame.
- Holders per share are unbounded: use a linked list through the reserved
  holder rows.
- Hitting `max_shares` is refused and logged once, with the count. 65536
  textures exhaust any GPU long before that.

### 4.6 Locks

`HostShares` has its own lock (a pthread mutex) around its rows, because
three kinds of caller reach it:

- every context's worker, through `share_object`, `open_object` and
  `close_object`;
- the main loop, through `release_shared`;
- any thread, through the export socket.

It never holds that lock across a host API call that may block, or across
taking the D3D11 lock. **Order: the D3D11 lock first, then the `HostShares`
lock.** Work that touches DXVK (`publish_texture`, `open_texture`, `show`,
`export_image`, `sync`, `collect`) runs under the D3D11 lock, which callers
already hold or which those methods take. Each such method takes the
`HostShares` lock only around row updates. A participant's
`export_object`/`import_object`/`destroy_object` runs with the `HostShares`
lock *released* and the row pinned by a "busy" state. Write that order into
the class comment, and test it.

### 4.7 The ABI (version 16)

**Added:**

- `open_domain` and `close_domain`
- `share_object(executor, object_id, kind, size, wire_handle)`
- `open_object(executor, object_id, kind, size, *wire_handle)`
- `close_object(executor, object_id)`

**Changed:**

- `release_shared`, `export_surface`, `sync_surface`, `export_image`,
  `read_image`, `map_frame` and `unmap_frame` take a domain where they took
  `gpu_uuid` (the domain implies the GPU).
- `AvContextOptions` carries the domain.

**Removed:** `export_object`, `import_object` and `release_object`.
`AV_EXECUTOR_ABI_VERSION` and `AV_EXECUTOR_ABI_VERSION_MINIMUM` both become
16. Delete every `abi_version >=` gate and fallback in
`src/device/execution/executor_library.cpp` and its callers. They are now
unreachable, and the project keeps no compatibility code.

The guest wire protocol does not change. `SharedObjectCall` share, open and
release, and the D3D wire's `CreateResource`/`OpenResource` fields, stay as
they are; only what the host does with them moves.

### 4.8 Visibility

- Every refusal logs once at the point of decision, with the value that
  caused it.
- When live shares in a domain cross a power of two (64, 128, 256 and so
  on), log one line with the counts per kind, the counts per origin API, and
  the exportable bytes. DWM's leak then shows up in QEMU's log as a rising
  line.

## 5. Steps

Do them in order. After each step:

```
cmake --build build -j"$(nproc)" && ctest --test-dir build --output-on-failure -j"$(nproc)"
```

The build directory is `build`, configured with
`cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++`, as
`tools/gate.sh` does. Each step leaves the tree green. If a step cannot be
made green on its own, fold it into the next one and say so in the
hand-back.

### Step 0: clean start and baseline

- The working tree has six uncommitted files that contain only the temporary
  `SHAREDTRACE` instrumentation from the investigation. Revert them:
  ```
  git checkout -- src/executor/d3d/d3d_backend.cpp src/executor/d3d/d3d_backend.hpp \
      src/executor/shim/d3d11_host_device.cpp src/executor/shim/d3d11_host_device.hpp \
      src/executor/shim/interop_textures.cpp src/executor/shim/interop_textures.hpp
  ```
  Then `git status --short | grep -v third_party` must show nothing.
- Build and test, and write down the baseline test count and any failures
  that already exist.
- Read `test_fullstack_shared_objects.cpp` and the unit tests of every class
  this plan deletes. Their claims, such as the hostile cases and "the object
  outlives the context that published it", must still be tested after the
  plan, against `HostShares`.

### Step 1: `ExportableImages` (shim)

- Move the Vulkan and DXVK work out of `InteropTextures` into
  `ExportableImages` and `ExportableImage`, unchanged in behaviour.
- The DXVK hook now reports which image retired to its owner.
- Drop the fixed capacity.
- `D3d11HostDevice` uses `ExportableImages`. Delete `InteropTextures`.

### Step 2: the sharing core, with unit tests

Write `ShareTable`, `ShareHolders`, `Share`, `ShareParticipant` and
`HostShares` against a fake participant and a fake images interface (follow
`tests/harness/executor/`), with tests under `tests/unit/executor/sharing/`.

Each test must judge an outcome. Cover at least:

- **Publish and open:**
  - publish, then an open from another session in another API, gives that
    session a handle, and the participant saw one export and one import;
  - an unknown id is refused;
  - a kind or size that disagrees is refused and the share is untouched;
  - a publish under a taken id in the same domain is refused, and the same
    id in another domain is not;
  - one session opening the same share twice is refused.
- **Capacity:** more than 1024 shares, and more than 256 per domain, succeed
  (the old limits). The table-full refusal logs once.
- **Holds and release:**
  - `close` drops only that session's hold;
  - `session_closed` drops every hold of that session, including the
    publisher's, but the share lives until the guest's `release`;
  - `release(domain, 0)` drops every guest hold in the domain only.
- **Freeing and collection:**
  - a share with no holders and no host object is freed;
  - a D3D texture share whose texture DXVK still holds stays retired until
    the hook fires, and then `collect()` frees it.
- **Lazy export:**
  - a D3D share's descriptor is exported on the first non-D3D open, and
    only once;
  - a D3D12 publish makes its D3D11 view on the first open, and only once.
- **Export socket:** `export_image` refuses a share with no exportable image.
- **`report`:** writes the counts per domain, kind and origin.
- **Locks:** the lock order is respected. A participant callback that
  re-enters `HostShares` (for example, an import that publishes) does not
  deadlock.

`tools/check_symbol_tests.py` requires every public symbol to be tested.

### Step 3: Direct3D onto `HostShares`

- `D3d11HostDevice`:
  - owns `ExportableImages`;
  - its scanout readback (`export_image(key,…)`, `read_image`, `stage`,
    `drain`) finds textures through `HostShares::open_texture`;
  - delete the 14 sharing methods and `D3dSharedResources`, with its test.
- `LoaderHostApis` owns `HostShares hostshares_[8]` next to the devices, and
  hands each to its D3D11 device and to `ExecutorRoot` through `HostApis`.
- `D3dBackend`:
  - `on_CreateResource` → `publish_texture`;
  - `on_OpenResource` → `open_texture`;
  - `on_DestroyObject` of an opened texture → `close`;
  - `counted` → `written`;
  - `on_PresentPoint` (0) → `show`;
  - `D3dBackend` implements `ShareParticipant` (export via
    `ExportableImages`; import still refused, now in one place).
- `D3d12Presentation::share` → `publish_d3d12`.

### Step 4: Vulkan, OpenGL and OpenCL onto `HostShares`

- `VulkanBackend`, `OpenGlBackend` and `OpenClBackend` implement
  `ShareParticipant` with the export and import code they have today.
- Delete their per-session sharing tables: `GlSharedObjects`,
  `ClSharedObjects`, and the sharing role of `HostExportedFds` (check what
  else uses it before you delete it) and `AdoptedBlobs`. Their wire handles
  live in `ShareHolders`.
- Keep `HandleRegistry`: it is the Vulkan handle table for everything, not
  just sharing.
- `ExecutorRoot` routes `share_object`, `open_object` and `close_object` to
  `HostShares`, with the session's backend as participant. On session
  destroy it calls `session_closed`.

### Step 5: ABI 16, and the device becomes routing only

- Change `src/device/abi/av_executor.h` as in §4.7 and update
  `executor_ops.cpp`, `ExecutorLibrary` and the executor test doubles.
- Device:
  - `SharedObjectCommand::run` calls `share_object`, `open_object` or
    `close_object` on the context's executor.
  - Delete `ContextSharedObjects` and `HostSharedObjects` with their tests.
    Move their test claims to `HostShares` (Step 2) and to the fullstack
    test (Step 6).
  - `LocalContextPlane::shared_released` → ABI `release_shared(domain, id)`.
  - Reset → `release_shared(domain, 0)`.
  - `ExportService::revoke` is unchanged.
- The domain:
  - `VirtioGpuAv` realize → `open_domain`;
  - unrealize → `close_domain`;
  - contexts get it in their options;
  - `ExportedGpu` uses it.
- Renderer mode:
  - `RendererService` opens the domain in its own `LocalContextPlane`;
  - `SharedReleasedMessage` and the export-socket path carry the domain;
  - read `docs/renderer.md` and its tests (`tests/.../renderer*`) for the
    messages that must change.
- Raise the minimum ABI to 16, and delete every version gate and fallback
  that became unreachable.

### Step 6: fullstack tests

- Update `test_fullstack_shared_objects.cpp` (and any other fullstack test
  of 053) to the new path. Its claims must hold end to end:
  - the miniport mints the id;
  - the creator publishes;
  - another context opens one host object;
  - the publishing context is torn down while the object is still open;
  - the hostile cases are answered with a status, with the object untouched.
- Add one D3D11-to-Vulkan or D3D11-to-GL fullstack claim if the harness
  allows it: a texture published by D3D11 and opened by Vulkan/GL reaches
  one exportable image, and the export happens once.
- Add one claim for the export socket path: `export_surface` of a published
  D3D11 shared texture gives a dma-buf.

### Step 7: guest side, with no fixed caps

- Miniport `SharedObjectTable`: replace `released_[256]` and
  `released_dropped_` with an unbounded queue, using the `Platform` and
  `ObjectPool` it already has. A dropped id today means the host holds that
  object until restart.
- Guest user mode: `SharedHandles` (64, and `max_private` 128),
  `OpenedObjects` (64), Vulkan `ExternalObjects` (64), and
  `GlExternalObjects`. Back them with `ObjectPool` and the ICD's `Platform`,
  as `src/icd/d3d/device_table.hpp` does.
- Keep each API's error mapping for real allocation failure
  (`VK_ERROR_OUT_OF_HOST_MEMORY` and so on), but remove the "too many"
  refusals.
- Cross-build the guest components the way `docs/testing.md` says, and run
  their unit tests.

### Step 8: documentation

- `docs/architecture.md`: add `src/executor/sharing/`; remove the deleted
  device classes; update the layer description (the device routes sharing,
  the executor owns it) and any "paths the build and the gates name".
- `docs/shared-surfaces.md`: rewrite around `HostShares`:
  - the lifetime diagram;
  - the domain;
  - the ownership table;
  - fix §1.2;
  - remove `SnapshotPool`;
  - close the per-GPU/per-adapter issue in §2.1.
- `docs/external-export.md`: the chain now ends in `HostShares`, with the
  domain.
- `docs/renderer.md`: the domain in the renderer process.
- `docs/trace.md`: the new state-dump keys.
- Any spec or feature doc for 053 under `docs/` that describes
  `HostSharedObjects` (grep for it).

### Step 9: gates

- Run `tools/scope_marks.py --apply`, then the `scope_marks` ctest.
- Run `tools/check_design.py`, `tools/check_symbol_tests.py` and
  `tools/check_self_contained.py`.
- Run `tools/gate.sh`: default, coverage, fuzz and qemu builds. It is slow,
  so start it in the background and wait for it.
- `src/executor/sharing/` is under the 100% branch-coverage gate; `shim/` is
  not.
- The fuzz targets that decode shared-object commands must still build and
  run clean (`tests/fuzz/`; grep for `shared`).

### Step 10: on the real system (LSW)

The LSW repository (`/home/accel/Coding/linux-subsystem-windows`) builds
this one as `external/accel-virt`.

1. Rebuild QEMU, the executor and the guest drivers:
   ```
   cmake --build /home/accel/Coding/linux-subsystem-windows/build --target accel_virt_host accel_virt_drivers -j"$(nproc)"
   ```
2. Install the new guest driver package in the VM. The guest UMD, ICDs and
   miniport all changed, and the device and guest must match. LSW has
   `scripts/upgrade-driver.sh` for this; read it first. Do not use
   `deploy-umd.sh`: its DriverStore path is stale.
3. Restart the VM. The user has allowed this for `lsw`. Never touch the
   `win11` VM.
   ```
   virsh -c qemu:///session shutdown lsw
   # poll `virsh -c qemu:///session domstate lsw` with an until-loop until "shut off";
   # after ~3 minutes still up:  virsh -c qemu:///session destroy lsw
   virsh -c qemu:///session start lsw
   ```
   Wait until the guest agent answers (§7).
4. **Desktop:** the guest boots to a working desktop (DWM composes through
   `HostShares`). QEMU's log has no refusals.
5. **Mirror cycle**, against Character Map. Notepad on Windows 11 is a
   store-app stub that exits at once.
   - Launch it: `agent.py launch '{"app_id":"C:\\Windows\\System32\\charmap.exe"}'`.
   - Start a host mirror:
     ```
     RWIN_LOG=info /home/accel/Coding/linux-subsystem-windows/build/remotewin/source/host/remotewin \
         --port 48730 --export-socket /run/user/1000/lsw-export.sock --pid <pid> --app-id lsw-test
     ```
     Run it in the background with its output in a file, then
     `agent.py start_window_mirror '{"port":48730,"pid":<pid>}'`.
   - The mirror log shows `imported surface` and never
     `frames come as pixels`.
   - Stop that mirror: `kill` the pid you found with `pgrep -x remotewin`
     plus a `/proc/<pid>/cmdline` check for `lsw-test`. Never use `pkill -f`.
6. **State dump:**
   `printf 'state\n' | socat - UNIX-CONNECT:/dev/shm/av-state-executor-<qemu pid>.sock`,
   or use Python's `socket.AF_UNIX`. After the cycle:
   - the ring surfaces are gone;
   - the three capture buffers remain, held by the guest. That is DWM's leak
     and expected;
   - released shares are collected.
7. **Stress:** 20 mirror cycles in a row. Live D3D shares rise by about 3
   per cycle, the log lines from §4.8 appear, and no mirror falls back to
   pixels.
8. **Other APIs.** Run whatever exercises cross-API sharing in the guest:
   - the `samples/clgl` sample;
   - an OpenGL or Vulkan window presenting through a D3D11 flip chain
     (`DxgiWindowChains`): for example a GL/VK sample from `samples/` in a
     mirrored window;
   - the guest suite for feature 053 if `docs/testing.md` names one.

   Each must work, and the state dump must show the objects under the right
   origin API with their holders.
9. Optional: restart DWM in the guest (§7) and check that the held capture
   buffers are released and collected.

### Step 11: hand back

Do not commit unless the user says so. When they do:

- Commit in this repository, one commit per step or per coherent group,
  with messages in the style of `git log`: lowercase, descriptive prose, a
  subject and a body that says why.
- End every message with:
  `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`
- The LSW repository then needs a submodule bump commit.

Report to the user:

- what changed per layer;
- what was deleted;
- the test counts before and after;
- the gate result;
- the numbers from Step 10;
- anything in this plan the code made you change.

## 6. Known defect this settles

`ContextSharedObjects::release_all`
(`src/device/execution/context_shared_objects.cpp:20-30`) closes every slot
on `HostSharedObjects` when a context is torn down, including slots that
context *published*. `give_back`, by contrast, keeps the publisher's hold,
because the guest's 0x02F1 drops it.

So a publishing context's teardown may drop one holder too many, and a later
0x02F1 then drops an opener's hold. `test_context_shared_objects.cpp:103`
looks consistent with this.

The new model separates the guest's hold from each session's hold, so it
cannot happen. Write the Step 2 test that proves it: publish from A, open
from B, tear down A, release from the guest, and B still holds until B
closes.

## 7. Done when

- `HostShares` is the only place on the host that knows a shared object's
  existence, kind, descriptor and holders. Every API publishes, opens and
  closes through it.
- `D3dSharedResources`, `InteropTextures`, `HostSharedObjects`,
  `ContextSharedObjects`, `GlSharedObjects`, `ClSharedObjects`, the 14 D3D11
  sharing methods, and ABI calls `export_object`/`import_object`/
  `release_object` are gone. So are all ABI gates below 16.
- No fixed small capacity remains anywhere on the path, host or guest.
- Shares are scoped by domain.
- `tools/gate.sh` passes, with 100% branch coverage of `sharing/`.
- On the real VM: the desktop works; 20 mirror cycles never fall back to
  pixels; the cross-API samples work; the state dump shows every share with
  its origin and holders.
- Docs are updated, and no `SHAREDTRACE` remains.

## 8. Working on this machine

**Shell traps:**

- The shell is zsh. A command stored in a variable (`V="virsh -c ..."; $V`)
  does not word-split. Write commands out in full.
- Never `pkill -f <pattern>`: it kills your own shell. Use `pgrep -x <name>`
  plus a check of `/proc/<pid>/cmdline`.
- A foreground `sleep` longer than a few seconds is blocked. Wait with an
  until-loop on a condition, or run the command in the background.
- zsh's `echo` interprets backslash escapes, which mangles Windows paths
  such as `\uploads`. Use `printf '%s\n'`.

**Reaching the guest:**

- The guest agent listens on `127.0.0.1:5102`. The LSW manager's MCP server
  (`127.0.0.1:5105`) only runs while the manager does, so talk to the agent
  directly.
- Protocol: frames are `[type u8 = 1][len u32 big-endian][JSON]`. A request
  is `{"id":N,"cmd":"...","params":{...}}`, and the reply carries the same
  `id`. Events (`{"event":...}`) arrive on the same connection; skip them.
- `run_script {"script": "..."}` runs the text as a `.cmd` batch file as
  SYSTEM in session 0. It returns a job id, not output.
- `console_open {"session":0}` returns `session_id`. Wait about 1.5 s, then
  `console_input {"session_id":..., "data":"<line>\r\n"}`.
  - Output arrives as `console_output` events, with base64 bytes in
    `data_b64`.
  - Close the console with `console_close`.
- To copy a file in: `upload_file` with `action` `begin` (`name`, `size`),
  then `chunk` (`upload_id`, `offset`, `data` base64, at most 192 KiB), then
  `finish`. Finish returns the guest path, under
  `C:\ProgramData\LSW\uploads\<id>\`.
- There is no process-list command. Run a batch file that writes one empty
  file per process (`tasklist /fo csv /nh`), and read the names back with
  `list_dir`.
- To dump the guest driver's state, upload
  `/home/accel/Coding/linux-subsystem-windows/build/accel-virt/drivers/guest/avinspect.exe`,
  then run `avinspect.exe kernel-state --out C:\ProgramData\LSW\ks.snap` and
  `type` the file in a console. The dump has
  `adapter/0/shared_objects/0/object/<id> ... opens=N` and
  `device_table/0/device/<n>/open/<m> allocation=<handle>` lines.
- To restart DWM (Windows restarts it by itself; the screen blinks once):
  `run_script {"script":"taskkill /f /im dwm.exe"}`.

**Small client.** Save this as `agent.py` outside the repository:

```python
#!/usr/bin/env python3
"""agent.py <cmd> [params-json]: one request to the guest agent; prints the result."""
import json, socket, struct, sys
s = socket.create_connection(("127.0.0.1", 5102), timeout=120)
body = json.dumps({"id": 1, "cmd": sys.argv[1],
                   "params": json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}}).encode()
s.sendall(struct.pack(">BI", 1, len(body)) + body)
buf = b""
while True:
    while len(buf) < 5: buf += s.recv(65536)
    n = struct.unpack(">I", buf[1:5])[0]
    while len(buf) < 5 + n: buf += s.recv(65536)
    msg = json.loads(buf[5:5 + n]); buf = buf[5 + n:]
    if msg.get("id") == 1:
        print(json.dumps(msg.get("result", msg), indent=2)); break
```

**QEMU's log** is `~/.cache/libvirt/qemu/log/lsw.log`. It rotates at 2 MB,
so search `lsw.log*`. A VM run starts at the last `starting up` line.
Executor lines are prefixed `virtio-gpu-av:`.
