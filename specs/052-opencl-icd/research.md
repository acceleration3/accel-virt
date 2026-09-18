# Research: OpenCL - the ICD on rvkgpu

Sources: the Khronos OpenCL ICD loader source, tag `v2026.05.29`
(`OpenCL-ICD-Loader-2026.05.29`, SHA-256
`48fd0c5181db7cd046f4f731d5955694892e10998d49d09ee0d997e7e04fd939`), which this
feature pins and builds; `third_party/khronos/include/CL/` from
`tools/fetch_khronos_registries.py`; the WDK's `d3dkmthk.h`; and this
repository's generated OpenCL tree, device, executor and two existing ICDs.
Line numbers are the loader's unless named.

## 1. How the Khronos loader finds and accepts an ICD

**Decision.** The INF writes, in each rvkgpu adapter's software key:

| Value | Type | Data |
|---|---|---|
| `OpenCLDriverName` | `REG_SZ` | `%13%\rvkcl.dll` |

`REG_SZ`, not the `REG_MULTI_SZ` that `VulkanDriverName` uses, and nothing under
`HKLM\SOFTWARE\Khronos\OpenCL\Vendors`.

**Why `REG_SZ`.** The loader has three discovery paths, tried in order by
`khrIcdOsVendorsEnumerate` (`loader/windows/icd_windows.c:186`):

1. `khrIcdOsVendorsEnumerateDXGK` (`icd_windows_dxgk.c:32`) - `gdi32.dll` is
   loaded, `D3DKMTSubmitPresentBltToHwQueue`'s presence is the OS check, then
   `D3DKMTEnumAdapters2` lists every adapter and each is asked
   `D3DKMTQueryAdapterInfo` with a registry query: `LOADER_QUERY_REGISTRY_ADAPTER_KEY`,
   `translate_path = TRUE`, **`value_type = REG_SZ`**, `value_name` =
   `OpenCLDriverName`. A value of another type is not returned by this path.
2. `khrIcdOsVendorsEnumerateHKR` (`icd_windows_hkr.c:206`) - SetupAPI over the
   display and software-component device classes, `CM_Open_DevNode_Key(...,
   CM_REGISTRY_SOFTWARE)`, then `RegQueryValueExA` of the same value name. This
   path accepts `REG_SZ` and tolerates `REG_MULTI_SZ` (`icd_windows_hkr.c:131`,
   "Accepting multi-string registry key type").
3. `khrIcdOsVendorsEnumerateAppPackage`, then the global vendors key
   `SOFTWARE\Khronos\OpenCL\Vendors` (`icd_windows.c:194`), neither of which
   this driver uses.

So `REG_MULTI_SZ` would be found by the second path alone, and silently missed
by the first. `REG_SZ` is found by both. `translate_path` is what makes the
`%13%` driver-store path resolve.

The 32-bit value name is `OpenCLDriverNameWow`, chosen at runtime by
`getOpenCLRegKeyName` (`icd_windows_hkr.c:55`) from `IsWow64Process`; feature
061 writes it.

**What the loader requires of the DLL** (`khrIcdVendorAdd`, `loader/icd.c:70`):

1. Exactly one symbol is resolved by name: **`clGetExtensionFunctionAddress`**
   (`icd.c:111`). Nothing else is looked up by export name.
2. `clIcdGetPlatformIDsKHR` is obtained *through* it (`icd.c:119`); the ICD is
   refused if that answers null. `clIcdGetFunctionAddressForPlatformKHR` and
   `clIcdSetPlatformDispatchDataKHR` are asked for too (`icd.c:128-129`) and are
   the `cl_khr_icd2` path; a platform is only held to them if it carries the
   ICD 2 tag, so an ICD 1 platform needs neither.
3. `clIcdGetPlatformIDs(0, NULL, &count)` then the fill call (`icd.c:132-150`).
4. A platform whose **first member `dispatch` is null is skipped**
   (`icd.c:162`). Every object this ICD hands out is therefore a struct whose
   first member is `cl_icd_dispatch *`.
5. `clGetPlatformInfo(CL_PLATFORM_EXTENSIONS, ...)` must succeed twice, size
   then data (`icd.c:204`); the string is searched for `cl_khr_icd_unloadable`.
6. `clGetPlatformInfo(CL_PLATFORM_ICD_SUFFIX_KHR, ...)` must succeed twice
   (`icd.c:261`), **or the platform is dropped**. This is the one query an ICD
   cannot decline.

