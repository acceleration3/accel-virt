# Data model: OpenCL - the ICD on rvkgpu

## Guest user mode (`src/icd/opencl`, namespace `av::clicd`)

**ClIcdRoot** - one per process, in `ObjectStorage` inside `ClIcdLibrary` in
`rvkcl_exports.cpp`; bound to `ClIcdBinding` for its life.
- Owns: the injected `BlockSource`, `Kmt`, two `ThreadLock`s, `LogSink`,
  `ServiceThread`; the `ClDispatchTable`, the one `ClPlatform`, the
  `ClObjectTable`, the `ClConnection`, the `ClExtensionPolicy`, the
  `ClEventWatcher`, and the `ObjectPool<GuestClContext>`.
- Rules: nothing opens an adapter until the first `clGetDeviceIDs`;
  `clIcdGetPlatformIDsKHR` and `CL_PLATFORM_ICD_SUFFIX_KHR` answer from the
  platform alone. Every pointer from the application is judged by
  `ClObjectTable` before it is used. Construction is serialised by the
  library's own one-shot, because the Khronos loader holds no lock.

**ClDispatchTable** - one, constant after fill.
- Fields: one `cl_icd_dispatch`.
- Rules: zeroed, this ICD's entries written, then
  `av::clproto::fill_unimplemented` last, which only writes null slots. After
  the fill no slot this ICD could be dispatched through is null. On a Linux
  host build the D3D/DX9 slots are `void *` and stay null, which a test must
  not mistake for a gap.

**ClPlatform** - one.
- Fields: the dispatch pointer (first word), the suffix `RVK`, the profile
  `FULL_PROFILE`, the version string, the name, the vendor, the extension
  string.
- Rules: answers `CL_PLATFORM_ICD_SUFFIX_KHR` and `CL_PLATFORM_EXTENSIONS`
  with no device open, because the loader asks both while enumerating. A query
  with a size too small answers `CL_INVALID_VALUE`; a null `param_value` with a
  size ret answers the size.

**ClObject** - one per object the application holds.
- Fields: the dispatch pointer (first word, so the loader can dispatch), the
  `Kind` (`platform`, `device`, `context`, `command_queue`, `mem`, `program`,
  `kernel`, `event`, `sampler`), the wire handle, the owning `GuestClContext`,
  the parent, the reference count the application holds.
- Rules: `Kind` is ordered so a parent's kind is always lower, which bounds the
  parent chain. The dispatch pointer is written before the object is returned,
  always.

**ClObjectTable** - one per process.
- Rows: an `ObjectPool<ClObject>` with a generation per slot.
- Rules: `made(pointer, kind)` is the only place a pointer from outside is
  believed - a stack address, a freed object, an object of another kind and a
  forged one all answer null. Destroying a context destroys its subtree. Wire
  handle 0 is never wrapped.

**GuestClContext** - one per `cl_context`.
- Fields: the `ClConnection` link (`no_link` until made), the staging block,
  `KmtTransport` of `SubApi::opencl`, `LockedTransport`, `KmtBlobArena`,
  `SvmTable`, up to 64 `MappedRegion`s, the state (`created`, `made`, `lost`).
- States: `created` -> (first call that needs the host) `made` -> `lost`.
  Nothing reaches the device before `made`; a make that fails holds nothing.
- Rules: `wire()` answers null unless made and not lost, and a faulted
  transport makes the context lost there - one accessor, so loss is detected on
  every call. The context carries no creation options: `RvkContextRecord`'s
  reserved tail is OpenGL's and must be zero here.

**ClConnection** - one per process.
- Rows: at most 4 links, each an `ObjectStorage<GuestConnection>` with its user
  count and lost flag; the `AdapterSearch`; the link new contexts join.
- Rules: `acquire` gives the current device or opens a new one when there is
  none or it was lost; `release` closes the device with its last user and the
  adapter with its last device; `lose` retires a link without disturbing the
  contexts already on it.

