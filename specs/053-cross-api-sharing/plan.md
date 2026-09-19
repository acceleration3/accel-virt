# Implementation Plan: Sharing across APIs and processes

**Branch**: `053-cross-api-sharing` | **Date**: 2026-09-19 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/053-cross-api-sharing/spec.md`

## Summary

One object, one host resource, however many guest contexts and processes name
it. Today a host object belongs to the context that made it: `ImportTable` is
per session, `HostExportedFds` is per context, and the Vulkan ICD's
`ExternalObjects` refuses any handle this process did not export (047's
`contracts/external_objects.md` says so in as many words). This feature gives an
object an identity the whole *device* holds - a 64-bit id minted once by the
adapter's miniport, carried in the allocation's private driver data, and
refcounted on the host by `HostSharedObjects` - and then builds the three APIs'
Win32 interop extensions on top of it.

Three things settled the shape (research.md):

1. **The identity has to be minted in the kernel, not the ICD.** dxgkrnl hands a
   second process the *creating* process's allocation private data
   (kb 04, "Allocation lifetime traps"), and that is the only channel that
   crosses a process boundary without a name. So the id lives in a
   `SharedObjectRecord` the miniport writes at create and reads at open, and a
   forged one is an id the adapter's `SharedObjectTable` never minted.
2. **The host side is one descriptor, not one object.** A guest device is a host
   device, so two guest contexts sharing an image are two host `VkDevice`s
   sharing one `VkDeviceMemory`'s fd - exactly what 047 already does inside one
   context, moved up to the device. The device keeps the fd; each importer gets
   a `dup` of it. The refcount is contexts holding the object, not fds.
3. **Nothing new crosses to the guest as an fd.** The guest names a shared
   object by its id only. An fd never leaves the QEMU process, so the hostile
   surface this adds is three 24-byte transport bodies, not a descriptor
   channel.

## Technical Context

**Language/Version**: C++23 freestanding (core, device, proto, ICDs); Python 3
(gate scripts).

**Primary Dependencies**: 047's `ExternalObjects`, `Win32HandleKind`,
`HostExportedFds` and the Win32 handle records; 051's OpenGL presentation and
`GlExtensionPolicy`; 052's `ClExtensionPolicy`, `ClObjectTable` and
`GuestClContext`; 045's `BlobImports`/`ImportTable` import path and 041's
host-visible memory; the executor ABI (`src/device/abi/rvk_executor.h`).

**Storage**: N/A.

**Testing**: L1 for every class added to `src/proto`, `src/core`, `src/device`
and `src/icd`; L2 for the miniport's shared create/open/close through the fake
dxgkrnl; L3 fuzz targets over the two records and the transport body; L4
loopback for two contexts holding one host object and for the owner dying
first; L5 the guest matrix, which this environment cannot run.

**Target Platform**: Windows 11 guest (x64), Linux/QEMU host.

**Project Type**: driver components (miniport core, QEMU device, executor
library, three user-mode ICDs, wire protocol).

**Performance Goals**: an import of a shared object costs one `dup` and one
host import; nothing on the submission path changes.

**Constraints**: no STL/CRT, no exceptions, `av::Status` everywhere; the
executor ABI stays loadable by libraries built for versions 1-5; nothing
reported that is not implemented and tested (constitution VI).

**Scale/Scope**: one wire record, one transport command family of three, one
adapter-global table in the miniport, one device-global table in the device,
executor ABI version 6, and the Win32 external-object paths of the three ICDs.

## Constitution Check

| Principle | How this feature satisfies it |
|---|---|
| I. Specification precedes implementation | spec.md states the three user stories and their independent tests; research.md answers every open question from the WDK headers, `dxgkrnl`, and this repository's own code |
| II. Core portable, shim translation only | every decision is in `src/core/memory`, `src/device/memory`, `src/device/execution`, `src/proto` and `src/icd`; `src/um` gains only the three `GdiKmt` translations |
| III. Object-oriented, with state | every class in the table below owns state; no static member function, no stateless class, no new free function |
| IV. Test-first and exhaustive | each class at 100% branches with `invariants_hold` asserted after every mutation; hostile tables and fuzz targets for `SharedObjectRecord`, `SharedObjectRequest` and the transport body; the allocation-failure sweep on both tables |
| V. Everything from outside is hostile | the miniport believes no id it did not mint, no kind that disagrees with the object, and no LUID but its own; the device believes no id its table does not hold and no wire handle its executor did not make |
| VI. Feature levels are phases | each interop extension is reported only where this ICD carries it *and* the host reports the fd equivalent; `cl_khr_gl_sharing` is reported only on a context whose device shares a GL executor |
| VII. IRQL and locking | the miniport's table declares PASSIVE and takes a ranked mutex; the device's table is reached from workers under its own host mutex |
| VIII. No speculative structure | one shared-object kind enum with four real members; no per-API table where one serves; the executor ABI grows by exactly the three calls the device cannot do without |
| IX. Reproducible and observable | every refusal logs the id and the reason once; `HostSharedObjects::report` puts the table in the device's state dump |

No amendment is needed: this feature adds no process-lifetime state.

## Classes this feature introduces

Wire, `src/proto/`:

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `SharedObjectRecord` | object | the magic, version, object id, kind, adapter LUID and size of one shared object | Carry a shared object's identity in an allocation's private driver data |
| `SharedObjectRequest` | object | the object id, kind and wire handle one transport command names | Frame the body the three shared-object transport commands share |
| `SharedObjectAnswer` | object | the status and the wire handle a shared-object command answers | Frame what a shared-object command answers |

Guest kernel, `src/core/memory/`:

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `SharedObject` | object | its id, kind, size, the allocation behind it, and how many devices hold it | Be one shared object the adapter knows |
| `SharedObjectTable` | system | the adapter's shared objects and the id it mints next | Give an allocation an identity the whole adapter agrees on |

QEMU device, `src/device/`:

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `HostSharedObject` | object | its id, kind, size, the host descriptor behind it, and how many contexts hold it | Be one host resource several contexts name |
| `HostSharedObjects` | system | the device's shared objects, under the lock its workers reach it through | Keep one host resource per object id for the whole device |
| `ContextSharedObjects` | object | what one context published or opened, and the wire handle each became | Hold one context's share of the device's shared objects |
| `SharedObjectCommand` | object | the header, the body and the tables the command reaches | Answer one SHARE / OPEN / RELEASE transport command |

Guest user mode, `src/icd/`:

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `icd::SharedHandles` | system | the NT handles this process holds, each with the object id behind it | Turn a Win32 handle into the object id the device knows |
| `glicd::GlExternalObjects` | system | the GL memory objects and semaphores this context imported | Hold one GL context's imported external objects |
| `clicd::ClExternalMemory` | system | the `cl_mem` objects this context imported from Win32 handles | Hold one OpenCL context's imported external memory |
| `clicd::ClGlSharing` | system | the GL objects a context has acquired, and the depth of each | Track which GL objects an OpenCL context currently owns |

Changed, not new: `AllocationDescriptor` (a shared record may follow it),
`Allocation` (a shared allocation may be held by more than one device),
`DeviceTable::open` (a second device's open of a shared object),
`Adapter` (the `SharedObjectTable` it owns), `CommandHeader` (ids 4-6),
`TransportCommand` (the three ids), `DeviceContext` (its
`ContextSharedObjects`), `VirtioGpuRvk` (its `HostSharedObjects`),
`ContextExecutor` (the three ABI calls), `VulkanBackend`, `OpenGlBackend` and
`OpenClBackend` (`export_object`/`import_object`/`release_object`), the
executor ABI (version 6), `ExternalObjects` (a handle that is now the
kernel's), the three extension policies, and `Kmt`/`GdiKmt`/`MockKmt` (three
calls).

## Project Structure

### Documentation (this feature)

```text
specs/053-cross-api-sharing/
├── plan.md, research.md, data-model.md, quickstart.md
├── contracts/shared_objects.md     # the identity, its refusals, and the NT handle
├── contracts/wire_additions.md     # the record, the three commands, ABI version 6
└── tasks.md
```

### Source Code

```text
src/proto/resources/shared_object_record.{hpp,cpp}
src/proto/resources/shared_object_kind.hpp
src/proto/submission/{shared_object_request,shared_object_answer}.{hpp,cpp}
src/proto/submission/command_header.hpp            # ids 0x4-0x6
src/core/memory/{shared_object,shared_object_table}.{hpp,cpp}
src/core/memory/{allocation,allocation_table}.*    # a shared allocation
src/core/scheduling/device_table.*                 # a second device's open
src/core/adapter/adapter.*                         # the table it owns
src/device/abi/rvk_executor.h                      # version 6
src/device/memory/{host_shared_object,host_shared_objects}.{hpp,cpp}
src/device/execution/{context_shared_objects,shared_object_command}.{hpp,cpp}
src/device/execution/{transport_command,context_executor}.*
src/device/executor/{vulkan,opengl,opencl}_backend.*
src/icd/common/shared_handles.{hpp,cpp}
src/icd/vulkan/external_objects.*                  # handles that cross a process
src/icd/opengl/gl_external_objects.{hpp,cpp}
src/icd/opencl/{cl_external_memory,cl_gl_sharing}.{hpp,cpp}
src/guest/kmt/kmt.hpp                              # share/open/close
src/um/icd/gdi_kmt.*                               # their Windows translations
tests/unit/{proto,core,device,icd}/...             # one file per class
tests/fuzz/fuzz_{shared_object_record,shared_object_request}.cpp
tests/loopback/test_fullstack_shared_objects.cpp
```

**Structure Decision**: the identity is minted in `src/core/memory/` beside the
allocations it names, and held in `src/device/memory/` beside the blobs, because
both are memory tables and both are already the thing their side refcounts. The
transport command joins `src/device/execution/` beside `BlobImportCommand`,
whose framing rules it copies exactly.

## Complexity Tracking

| Violation | Why needed | Simpler alternative rejected because |
|---|---|---|
| Executor ABI version 6 (three calls) | the device must move a host object's descriptor between two executors, and nothing in the table runs that way | reusing `import_blob` was tried on paper: its handle is a guest blob's pages, and a shared object has none |
| A second refcount (the device's, beside the miniport's) | the two sides fail independently - a guest process can die without the device noticing, and a context can fault without the guest noticing | one refcount would make either side's loss a leak on the other |
| `ContextSharedObjects` beside `HostSharedObjects` | what a context made of an object is the context's and dies with it; what the object *is* outlives every context | one table would have to be unwound per context anyway, which is the second table written inside the first |