**Consequence for this feature.** `rvkcl.dll` exports one function,
`clGetExtensionFunctionAddress`. `CL_PLATFORM_ICD_SUFFIX_KHR` answers `RVK` and
`CL_PLATFORM_EXTENSIONS` is answered before any device exists - both from the
platform alone, without opening an adapter, because the loader asks them while
enumerating and an ICD that opened a device here would open one in every process
that ever calls `clGetPlatformIDs`.

**Alternatives considered.** Exporting the whole API surface (what a
non-ICD OpenCL implementation does): rejected, the loader never looks the names
up and the exports would only invite direct linking that bypasses the loader.
`cl_khr_icd2` loader-managed dispatch: rejected for this feature, it is gated on
`CL_ENABLE_LOADER_MANAGED_DISPATCH` in the loader's own build and adds two entry
points and a tag for no behaviour this feature needs; the ICD 1 path is what
every shipping loader carries.

## 2. Where the loader in the guest comes from

**Decision.** `tools/fetch_opencl_loader.py` with `tools/opencl_loader_pins.txt`
fetches the loader's **source** archive by pinned URL and SHA-256, and the build
cross-compiles `OpenCL.dll` for the guest with the same `clang-cl` toolchain
that builds `rvkgpu.sys` and the ICDs. The runner pushes it beside
`rvkclprobe.exe`. Nothing is committed and the driver package ships no loader.

**Why not a pinned binary.** The Vulkan precedent
(`tools/vulkan_runtime_pins.txt`) pins LunarG's prebuilt `vulkan-1.dll`, and the
first choice here was the same shape. Khronos publishes no prebuilt Windows
binary for the ICD loader: the GitHub releases for
`KhronosGroup/OpenCL-ICD-Loader` (checked at `v2026.05.29`, `v2025.07.22`,
`v2024.10.24`) carry **no assets at all**, only the auto-generated source
archives. So there is no binary to pin, and the honest reading of "fetch by
pinned hash" is to pin the source and build it.

**What building it costs.** The Windows loader is C: `loader/icd.c`,
`icd_dispatch.c`, `icd_dispatch_generated.c`, `icd_trace.c` and
`loader/windows/{icd_windows,icd_windows_hkr,icd_windows_dxgk,icd_windows_envvars,icd_windows_apppackage,icd_windows_library}.c`,
exporting through `loader/windows/OpenCL.def`. Its headers come from the CL
headers this repository already fetches, and its DXGK path needs only
`gdi32.dll` at runtime. It is built as a test artifact for the guest image, not
as part of `rvkgpu`'s package, so it is outside the driver-image checks.

**Alternatives considered.** Shipping a loader in the package: rejected by
FR-001 and by the constitution's dependency rule. Assuming the guest already has
one: rejected, a clean Windows image has no `OpenCL.dll` and the L5 run would
silently test nothing.

## 3. What the ICD is, in this repository's shape

**Decision.** `rvkcl.dll` follows `rvkvk.dll` and `rvkgl.dll` exactly: a
`constinit ClIcdLibrary` in `src/um/opencl/rvkcl_exports.cpp` holding an
`ObjectStorage<Seams>`; the seams are `ProcessHeap`, two `SrwLock`s, `FileLog`,
`GdiKmt` and `ProcessTrace`, already written and shared; the portable
`ClIcdRoot` in `src/icd/opencl/` takes them as abstract references and binds
itself to `ClIcdBinding` in its constructor. The named DLL entry point returns
`TRUE` and does nothing, because the loader lock forbids opening a file or an
event at attach.

**One process-wide dispatch table.** The ICD ABI is positional: `cl_icd_dispatch`
(`third_party/khronos/include/CL/cl_icd.h:38-309`) is a flat array of 149 active
slots at `CL_TARGET_OPENCL_VERSION 300` with no version field, no count and no
tag - position is the whole ABI. A version-gated slot degrades to `void *`
rather than disappearing, so offsets never move. Every object this ICD hands
out - the nine `_cl_*` structs, sub-buffers, sub-devices, cloned kernels, user
events and `clCreateKernelsInProgram`'s out-array included - is a struct whose
first member is a `cl_icd_dispatch *` pointing at that one table. A null first
word is a jump into nothing inside the loader, with no diagnostic.

`av::clproto::fill_unimplemented(table)`
(`src/proto/generated/opencl/cl_icd_stubs.h:548`) fills every slot the ICD does
not claim: a named refusal where the slot's type matches, a type-derived
`Refuse<>::call` where `cl.xml` and `cl_icd.h` disagree about a parameter, and a
cast where the header declares the slot `void *`. The discipline is: zero the
table, write the ICD's own entries, call `fill_unimplemented` last - it only
writes null slots. On a Windows build the 16 D3D/DX9 slots are typed and get
refusals; on Linux they are `void *` and stay null, so a host-side test must not
assert "no slot is null".