**ClDeviceReport** - one per device.
- Fields: the adapter LUID, the version the host reported capped at 3.0, the
  SVM capabilities this feature implements, the vendor id.
- Rules: every other `CL_DEVICE_*` is forwarded to the host unchanged. The four
  it decides are decided because forwarding them would name the host's own
  adapter rather than rvkgpu's.

**ClExtensionPolicy** - one, constant.
- Rows: `using Row = icd::ExtensionPolicy::Row;` - name, version, the test that
  proves it, whether the host must report it too.
- Rules: an extension is reported only when a row names it and the host device
  reports it. `cl_khr_gl_sharing` has no row until 053.
  `tools/check_extension_policy.py` fails a row whose test does not exist.

**SvmAllocation** - one per `clSVMAlloc`.
- Fields: the host base the host's `clSVMAlloc` returned, the guest mirror, the
  size, the alignment, the map depth.
- Rules: the application is handed the mirror. Coarse-grained means host access
  is only defined inside map/unmap: a map fills the mirror from the host, an
  unmap writes it back. Freeing one still mapped is refused.

**SvmTable** - one per context.
- Rows: at most 256 `SvmAllocation`s.
- Rules: `host_address_of(pointer)` answers the host base plus the same offset
  for a pointer inside a live allocation, and null otherwise - so a pointer the
  application invented never reaches the wire. This is the only address
  translation in the design; the wire carries host addresses only.

**MappedRegion** - one per live mapping.
- Fields: the `cl_mem` it maps, the offset and size, the map flags, and either
  the arena lease (zero copy) or the staged guest block.
- Rules: which of the two a mapping is, is decided once per buffer and logged
  once per context. Unmapping a pointer that is not a live mapping of that
  object answers `CL_INVALID_VALUE`. A buffer released with a mapping live
  keeps the mapping's memory until the unmap.

**ClEventWatcher** - one per process.
- Rows: at most 256 registrations of (event, callback type, function,
  user data), the `ServiceThread`, and the context whose transport it asks.
- Rules: the thread sleeps in the transport's wait; it never polls. When it
  wakes it asks which events completed, then runs each callback **once**, with
  no ICD lock held, and forgets the registration. A callback that releases its
  event or its context is safe because the registration is forgotten before it
  runs. Nothing is registered on a lost context.

## Shared seam (`src/icd/common`, `src/um/icd`)

**icd::ServiceThread** - an interface, two implementations.
- `start(Runnable &)`, `stop()`, `running()`.
- `WindowsServiceThread` owns a thread handle and a stop event;
  `ScriptedThread` in the harness runs the loop on the test's own thread when
  the test steps it.

## Device and executor (`src/device`)

**HostSvmRanges** - one per session.
- Rows: the SVM allocations this session's guest made - base and size.
- Rules: `contains(address, bytes)` is a bounds check with overflow checked;
  it never dereferences. Registered at `clSVMAlloc`, forgotten at `clSVMFree`,
  and all forgotten at `shut_down`.

**ClEventSignals** - one per session.
- Rows: the host events a guest asked to be told about, and which have
  completed.
- Rules: the completion side is written from the host driver's thread under a
  short mutex and read on the worker; nothing else is touched from that thread.
  A registration outlives the session's teardown, because OpenCL cannot
  unregister a callback - the epoch it carries is what makes a late callback
  harmless.

**ContextSignal** - one per device context.
- Fields: an any-thread flag, and the worker's wake event.
- Rules: the only thing a foreign thread may do is set the flag and signal the
  event. The worker clears the flag and calls `FenceReporter::nudge`.

## Wire (`src/proto`)

Ten commands join the OpenCL protocol at ids `0x2006A`-`0x20073`; the fence
page does not change. `FenceReporter::nudge` moves `progress` and leaves
`completed_fence` where it was, which is what the guest's `FenceProgress`
already accepts.
