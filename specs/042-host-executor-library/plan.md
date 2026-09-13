# Implementation Plan: the host executor

**Branch**: `042-host-executor-library` | **Date**: 2026-09-13 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/042-host-executor-library/spec.md`

## Summary

The device has had a hole where the host GPU should be since feature 032: it
loads an executor library by the `executor=` property, splits batches into
commands and calls `execute` for each, and the only library that answers is
`rvk_echo_executor`, which echoes payloads. Feature 063 then generated the half
that decodes: `vk_dispatch_server.cpp` and `cl_dispatch_server.cpp` already
compile into this build.

This feature writes what sits between them - a shared object that implements
`rvk_executor.h` over host Vulkan, OpenGL and OpenCL - and, because it owns the
contract, the conformance suite that holds *any* library to it, which does not
exist today.

Two things are bigger than they look and set the phasing:

1. **The ABI has no memory channel at all.** Every byte an executor can see
   today is one command's payload. "Imported, not copied" is not a tweak to
   version 1; it is the reason version 2 exists. Research settled its shape: two
   tagged handle kinds, exact ranges with no offset, explicit release, and a
   capability query on the table because the capset is decided at realize.
2. **OpenGL is not the same kind of work as the other two.** Vulkan and OpenCL
   get complete passthrough backends from the generator and need subclassing;
   OpenGL gets a table of 1375 function pointers, and three things this feature
   must write before `gl_dispatch_server.cpp` compiles at all: the sync table,
   the guest-memory resolver its 1016 bulk-data sites call, and four mapping
   commands the generator refused to guess.

## Technical Context

**Language/Version**: C++23, freestanding, for the library and the device
changes; C for the ABI header. Python 3 for the conformance runner's qtest half.

**Primary Dependencies**: the generated server dispatch (063); the host's
Vulkan loader, EGL and the OpenCL ICD loader, all resolved at runtime so the
library builds on a machine with headers and no GPU; `/dev/udmabuf` and QEMU's
`UDMABUF_CREATE_LIST` for guest-backed blobs.

**Testing**: L1 unit tests over every class with `MockPlatform`; a new
executor conformance suite driven against both the echo library and this one;
the existing qtest virtio-gpu suites; a fuzz target over the ABI boundary; and
the L5 guest run.

**Target Platform**: Linux x86-64 host, in the QEMU process. Nothing here
crosses into the guest.

**Project Type**: a shared object plus device-side changes in an existing
repository.

**Performance Goals**: none of its own beyond not making SC-002 impossible. The
measured fallback copy is ~45 µs per MiB, which is why the capset must be
truthful about import rather than fast about copying.

**Constraints**: no C++ standard library, no exceptions, no RTTI - the line does
not move (research §4). Memory comes from the host ops with an explicit pool.
Nothing on the submission path allocates. Every call the device makes must
return, because nothing times it out.

**Scale/Scope**: 386 Vulkan entry points, 105 OpenCL and 1375 OpenGL, of which
this feature hand-writes 41 (36 + 1 + 4) and inherits the rest.

## Constitution Check

| Principle | How this feature satisfies it |
|---|---|
| I. Specification precedes implementation | The spec was rewritten in 063 when render-server stopped being a dependency; this plan follows it and records where the code disagreed with it. |
| II. The core is portable; the shim is not | The library is host-side and never reaches `src/km`. The host APIs are behind the host ops the device already owns. |
| III. Object-oriented, with state | Every class below owns state and is named with its kind. The C entry points are the ABI boundary the constitution permits, and each is one line. |
| IV. Test-first and exhaustive | The conformance suite is written before the library and must fail against a stub; the ABI boundary gets a fuzz target with a committed corpus. |
| V. Everything from outside is hostile | The guest's command payload, every count inside it, and every blob id are attacker-chosen. The generated decoder is fuzzed in 063; this feature fuzzes the boundary above it and the blob table. |
| VI. Feature levels are phases | Vulkan first and it can stop there; OpenGL and OpenCL are later phases with their own checkpoints. |
| VII. IRQL and locking are typed | No IRQL here, but the thread rule is the same shape: `sub_apis` and the capability query on the main loop, `create`/`execute`/`destroy`/`import`/`release` on one context's worker. Every public method asserts which. |
| VIII. No speculative structure | Two handle kinds because two exist and both are needed; no third. No abstraction over the three backends until two are written. |
| IX. Reproducible and observable | The conformance suite is the reproduction; a fault says which of decode, arena exhaustion or host loss caused it. |

No violations.

## Classes this feature introduces

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `HostGpu` | object | one host GPU: its Vulkan device UUID, its DRM render node, which sub-APIs and import kinds it serves | Answer what one host GPU can run |
| `HostGpuSet` | system | every host GPU this process found | Find the GPU a UUID names |
| `ExecutorRoot` | system | the `HostGpuSet` and the live sessions | Answer the ABI's table for this process |
| | | | *Named a root, not a library: the device already has an `av::ExecutorLibrary` - the table it loaded - and these are opposite ends of one ABI* |
| `HostApis` | interface | nothing; it is the seam | Say what this host's own loaders can do |
| `LoaderHostApis` | object | the opened loader, its instance and what it enumerated | Ask this host's loaders |
| `ContextSession` | object | one context's backend, arena block and imports | Run one context's commands |
| `SessionBackend` | interface | nothing; it is the seam | Say what a session requires of a host API |
| `DecodeArena` | object | the block one decode unflattens into, and how often it refused | Hand the generated decoder room for one command |
| `VulkanBackend` | object | one context's Vulkan instance, device and queues | Run Vulkan commands on the host |
| `OpenGlBackend` | object | one context's EGL display, context and `RealGl` table | Run OpenGL commands on the host |
| `OpenClBackend` | object | one context's CL platform, device and context | Run OpenCL commands on the host |
| `ImportedBlob` | object | one blob's handle, the API object it became, whether it was released | Hold one guest blob inside one host API |
| `ImportTable` | system | the imports one session holds, by blob id | Find what a blob id was imported as |
| | | | *Named `ImportTable`, not `BlobTable`: the device already has an `av::BlobTable` for its own blob resources (`src/device/memory/blob_table.hpp:22`)* |
| `GuestRegion` | object | the base and length one context's blob resolver may hand out | Turn a guest offset into a bounded host pointer |
| `SyncTokens` | system | the live token-to-`GLsync` pairs of one OpenGL context | Find the host fence a guest token names |
| `BlobHandleSource` (device side) | system | the udmabuf and host-pointer handles the device has built | Build the handle for one blob, on the main loop |
| `ExecutorConformance` | system | the scripted calls one library must answer | Judge a library against the ABI |

Sixteen, which is what a feature spanning an ABI, a device change, three host
APIs and a suite costs. Eleven of them are the library; two are the device
side; `ExecutorConformance` is the test that FR-001 says this repository owes.

`HostApis` is virtual for the reason `Platform` is: two real implementations,
and the mock is what makes a host with no GPU, a host with two, and a host
whose OpenCL cannot import testable on a machine that is none of those. Its
loader-facing half lives in `src/device/shim/`, which is the non-portable edge
constitution II names - one rule in `header_scan.is_shim`, kept by the coverage
and symbol gates, so that moving a file out of that directory puts both gates
back.

Three named non-classes: the `extern "C"` ops functions are the ABI boundary
(constitution III's exception) and each forwards one line into
`ExecutorLibrary`; `gl_sync_table.hpp`'s three functions are the generated
code's C-style hook and forward into `SyncTokens` the same way; `RealGl` is a
generated POD table, not ours.

## Project Structure

```text
src/device/abi/rvk_executor.h           # version 2: the memory channel
src/device/executor/                    # the library
src/proto/gl_sync_table.hpp             # the generated OpenGL hook's header
tests/conformance/executor/             # the suite any library must pass
tests/fuzz/fuzz_executor_abi.cpp        # the boundary above the decoder
tests/unit/device/executor/             # one file per class
```

**Structure Decision**: the library lives under `src/device/` because it is the
host half and shares the no-STL rules; `gl_sync_table.hpp` lives in `src/proto/`
because the generated code includes it by that path and the guest's client
dispatch mints the tokens it names.

## Phase 0: research

`research.md` answers what the device actually promises (and the three places
its code does not keep the header's promises), what each API's generated
dispatch asks of a backend, and which import paths work on real hardware -
measured on both GPUs in this machine, because the answer differs per GPU and
per API and that is what forced two handle kinds rather than one.

## Phase 1: design

`data-model.md` describes the ABI's data - the ops table, the handle, the
session, the blob table - and the rules that make a regeneration or a second
implementation interchangeable. `contracts/` holds the version 2 header's
contract, statement by statement, including what the device must guarantee
before each call. `quickstart.md` is how to build the library, point a device at
it, run the conformance suite and fuzz the boundary.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| Two blob handle kinds | Vulkan imports a host pointer on both GPUs here; OpenGL and OpenCL import a dma-buf and only on Mesa | One kind gives up a whole API on one of the two GPUs in this machine - measured, not assumed |
| Fourteen classes | An ABI version, a device change, three host APIs and the suite the ABI has never had | Fewer would mean a class that owns two of: a GPU, a session, an import, a sync table - each of which has a different lifetime |