**Three gaps this feature closes.**

1. `av::icd::stub_called(api, entry_point, why)` is called once per refusal by
   `cl_icd_stubs.h:21` and declared nowhere: `src/proto/icd_stub_report.hpp`
   declares only `payload_dropped`. **`cl_icd_stubs.h` has never been
   compiled.** 052 adds the declaration beside `payload_dropped` and defines it
   in the OpenCL ICD's hooks, one line into the ICD's log, exactly as
   `src/icd/opengl/gl_hooks.cpp:237` defines `payload_dropped`. The generated
   OpenCL client calls `payload_dropped` nowhere, so the ICD needs only the one.
2. Neither existing ICD serialises its own construction, because each is handed
   one for free: the Vulkan loader holds `loader_lock` while it scans drivers,
   and `opengl32` calls `DrvValidateVersion` first under its own lock. **The
   Khronos OpenCL loader gives no such guarantee** - `clIcdGetPlatformIDsKHR`
   may be called on any thread with nothing held - so `ClIcdLibrary` needs its
   own one-shot around `construct()`.
3. `icd::ServiceThread` does not exist; `src/icd/common/` has `ExtensionPolicy`,
   `FramePacer`, `LockedTransport`, `LogSink`, `SlicedWait`, `ThreadLock`,
   `WindowSurface` and nothing that runs a thread. 052 adds it, with the
   Windows implementation in `src/um/icd/` and a stepped one in the harness.

**ICD version 1, not 2.** `cl.xml` carries `clIcdGetFunctionAddressForPlatformKHR`,
`clIcdSetPlatformDispatchDataKHR` and `CL_ICD2_TAG_KHR`, and the loader will use
them when a platform carries the tag (`loader/icd.c:167-201`), but the pinned
`OpenCL-Headers` release has none of the three and `cl_icd_dispatch` has no
dispatch-data member. The header this ICD compiles against cannot express ICD 2,
and the loader's ICD 2 path is gated on its own build's
`CL_ENABLE_LOADER_MANAGED_DISPATCH`. The generated `fill_unimplemented` already
straddles this with `if constexpr (requires { table.clIcd... })`, which compiles
to nothing here.

**Exports.** `rvkcl.def` exports `clGetExtensionFunctionAddress` and
`clIcdGetPlatformIDsKHR`. The pinned loader resolves only the first by name
(§1), but older loaders looked the second up directly and it costs one line to
be found by both. `--only-exports` on the image gate then holds the surface to
exactly those two.

**Alternatives considered.** Exporting the whole API: rejected, see §1. One root
shared with the Vulkan or OpenGL ICD: rejected for the reason 050 gave - it
would make each DLL link the other's world.

## 4. How it is built, registered and proven

**The build.** Host side, mirroring 050's pair (`CMakeLists.txt:289-310`): an
`av_cl_generated` OBJECT library over
`src/proto/generated/opencl/cl_dispatch_client.cpp` compiled against the ICD's
own hook declarations, and an `av_cl_icd` static library over
`src/icd/common/*.cpp src/icd/opencl/*.cpp` plus those objects. The per-test
link dispatch (`CMakeLists.txt:342-346` and again at `:630-636`) gains a
`/tests/unit/icd/opencl/` branch **before** the `/tests/unit/icd/` catch-all,
because an ICD's tests link its core and no other test may. The host build does
not currently put `third_party/khronos/include` on any include path - the guest
build does - so `av_cl_icd` adds it.

Guest side, mirroring `rvkgl` (`src/um/CMakeLists.txt:128-158`): a `rvkcl`
SHARED target over the same ICD sources plus `src/guest/{kmt,transport}`,
`src/proto`, `src/core/{support,trace}`, `src/um/{trace,icd,opencl}`, with
`PREFIX ""`, `/ENTRY:rvkcl_entry`, `/DEF:.../opencl/rvkcl.def`, and
`list(REMOVE_ITEM ... ${RVKTEST_GENERATED})` so the recursive `../proto/*.cpp`
glob does not sweep `cl_dispatch_server.cpp` into a guest DLL. Imports are
gdi32, kernel32 and ntdll only - no user32, because this ICD makes no window -
and `src/um/CMakeLists.txt:20`'s exclude regex gains `opencl` so the exports do
not leak into `rvkguest`. `rvkcl.dll` and `rvkclprobe.exe` join `BUILD_BYPRODUCTS`
at `CMakeLists.txt:441-443`.

