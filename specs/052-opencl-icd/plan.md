# Implementation Plan: OpenCL - the ICD on rvkgpu

**Branch**: `052-opencl-icd` | **Date**: 2026-09-18 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/052-opencl-icd/spec.md`

## Summary

`rvkcl.dll`, an OpenCL 3.0 ICD the Khronos loader finds through the adapter's
`OpenCLDriverName`, over the guest transport of feature 043. The loader resolves
one export and dispatches through a `cl_icd_dispatch` this ICD fills, so every
object it hands out carries that table in its first word; the generated OpenCL
client encodes each call onto one rvk OpenCL context per `cl_context`, and the
device's `OpenClBackend` runs it on the host GPU.

Research turned three parts of the spec from "add it" into real work.
The wire does not carry maps, SVM or `clSetEventCallback`, and eight of the ten
commands the clarification adds do not lower at all today: the generator needs
an address return and a new SVM-pointer role (§5). The device cannot signal the
guest out of band at all - every fence-page mutation is worker-only and the
executor ABI has no way back from the library - so the event-completion push
costs ABI version 5, an any-thread landing pad, and a new fence operation (§6).
And `cl_icd_stubs.h`, which fills every unclaimed dispatch slot, has never been
compiled: the `stub_called` it calls is declared nowhere (§3).

## Technical Context

**Language/Version**: C++23 freestanding (ICD core, device, executor, proto);
Python 3 (generator, gate scripts, guest runner).

**Primary Dependencies**: 044's live rvk contexts; 043's `KmtTransport`,
`GuestConnection` and `ClientBlobArena`; 041's host-visible memory and blob
import; 045/050's shared ICD pieces in `src/icd/common/` and `src/um/icd/`
(`ThreadLock`, `LockedTransport`, `ExtensionPolicy`, `SlicedWait`, `LogSink`,
`ProcessHeap`, `SrwLock`, `FileLog`); 063's vendored OpenCL generator and the
committed tree in `src/proto/generated/opencl/`; the device's `OpenClBackend`
and `OpenClDevice`; the Khronos ICD loader in the guest test image.

**Storage**: N/A.

**Testing**: L1 over `MockKmt` with a scripted host for every ICD class,
including a fake Khronos loader that dispatches through `cl_icd_dispatch`
exactly as the real one does; emitter tests in `tests/tools/`; device and
executor unit tests for the ten new commands and the out-of-band event signal;
fuzz targets over the property lists, the info queries and the host replies;
L4 full-stack through the ICD's dispatch table on the real executor; L5
`run_guest_suite.py --opencl` with `rvkclprobe.exe` through a real `OpenCL.dll`.

**Target Platform**: Windows 11 guest (x64 user mode, no CRT); Linux host.

**Project Type**: driver components (user-mode DLL, QEMU device, host executor
library, generator).

**Performance Goals**: no enqueue blocks the calling thread beyond the host's
own queueing; a mapped buffer over imported guest memory costs no copy.

**Constraints**: no STL/CRT; the DLL loads, unloads and reloads cleanly;
nothing reported that is not tested (constitution VI); the executor ABI stays
loadable by libraries built for earlier versions.

**Scale/Scope**: one ICD DLL, ten new wire commands with their generator
support, an out-of-band event signal through the device and executor, a probe,
a pinned loader fetch, runner and INF changes.

## Constitution Check

| Principle | How this feature satisfies it |
|---|---|
| I. Specification precedes implementation | spec clarified (2026-09-18, five questions); research.md answers each from the headers, the binaries and this repository's code |
| II. Core portable, shim translation only | every decision in `src/icd/opencl` and `src/icd/common`; `src/um/opencl` holds one-line exports and the Windows seams only |
| III. Object-oriented, with state | every class in the spec's table owns state; the ICD root is one `ObjectStorage` in the exports' translation unit, reached by the generated hooks through `ClIcdBinding` - the exception 045 and 050 already took, taken a third time (PATCH amendment) |
| IV. Test-first and exhaustive | each class over `MockKmt` at 100% branches; hostile property lists, info queries, attribute sizes and host replies, each with a fuzz target; invariants after every mutation; the allocation-failure sweep |
| V. Everything from outside is hostile | the ICD judges every `cl_*` object pointer, size, offset, flag and property list before arithmetic or a host command; the device judges every wire handle before the host's OpenCL sees it |
| VI. Feature levels are phases | version and extensions are the intersection of this ICD's policy with the host device; `cl_khr_gl_sharing` withheld until 053; SVM reported only as coarse-grained |
| VII. IRQL and locking | user mode throughout; lock order recorded and asserted by the recording locks the ICD harness provides |
| VIII. No speculative structure | the thread seam exists because the watcher thread needs it and the tests need a stepped one; nothing is added for a second backend that does not exist |
| IX. Reproducible and observable | the loader is pinned by URL and hash; the ICD's log names every refusal and the map path it took |

**Amendment carried (PATCH):** the constitution's process-lifetime list gains
`rvkcl.dll`'s root behind its ICD exports and `ClIcdBinding`, for the reason
045's and 050's entries give.

## Classes this feature introduces

Guest user mode, `src/icd/opencl/` (namespace `av::clicd`):

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `ClIcdRoot` | system | the injected seams, the dispatch table, the platform, the object table, the connection, the extension policy, the watcher | Answer the ICD entry points the loader reaches |
| `ClIcdBinding` | object | the one pointer to the root | Reach the root from an entry point handed no context |
| `ClDispatchTable` | object | one `cl_icd_dispatch`, this ICD's entries in it | Present one filled dispatch table to the loader |
| `ClPlatform` | object | the dispatch pointer, the suffix, the version, the extension string | Be the platform the loader enumerates |
| `ClObject` | object | the dispatch pointer, its kind, its wire handle, its owning context | Be one object the application holds |
| `ClObjectTable` | system | every live `ClObject`, by kind, with its generation | Judge an object pointer that came from the application |
| `GuestClContext` | object | its link, staging, transport, blob arena, SVM table, mappings, state | Be one `cl_context` |
| ~~`ClConnection`~~ | - | - | Not written: `GlConnection` was already this, and moved to `src/icd/common/` as `icd::IcdConnection` for both ICDs |
| `ClDeviceReport` | object | the adapter's LUID, the version cap, the capabilities this feature tests | Answer the device queries this ICD decides rather than forwards |
| `ClExtensionPolicy` | object | the reportable rows and the test each names | Decide which extensions the application sees |
| `SvmAllocation` | object | host base, guest mirror, bytes, map depth | Be one coarse-grained SVM allocation |
| `SvmTable` | system | the SVM allocations of one context | Translate a mirror address into its host address |
| `MappedRegion` | object | the buffer, the range, the arena lease or the staged block | Be one live mapping of a buffer |
| `ClEventWatcher` | system | the registered callbacks, the service thread, the transport it asks | Run an event's callbacks once the host reaches it |

Shared seam, `src/icd/common/` and `src/um/icd/`:

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `icd::ServiceThread` | object (interface) | nothing; two implementations own the thread | Run one loop until it is asked to stop |
| `WindowsServiceThread` | object | the thread handle and its stop event | Run that loop on a Windows thread |

Device and executor, `src/device/`:

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `HostSvmRanges` | system | the SVM allocations one session made | Say whether an address and extent lie inside one of them |
| `ClEventSignals` | system | the host events registered, and which have completed | Record a host event's completion for the worker to read |
| `ContextSignal` | object | an any-thread flag and the worker's wake event | Wake a context's worker from a thread that is not it |

Changed, not new: `FenceReporter` (a `nudge()` that raises with `progress`
moved and `completed_fence` unchanged), `Worker` (an any-thread wake beside its
main-loop-only `post`), `OpenClBackend` (the ten commands, the host callback
trampoline, the SVM ranges), `ExecutorLibrary`/`ContextExecutor` (a
`names_events()` gate), the executor ABI (version 5, the first device-supplied
callback), `src/proto/icd_stub_report.hpp` (`stub_called`), and the OpenCL
generator (an address return, `Role::SVM_POINTER`, a `svm_for` server hook, a
callback trampoline hook).

Test harness, `tests/harness/icd/`: `FakeClLoader`, `ClIcdRig`, `ClObjectRig`,
`ScriptedThread`, and `ScriptedHost`'s new encoders.

## Project Structure

### Documentation (this feature)

```text
specs/052-opencl-icd/
├── plan.md, research.md, data-model.md, quickstart.md
├── contracts/icd_interface.md      # INF values, exports, the dispatch table, object layout
├── contracts/wire_additions.md     # the ten new commands and the event signal
└── tasks.md
```

### Source Code

```text
tools/codegen/rvk_codegen/opencl/{model,lower,emit}.py   # address return, Role.SVM_POINTER, svm_for, the callback hook
tools/codegen/profiles/opencl.toml                       # the ten commands and their annotations
tools/codegen/profiles/opencl_ids.toml                   # ids 0x2006A-0x20073, append-only
src/proto/generated/opencl/                              # regenerated, committed with the ids
src/proto/icd_stub_report.hpp                            # stub_called, beside payload_dropped
src/icd/common/service_thread.hpp                        # the thread seam
src/icd/opencl/                                          # the ICD: the table above
src/um/icd/windows_service_thread.{hpp,cpp}              # the Windows thread
src/um/opencl/{rvkcl_exports.cpp,rvkcl.def,cl_icd_library.{hpp,cpp}}
src/um/imports/OpenCL.def                                # the probe's import library
src/um/probe/cl/                                         # rvkclprobe's shim
src/guest/suite/{cl_probe,cl_entry}.{hpp,cpp}            # the probe's scenarios, portable
src/device/abi/rvk_executor.h                            # version 5: the signal callback
src/device/executor/{opencl_backend,host_svm_ranges,cl_event_signals}.*
src/device/{fence/fence_reporter,threading/worker,context/device_context}.*
src/km/rvkgpu.inf                                        # OpenCLDriverName, REG_SZ, rvkcl.dll
tools/{fetch_opencl_loader.py,opencl_loader_pins.txt}    # the loader, pinned and built
tools/guest/run_guest_suite.py                           # --opencl
tests/harness/icd/{fake_cl_loader,cl_icd_rig,cl_object_rig,scripted_thread}.*
tests/unit/icd/opencl/, tests/loopback/test_fullstack_opencl_icd.cpp
tests/fuzz/fuzz_{cl_property_list,cl_info_query,cl_host_reply}.cpp
```

**Structure Decision**: `src/icd/opencl/` beside `src/icd/vulkan/` and
`src/icd/opengl/`, sharing `src/icd/common/` and `src/um/icd/`. The ICD is
modelled on the Vulkan one, because OpenCL names its objects in every call and
has no current context; the device-loss policy is OpenGL's `GlConnection`.

## Complexity Tracking

| Violation | Why needed | Simpler alternative rejected because |
|---|---|---|
| A sixth piece of process-lifetime state (the OpenCL ICD root and `ClIcdBinding`) | the loader calls `clGetExtensionFunctionAddress` and `clIcdGetPlatformIDsKHR` with no ICD context | one root shared with the Vulkan or OpenGL ICD would make each DLL link the others' world |
| The first device-supplied callback in the executor ABI (version 5) | a host OpenCL event completes on the host driver's thread, and nothing in the ABI runs device-ward | the ICD polling `clGetEventInfo` was offered and declined; it would need no device change at all |
| A new fence operation (`FenceReporter::nudge`) | the guest must be woken with no batch completed, and `completed_fence` may not move past what was submitted | reusing `complete()` would claim batches finished that had not |
| An any-thread wake on `Worker` | the host callback may touch nothing worker-owned, so its whole budget is a record and a wake | `Worker::post` is main-loop-only by design, and relaxing it would let any thread queue work |
| A third ICD archive (`av_cl_icd`) | each ICD defines strong hooks that replace the generated defaults every other binary relies on | one archive for all three would collide at link time, as 050 found |
| An `SvmAllocation` mirror rather than shared memory | a kernel may only dereference a pointer the host's own `clSVMAlloc` produced | backing SVM with the guest's host-visible chunk cannot produce such a pointer |