**The registration.** One line in `[Rvkgpu_SoftwareSettings]`, plus
`rvkcl.dll` in `[SourceDisksFiles]` and `[Rvkgpu_Files]`:

```
HKR,, OpenCLDriverName, 0x00000000, "%13%\rvkcl.dll"
```

`0x00000000` is `FLG_ADDREG_TYPE_SZ`. This is the one place 052 deliberately
departs from the two lines above it: `VulkanDriverName` and `OpenGLDriverName`
are written `0x00010000` (`REG_MULTI_SZ`) because their readers take either, and
§1 shows the OpenCL loader's primary path asks `D3DKMTQueryAdapterInfo` for a
`REG_SZ` and its fallback path is the slower SetupAPI walk. Written multi-string,
the ICD would be found on one path and missed on the other.

`tests/tools/test_driver_package.py` couples the INF and the runner: its
`PACKAGE_FILES` tuple and its assertion that the runner's staged set equals the
INF's named set both have to learn `rvkcl.dll`, and an `OpenCLDriverName` test
joins the `OpenGL*` one at `:86-105`.

**The image gates.** `tools/check_guest_image.py` needs no change - its
allowlist is argument-driven - only two new registrations:
`rvkcl_image` (`--dll --only-exports`, the two required exports, no
`--allow-import`) and `rvkclprobe_image` (`--allow-import OpenCL.dll`,
`--require-import clGetPlatformIDs`).

**The probe.** `rvkclprobe.exe` splits like `rvkglprobe.exe`: the decisions in
`src/guest/suite/cl_probe.*` over a pure-virtual `ClEntry` seam, and translation
only in `src/um/probe/cl/`. This is a different thing from 044's
`OpenClScenario`, which drives the generated client straight over the transport;
`ClProbe` goes through the Khronos loader, which is the whole point of SC-001.
The seam has three implementations, as `GlEntry` does: the system `OpenCL.dll`
in the guest, a fake loader on the full-stack rig, and a scripted one in unit
tests. A new `src/um/imports/OpenCL.def` gives the probe its import library.

**The runner.** `--opencl` is the `--opengl` shape: a constants block
(`CL_SUMMARY`, `CL_STEP`, `CL_NOTE`, `CL_STEPS`), four arguments, a
`prepare` check that the pinned loader is on disk (reusing
`fetch_opencl_loader.py`'s own `pins_from`/`problems_on_disk`, as
`vulkan_runtime()` reuses the Vulkan one), two `push` entries plus `rvkcl.dll`
in the package directory, one entry in the `real_executor` scenario tuple, and
an `opencl()` method that runs the probe through `cmd /c ... > opencl.txt 2>&1`
and pulls the file. `tools/gate.sh:74-78`'s `--vulkan` pattern - append the mode
only when the pinned artifact checks out - is copied for `--opencl`.
`tests/tools/fake_guest/fake_qemu.py`'s `PROGRAMS` tuple must learn
`rvkclprobe`, or the fake guest will not recognise it.


## 5. What the wire extension actually costs

The clarification's answer was "extend the wire", and the shape of that turned
out to be much larger than adding ten names to `tools/codegen/profiles/opencl.toml`.
Lowering all ten against the untouched inference: **two lower and emit
non-compiling C++, eight raise before emission, and one of the two that lowers
is semantically dead.** None of the ten works today.

### 5.1 The `void *` return - `clEnqueueMapBuffer`, `clSVMAlloc`

No command on the wire returns a raw pointer: all 105 return `cl_int`, `void`,
or a `cl_*` handle. `Command.returns_handle` (`opencl/model.py:88-93`) is
"non-pointer, `cl_`-prefixed, not an integer type", and `returns_void` is
`void` at depth 0, so a `void *` return is **neither**, and the path has never
been exercised. Four places emit ill-formed C++ for it:
`_client_failure` returns `CL_OUT_OF_RESOURCES` from a `void *` function
(`emit.py:664-672`); `_decode_reply` casts an `int64_t` to `void *` through
`static_cast` (`:694-698`); the server writes `static_cast<uint64_t>(static_cast<int64_t>(ret))`
over a pointer (`:1198-1200`) and declares the local `const void *ret`
(`:1186`); and `_succeeded` answers `"true"` for any pointer return
(`:474-480`), so a null map would be reported as success.

**Decision.** A third return classification beside `returns_handle` and
`returns_void` - an **address return** - encoded as a `uint64_t` like a handle,
with `_succeeded` as `ret != nullptr`, `_client_failure` returning `nullptr`,
and a `_default_body` arm.

### 5.2 The SVM pointer - seven commands

Inference sends every non-const `void *` to `BYTES_OUT` and every const one to
`BYTES_IN` (`lower.py:91`), and `_needs_len` then raises for all seven
(`lower.py:140-143`). No existing role is right: `BYTES_*` would copy a payload
where there is none (`clSVMFree`, `clSetKernelArgSVMPointer`,
`clEnqueueSVMUnmap`) or copy the wrong bytes where the address must name a
*host* allocation (`clEnqueueSVMMap`, `Memcpy`, `MemFill`); `SCALAR` emits
`void svm_ptr{}` and a cast to `void`; and `HANDLE` resolves exact object
identity, which is wrong for the two cases that matter - `clSetKernelArgSVMPointer`
takes an address *inside* an allocation, and `clEnqueueSVMMemcpy`/`MemFill` take
an address whose extent must lie inside one live allocation.

**Decision.** A new `Role.SVM_POINTER`: `uint64_t` on the client, encoded
`write_pod<uint64_t>`; `void *` on the server, resolved through a new
`ServerDispatch` virtual beside `handle_for` - `void *svm_for(uint64_t address,
uint64_t bytes)` - which answers null unless `[address, address + bytes)` lies
inside one live SVM allocation the guest made. The profile spells it
`role = "svm_pointer"` with `len` naming the size parameter, so the refusal is
generated rather than written by hand. Exactly one parameter of the ten needs
nothing new: `clEnqueueSVMMemFill.pattern` is `bytes_in` with
`len = "pattern_size"`, the shape `clEnqueueFillBuffer.pattern` already uses.

This also retires a refusal: `EXEC_INFO_POINTERS` (`emit.py:114-116`, tested at
`tests/unit/device/executor/test_opencl_dispatch.cpp:363-366`) refuses
`CL_KERNEL_EXEC_INFO_SVM_PTRS` because "this wire carries no allocation of
either", which stops being true.

### 5.3 The callback - `clSetEventCallback`

`parse.py:102-108` marks a parameter with no `<type>` child as a callback, and
`lower._infer` (`:47-50`) makes both `pfn_notify` and the `user_data` after it
`Role.IGNORED`. So the command lowers and compiles - and the server calls
`clSetEventCallback(event, type, nullptr, nullptr)`, which the API refuses with
`CL_INVALID_VALUE`, because unlike `clCreateContext` a null notify is not legal
here. The generated path cannot express this command.

**Decision.** The host side synthesises its own trampoline: the server registers
a callback of its own with the host's OpenCL and the guest is told through the
completion channel (§6), so what crosses the wire is "tell me when this event
reaches this status", not a guest function pointer. This is a per-command server
hook, the kind `with_kernel_arg_info` already is.

### 5.4 Ids, gates and what must be committed together

`Profile.command_id` (`profile.py:22-28`) reuses an id already in
`opencl_ids.toml` and gives a new command `max + 1`; ids never move and are
never reclaimed (`opencl_ids.toml:111` still holds a command no longer carried).
The next free id is **131178 (`0x2006A`)**, so the ten take `0x2006A-0x20073` in
the order they are added to the profile's list.

The gate that catches a forgotten regeneration is `generated_sources`
(`tools/check_generated.py`, `CMakeLists.txt:543`), which re-emits into a
temporary directory and compares the committed tree byte for byte.
`protocol_ids` (`tools/check_protocol_ids.py`) only checks range and uniqueness,
so a stale `opencl_ids.toml` can pass it while leaving the tree one
regeneration away from an id shift - the regenerated sources and the ids file
are one commit, and `docs/protocol.md` moves with them.

`cl_icd_stubs.h` does **not** change: `emit_icd_stubs` is driven by the raw
registry, not the profile, so every one of the ten already has a stub there.
`HandleKind` does not change either; all five object types these commands name
are already carried.

### 5.5 Where the emitter is tested

There is no OpenCL emitter test file; the emitter is reached by
`tests/tools/test_client_call_scope.py:78-115` and
`tests/tools/test_generated_trace_scopes.py:96-121`, both of which build a
`Registry` in memory, emit into a temporary directory and assert on the text.
That is the shape for the new role's tests, plus a cheaper `lower()`-level test
that asserts the inferred roles with no emission at all.

## 6. The completion channel: what the device cannot do today

The clarification's answer was "the host pushes completion and a guest thread
wakes on the fence". Reading the device, **no part of that exists**, and the
reason is a rule worth keeping rather than a gap worth patching over.

### 6.1 How a fence reaches the guest now

`RingConsumer::consume_next` (`src/device/ring/ring_consumer.cpp:118`) calls
`FenceReporter::complete`, which sets `completed_`, bumps `progress_`, sets
`pending_` and republishes the 32-byte page into the guest's blob
(`src/device/fence/fence_reporter.cpp:44-53`, `fence_window.cpp:18-24`). Once
per service pass `RingConsumer::service` calls `FenceReporter::signal`
(`:75-82`), which raises the context's line through the device-wide
`FenceInterrupts` (`fence_interrupts.cpp:55-66`) and out to the fence queue's
guest notifier. `BatchExecutor::run_command` also calls `heartbeat()` after every
command, which moves `progress_` **without** setting `pending_` - so a heartbeat
never interrupts. That asymmetry is the hook this feature needs.

### 6.2 Why the obvious routes are closed

- **Every fence-page mutation is worker-only.** `complete`, `heartbeat`, `fault`
  and `signal` each open with `host_.require_worker(...)`; `MockHost` records a
  `worker_required` violation and unclaimed violations fail the test binary at
  its destructor. A host OpenCL callback runs on the host driver's own thread,
  which is neither the worker nor the main loop.
- **The worker queue is main-loop-only.** `Worker::post` is
  `require_main_loop`, so a foreign thread cannot enqueue work.
- **Nothing the callback would touch is locked.** `ClHandleRegistry` has no
  lock, `ContextSession`'s 64 MiB `DecodeArena` is reset at the head of every
  `execute`, and `FenceWindow::publish` is a plain copy into guest memory that
  two writers would tear - which the guest's `FenceObserver` would then fail the
  context over.
- **The executor ABI has no way back.** Every field of `RvkExecutorOps` is
  device to library; there is no callback pointer and no opaque flowing the
  other way.

### 6.3 What the guest will accept

`FenceProgress::observe` (`src/proto/submission/fence_progress.cpp`) fails a
context for good on a page whose `completed_fence` or `progress` went backwards,
on a `completed_fence` past what the guest submitted while the driver submits,
and on a fault that un-faults. It does **not** object to `progress` moving on its
own. So the legal shape of an out-of-band signal is: **progress moves,
`completed_fence` does not, and the interrupt is raised.**

### 6.4 The four seams

1. **Executor ABI version 5**, `RVK_EXECUTOR_ABI_VERSION_EVENTS 5`, adding the
   first device-supplied callback: a `signal` function pointer and a device
   opaque handed to the library, with `ExecutorLibrary::names_events()` beside
   `names_options()` and `ContextExecutor::accepts` consulting it - the shape
   feature 050 used for `create_with_options`. A version 4 library keeps
   working and simply reports no events.
2. **An any-thread landing pad.** The host callback's whole budget on the
   foreign thread is a short record under a small mutex plus one
   `Host::signal_event` on the worker's wake event. That is precedented: the
   doorbell's KVM ioeventfd already sets the same event from outside the worker
   (`src/device/threading/doorbell.cpp:19-30`). `Worker` needs an any-thread
   wake beside its main-loop-only `post`.
3. **`FenceReporter::nudge()`** - worker-side, bumps `progress_`, republishes
   and sets `pending_`, so the next `signal()` raises with `completed_fence`
   unchanged. This is the one new fence operation, and §6.3 is why it is legal.
4. **No fence-page change.** The page is fixed at 32 bytes with no room for an
   event id, and it does not need one: the wake carries no payload and the ICD's
   watcher thread asks which events completed over the wire, as the
   clarification says.

### 6.5 The lifetime hazard

OpenCL cannot unregister a `clSetEventCallback` callback. `ExecutorRoot::close`,
`OpenClBackend::shut_down` and `ContextExecutor::destroy` can all run while a
host callback is in flight, so the trampoline must reach the device through
something that outlives the session - an epoch the callback checks, or a
registration held until the host event itself is released. This is the single
most dangerous part of the feature and it gets its own tests.

## 7. Memory: what a map and an SVM allocation actually are

`HostVisibleChunk` (`src/guest/transport/host_visible_chunk.hpp:19`) is genuinely
CPU-mappable host GPU memory: allocate an `AllocationKind::host_visible_memory`
allocation on the connection's device, `Kmt::lock` it for a CPU view - "for one
in a CPU-visible segment, the host's own memory through the hostmem BAR"
(`src/guest/kmt/kmt.hpp:41-44`) - and bind it through a D3DKMT escape on the
transport's own rvk context so the host can import it. `KmtBlobArena` wraps that
and lends the view to the transport's `ClientBlobArena`, 16 MiB by default and
256 MiB at most (`kmt_blob_arena.hpp:24-25`). It cannot be had before the
context's `KmtTransport::initialize()` has succeeded.

**Today the OpenCL client never uses it.** `grep -c "blobs()"
src/proto/generated/opencl/cl_dispatch_client.cpp` is **0**, against 1033 uses in
the OpenGL client: every OpenCL argument crosses inline in the ring. So the
zero-copy half of US2 is new generator work, not something already there.

### 7.1 A mapped buffer

**Decision.** A buffer the application asks to map is backed by the context's
arena: the guest names an offset in its bound chunk, and the device makes the
host buffer over the chunk's host address with `CL_MEM_USE_HOST_PTR`. The
device knows that address because it allocated the chunk. `clEnqueueMapBuffer`
then returns the guest view of the same bytes and costs no copy, and
`clEnqueueUnmapMemObject` costs none either. Where the capset offers no
host-visible memory, or the buffer is larger than the arena, the ICD stages the
copy in at map and out at unmap - what the OpenGL ICD's `BufferMapping` does,
logged once per context so the log says which path a run took.

### 7.2 An SVM allocation

Coarse-grained SVM requires host access to happen inside
`clEnqueueSVMMap`/`clEnqueueSVMUnmap`, which is what makes a mirror correct
rather than a compromise. The host's `clSVMAlloc` is the only thing that can
produce a pointer a kernel may dereference, and it allocates in the host
process - it cannot be made to allocate out of the guest's chunk.

**Decision.** An `SvmAllocation` is a pair: the host pointer `clSVMAlloc`
returned, and a guest mirror of the same size the ICD allocates. The
application is handed the **mirror**, because it is the pointer the application
dereferences. `clEnqueueSVMMap` fills the mirror from the host,
`clEnqueueSVMUnmap` writes it back, and `clSetKernelArgSVMPointer(p)` is
translated by the ICD - mirror base plus offset becomes host base plus the same
offset - before anything is sent. **The wire carries host addresses only.** The
device therefore never translates: `svm_for(address, bytes)` (§5.2) only has to
answer whether `[address, address + bytes)` lies inside one live allocation this
context made, which is a bounds check, not a lookup table.

`CL_DEVICE_SVM_CAPABILITIES` reports `CL_DEVICE_SVM_COARSE_GRAIN_BUFFER` alone,
and only when the host device reports it.

### 7.3 What the mirror cannot do yet, and why

Filling the mirror turned out to need something no command on this wire
carries. `clEnqueueSVMMemcpy` names two addresses and both of them are the
host's - that is what an SVM pointer is - and the commands that do carry bytes
between guest memory and the host, `clEnqueueReadBuffer` and
`clEnqueueWriteBuffer`, carry them for a `cl_mem` and not for an address.

So as built, a kernel's view of shared memory is whole: an allocation is made,
its address translates, a kernel takes it as an argument, and what one kernel
writes another reads. The host's view between `clEnqueueSVMMap` and
`clEnqueueSVMUnmap` is not: what an application writes into the mirror does not
reach the allocation. The ICD says so once into its log rather than pretending
otherwise, and `CL_DEVICE_SVM_CAPABILITIES` still reports coarse-grained
because that is what the device does - the gap is in this ICD's data path, not
in what the host offers.

Closing it needs one of two things, and both are a change to the wire rather
than to the ICD: a command that carries bytes to and from an SVM address, or a
way to wrap an SVM allocation as a `cl_mem` so that read and write reach it -
`clCreateBuffer` with `CL_MEM_USE_HOST_PTR` over an SVM pointer is exactly
that, and it needs `host_ptr` to be expressible as an address on a command
where it is otherwise bytes. Neither is a small change to the profile, and
neither is this feature's to make quietly.

**Alternatives considered.** Sending guest addresses and translating on the
device: rejected, it puts a guest-chosen address in the device's arithmetic for
no gain. Backing SVM with the host-visible chunk: rejected, a kernel cannot
dereference a pointer the host's OpenCL did not allocate.

## 8. What the ICD is made of

The Vulkan ICD is the closer model of the two, because OpenCL has no current
context: every call names its objects. What OpenGL contributes is
`GlConnection`'s device-loss policy (`src/icd/opengl/gl_connection.hpp:25`) -
when one context's transport faults its whole device is lost, the contexts on it
keep their link until they are deleted, and the next context opens a new device
beside it. That is what a `cl_context` outliving a fault needs.

What is **reused unchanged** from `src/icd/common/`: `ThreadLock`, `LogSink`,
`LockedTransport` (exactly the serialisation a multi-threaded OpenCL
application needs over one staging buffer and one reply slot), and `SlicedWait`.
What is **not**: `ExtensionPolicy`, whose `report()` is spelled in
`VkResult`/`VkExtensionProperties`. 050 already set the precedent - `GlExtensionPolicy`
takes `using Row = icd::ExtensionPolicy::Row;` and matches over GL's shape - and
052 does the same over `CL_DEVICE_EXTENSIONS`' one space-separated string.
`WindowSurface` and `FramePacer` are not taken at all: this ICD presents nothing.

Two constraints from the transport that shape the design:

- **A context gets no creation parameters.** `RvkContextRecord`'s last eight
  bytes are OpenGL's options and must be zero for any other sub-API
  (`src/proto/context/rvk_context_record.hpp:8-10`), and `KmtTransport` refuses
  non-default options on a non-OpenGL context without asking the Kmt. So
  anything OpenCL needs at creation travels as a wire command after the context
  exists.
- **Every blocking call is sliced.** A host call that blocks holds its context's
  worker, and a batch that outlives TdrDelay has dxgkrnl reset the adapter. So
  `clWaitForEvents`, `clFinish`, a blocking `clEnqueueRead/WriteBuffer` and a
  blocking map all go through `SlicedWait`'s 500 ms slices, as
  `IcdRoot::wait_for_fences` does.

`ContextFault`'s second constructor takes a command id
(`src/guest/transport/context_fault.hpp:23`) precisely so a non-Vulkan context
can fault itself - a Vulkan command is merely skipped by another API's decoder -
so the probe's forced-fault step names an OpenCL command id.

## 9. How it is tested

`ScriptedHost` already speaks OpenCL: `cl_cpu_platform`, `cl_gpu_platform`,
`cl_device`, `cl_context`, `cl_queue`, `cl_buffer`, `cl_program`, `cl_kernel`
handles and the `cl_result`/`cl_handle`/`cl_ids`/`cl_read`/`cl_info` encoders
(`tests/harness/guest/scripted_host.hpp:39-106`), with `opencl_run()` and
`opencl_waits = 24`. `MockKmt` never blocks: `queue_answer` is a FIFO of one
answer per wait, `withhold_answer_at` plays a host-faulted context,
`play_at_wait` runs a callback on the locked views between a submission and its
wait - which is how a host-visible copy is played - and `cut_next` truncates an
answer the way a host that wrote a short reply leaves it. Lock order is asserted
as a string by `LockTrace` and `RecordingThreadLock`, whose destructor fails the
test on a re-acquire, an unbalanced release, or a lock still held.

Five things the harness does not have and 052 adds:

1. **`FakeClLoader`** - the analogue of `FakeOpengl32`. It calls the ICD's
   exports, reads the `cl_icd_dispatch *` out of the first word of each object
   it is handed, and dispatches **through the table**, so a slot left null or
   filled with the wrong arity fails a test rather than a guest.
2. **`ClIcdRig`** - `MockPlatform`, `MockKmt`, `ScriptedHost`, `LockTrace`, two
   `RecordingThreadLock`s, `RecordingLog`, `FakeClLoader` and the root, with the
   rvkgpu adapter added and a LUID set. No window surface, no pacer.
3. **A stepped thread** for the watcher seam: nothing else in this harness is
   threaded and `RecordingThreadLock` asserts single-thread ownership, so the
   test drives the watcher from the test thread.
4. **An out-of-band signal path** through `MockKmt`/`ScriptedHost`: the reply
   slot is strictly one answer per wait today, and the device's push has no
   guest-side script.
5. **`ClProcess` + `test_fullstack_opencl_icd.cpp`**, factored as
   `test_fullstack_opengl_icd.cpp` factors `GlProcess`, so the L4 test reuses
   the rig's aggregate with a real `DdiKmt`. Its strongest assertion is the one
   the GL test makes: ask the executor for `CL_DEVICE_NAME` with no guest in
   between, and require the ICD to hand the application the same bytes.

The host OpenCL drivers leak on first use; `tests/loopback/test_fullstack_opengl_icd.cpp:39-58`
already carries `__lsan_default_suppressions` naming `libOpenCL`, `intelocl` and
`libigdrcl`, and the OpenCL tests take the same.
