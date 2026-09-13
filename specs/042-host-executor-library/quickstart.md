# Quickstart: building the executor, pointing a device at it, holding it to the contract

Read this as a run guide, not a design. The design is `plan.md`, `research.md`
and `contracts/executor_abi.md`.

**What exists today.** The device loads an executor library by its `executor=`
property and calls it (`src/device/qemu/virtio-gpu-rvk.c:52-66`); the only
library that answers is `rvk_echo_executor`
(`tests/qemu/echo_executor.cpp`, built at `CMakeLists.txt:131-132`); the Vulkan
and OpenCL server dispatch is generated and compiles
(`CMakeLists.txt:71`, `:73`).

**What does not exist yet.** `src/device/executor/`, the library's CMake
target, `tests/conformance/executor/`, `tests/fuzz/fuzz_executor_abi.cpp`,
`src/proto/gl_sync_table.hpp`, and version 2 of
`src/device/abi/rvk_executor.h` — which still says
`RVK_EXECUTOR_ABI_VERSION 1u` (`src/device/abi/rvk_executor.h:24`). Every
command below that names one of those says so where it names it.

## Once, per machine

```bash
tools/fetch_khronos_registries.py     # vk.xml, gl.xml, cl.xml + headers, by hash
tools/fetch_windows_kit.sh            # the configure refuses without it
```

Without the registries the generated sources are dropped from the build with a
message saying so (`CMakeLists.txt:79-86`), and there is nothing for the
executor to decode with. The fetch writes `third_party/khronos/include/` with
`vulkan/` and `CL/` only; OpenGL compiles against the registry-generated
`src/proto/generated/opengl/gl_types.h` and needs no external header
(`CMakeLists.txt:76-78`).

A host GPU is needed to *run* anything, not to build: the Vulkan loader, EGL
and the OpenCL ICD loader are all resolved at runtime (plan.md, Technical
Context). Which sub-APIs this host can actually serve is what these three
answer:

```bash
vulkaninfo --summary          # a "Devices:" section per Vulkan physical device
clinfo -l                     # one line per OpenCL platform and device
eglinfo -B                    # per EGL platform; the GBM one is the surfaceless path
tools/host/list_gpus.py       # the same Vulkan devices as the device sees them, by UUID
```

`list_gpus.py` is the authority for the UUID the `host-gpu=` property takes,
because the device binds by Vulkan device UUID and nothing else.

What this particular machine answers is recorded in `research.md` §3, per GPU
and per import path — read the table there rather than re-deriving it. The two
facts that change what you can test here: NVIDIA has no OpenGL dma-buf import
and no `GL_AMD_pinned_memory`, and its OpenCL `cl_khr_external_memory` is
absent, so zero-copy on the 3070 Ti is Vulkan-only. `/dev/udmabuf` is present
and usable by this user, which is what makes the guest-backed path testable at
all.

## Building the library

The library is built by the `AV_QEMU` configuration, next to the echo executor:

```bash
cmake -S . -B build-qemu -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DAV_QEMU=ON
cmake --build build-qemu
ls build-qemu/librvk_echo_executor.so     # exists today
ls build-qemu/librvk_executor.so          # this feature's target; does not exist yet
```

`AV_QEMU` is the right configuration because it is the only one that returns
before the unit-test half of `CMakeLists.txt` (`CMakeLists.txt:143`), and
because it is where the device it plugs into gets built. The echo executor's
three lines are the model:

```cmake
add_library(rvk_echo_executor SHARED tests/qemu/echo_executor.cpp)
target_include_directories(rvk_echo_executor PRIVATE src/device/abi)
```

**The plan does not name the new target.** `rvk_executor` /
`librvk_executor.so` is the name used throughout this document; if `tasks.md`
picks another, the path is the only thing that changes. Unlike the echo
executor it links `rvk_device`'s sources for the generated dispatch and the
wire, and it is built with the same flags as everything else here — no STL, no
exceptions, no RTTI (`CMakeLists.txt:18-26`, and `research.md` §4, which
settles that the line does not move).

The first `cmake --build build-qemu` also builds QEMU 11.1.1 from
`tools/fetch_qemu.sh` through `tools/build_qemu.sh`
(`CMakeLists.txt:125-129`); that is the long part, and later builds relink
only.

## Pointing a device at it

The property is a path to the shared object. `tests/qemu/rvk_context.py:39-41`
builds the line the suites use:

```python
def rvk_device(options=""):
    return f"virtio-gpu-rvk-pci,host-gpu=auto,executor={EXECUTOR}{options}"
```

By hand, with this feature's library:

```bash
QEMU_MODULE_DIR=build-qemu/qemu build-qemu/qemu/qemu-system-x86_64 \
    -M q35 -display none -nodefaults -machine accel=qtest -qtest stdio \
    -device virtio-gpu-rvk-pci,host-gpu=auto,executor=$PWD/build-qemu/librvk_executor.so
```

`host-gpu` takes `auto` or a device UUID from `list_gpus.py`. The distro QEMU
cannot load these modules; it must be the built one.

**How you know the library was accepted.** Realize is silent on success — the
only lines a good realize prints are the feature-039 probe verdicts, which
`qemu_smoke.py` deliberately filters out of its "complaints"
(`tests/qemu/qemu_smoke.py:64-70`). So the positive signal is indirect: the
device offers one capset and `VIRTIO_GPU_F_CONTEXT_INIT`
(`src/device/qemu/virtio-gpu-rvk.c:74-77`), which reads back as
`device_config(32, 12) == 1` over qtest
(`tests/qemu/test_virtio_gpu_contexts.py:135`).

The negative cases are loud, and these are the exact strings:

| What is wrong | What realize says |
|---|---|
| the path does not resolve, or is not a shared object | `virtio-gpu-rvk: executor '<path>' could not be loaded: <g_module_error>` (`virtio-gpu-rvk.c:57-58`) |
| it loads but exports no entry point | `virtio-gpu-rvk: executor '<path>' exports no rvk_executor_ops` (`virtio-gpu-rvk.c:62`) |
| `host-gpu` is malformed or names no device | `is neither auto nor a device UUID` / `no host Vulkan device has UUID` (`tests/qemu/qemu_smoke.py:91-95`) |

`qemu_smoke.py:96-100` already asserts the first two against
`/nonexistent/librvk_codec.so` and `libc.so.6`. Two cases have **no** message
today and are this feature's to add (research §1, defects 1 and 3): a
`rvk_executor_ops()` that answers null, and a version the device does not know.
Both must fail realize rather than yield a capset-less device (contract A1).

## The conformance suite

**It does not exist.** `tests/conformance/executor/` is in the plan's project
structure and nothing is there yet. This is the shape it takes.

It is a binary built by the `AV_QEMU` configuration that takes a library path,
loads it exactly as the device does — `g_module_open`, then the
`rvk_executor_ops` symbol — and walks the statements in
`contracts/executor_abi.md` §A and §B in order. It is registered as a ctest per
library, so a failure names which library failed:

```bash
ctest --test-dir build-qemu -R executor_conformance --output-on-failure
# or, directly:
build-qemu/executor_conformance build-qemu/librvk_echo_executor.so
build-qemu/executor_conformance build-qemu/librvk_executor.so
```

**It must run against `rvk_echo_executor` too.** A suite only one library
passes is a suite that has tested nothing: it would be asserting this
implementation's behaviour, not the contract's. The echo executor answers
version 1 and offers only Vulkan (`echo_executor.cpp:70`, `:121`), so the suite must
pass on a version-1 library with no blob operations at all and check §B only
where the table carries it — that split is exactly what FR-006 means by "the
device accepts both".

A pass prints one line per statement checked and exits 0. A failure names the
statement by its id from the contract (`A7`, `B2`, …), the library, and what it
answered instead. Two statements need machinery rather than an assertion:

- **A9, every call returns.** Each call is made under a watchdog and a call
  that has not returned inside the bound fails the suite rather than hanging
  it. This is the one the device itself cannot enforce — `Worker::stop` joins
  unconditionally on the main loop (research §1).
- **A10, no invented result.** Any value above `RVK_DEVICE_LOST` fails. Today
  the device folds those into `device_lost`
  (`src/device/execution/context_executor.cpp`, research §1 defect 3).

Worker identity (research §1, defect 2) is **not** checked: `TableHost` only
tests that the BQL is not held. The suite records it as a known gap rather than
pretending.

Under ASan and UBSan for SC-001 — which, because `AV_QEMU` returns before
`AV_SANITIZE` is applied (`CMakeLists.txt:143`, `:165-169`), needs the suite
and the library built with `-fsanitize=address,undefined` explicitly. **The
plan does not say how**; see "What the design does not yet answer".

## The three workloads

SC-002 names three, and they are qtest scenarios in `tests/qemu/`, driven the
way `test_virtio_gpu_execution.py` drives the echo executor: a context with a
guest-backed ring, a fence page and a reply slot (`Context.__init__`,
`test_virtio_gpu_execution.py:35-55`), batches written into the ring and rung
through the doorbell, and the answer read out of the reply slot with
`Context.answer()` (`:66-69`).

The constraint that shapes all three: **one reply per batch, the last
reply-flagged command's answer** (`src/device/execution/batch_executor.cpp:107-108`),
and `reply` is null with capacity zero for every command that did not ask
(`:95-96`). Nothing can volunteer an answer. So a workload either ends with the
one command whose answer is the result, or writes its result into a blob the
qtest reads back directly with `qtest.read_bytes`.

| Workload | Submitted | Comes back | Read as |
|---|---|---|---|
| Vulkan triangle | one batch of generated Vulkan commands: create instance, enumerate and create a device, an image and a render pass, draw, then a readback whose destination is a host-visible blob | the last reply-flagged command's answer: the `VkResult` chain's end | `Context.answer()` for the status; the pixels with `qtest.read_bytes` over the blob's guest address |
| OpenGL clear | a GL context on surfaceless EGL, `glClearColor` + `glClear`, then `glReadPixels` | `glReadPixels` is a bulk-data command: it lands in the blob, not the reply | `qtest.read_bytes` over the blob; the reply carries only the status |
| OpenCL vector add | build a program, two input buffers imported from blobs the guest wrote, a kernel enqueue, a finish | the finish's status | `Context.answer()` for the status; the output buffer with `qtest.read_bytes` |

`rvk_context.py` already has the blob helpers each of these needs —
`host_blob(resource, size)` and `guest_blob(resource, address, size)`
(`rvk_context.py:69-77`) — and the suite creates both for every context today.
What it does **not** have is the command encoder: these batches are generated
Vulkan/GL/CL wire, not `command(ECHO, ...)`, and no Python encoder for the
generated wire exists. That is a task, not a command you can run now.

**The OpenGL clear cannot run until three things are written.** Only
`gl_command_names.cpp` is in the build (`CMakeLists.txt:74`);
`gl_dispatch_server.cpp` is generated and committed but compiles nowhere,
because `src/proto/gl_sync_table.hpp` does not exist, `RealGl::rvk_blob` has no
resolver behind its 1016 call sites, and four mapping commands are unwritten
(research §2). Vulkan and OpenCL have complete passthrough dispatch today and
have no such blocker.

**Zero-copy is not available for every workload on every GPU here.** On the
3070 Ti, OpenGL and OpenCL import nothing (research §3), so those two workloads
run over the boundary copy — ~45 µs/MiB — and the capset must say so. Run each
workload against both GPUs by UUID, not `host-gpu=auto`, or you will only ever
test the Intel answer.

## Fuzzing the boundary

The generated decoder is already fuzzed:

```bash
cmake -S . -B build-fuzz -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DAV_FUZZ=ON
cmake --build build-fuzz
ctest --test-dir build-fuzz -R fuzz_generated_decode_smoke --output-on-failure
```

That target is dropped from the build when the registries are not fetched
(`CMakeLists.txt:290-292`), because there is nothing to decode against.

This feature adds `tests/fuzz/fuzz_executor_abi.cpp`, which the glob picks up
with no CMake edit (`CMakeLists.txt:287`), giving a
`fuzz_executor_abi_smoke` ctest and requiring a committed non-empty seed corpus
at `tests/corpus/fuzz_executor_abi/` — `cmake/check_fuzz_corpora.cmake` is a
configure-time `FATAL_ERROR` otherwise. Corpora live under
`tests/corpus/<target>/` and are read-only; new units land in
`build-fuzz/corpus/<target>/` and crashes as `build-fuzz/corpus/<target>-*`
(`CMakeLists.txt:300-305`).

```bash
build-fuzz/fuzz_executor_abi build-fuzz/corpus/fuzz_executor_abi \
    tests/corpus/fuzz_executor_abi -max_total_time=60 -print_final_stats=1
```

60 s per target is the gate's default (`AV_FUZZ_SECONDS`,
`CMakeLists.txt:284-285`); SC-003 asks for 60 s per sub-API, so the ABI target
must either take the sub-API from the input's first bytes or be split into
three. The required outcome is a fault, never a crash: the target drives
`execute`, `import_blob` and `release_blob` with hostile ids, payloads and blob
handles, and asserts the answer is one of the four defined results.

## The gates

```bash
tools/gate.sh                                  # all four Linux configurations, then the guest
ctest --test-dir build -E tools_tests -j20     # the fast loop, ~5 s
```

| Configuration | Directory | What it costs | What it adds for this feature |
|---|---|---|---|
| default | `build/` | `-E tools_tests` is ~5 s; `tools_tests` alone is ~9 min | the unit tests under ASan+UBSan for every class in `src/device/executor/`, plus `design_rules` and `symbol_tests` |
| coverage | `build-coverage/` | a full rebuild plus the run | 100% branch coverage of what this feature adds to `src/device` and `src/proto` |
| fuzz | `build-fuzz/` | 60 s per target, 20 targets today and 21 with `fuzz_executor_abi`, 20-way parallel — a few minutes | `fuzz_executor_abi_smoke` and `fuzz_generated_decode_smoke` |
| qemu | `build-qemu/` | the first build compiles QEMU; after that, minutes | `qemu_smoke`, `qemu_virtio_gpu`, and the conformance ctests |

`tools/gate.sh:17-22` is the list; it configures, builds and runs each in turn
and stops at the first failure.

**The L5 guest run is a merge gate.** `gate.sh:40-48` runs it when
`AV_GUEST_IMAGE` names a prepared image and prints that it skipped otherwise —
and from feature 037 on, a skipped guest is not a pass
(`docs/testing.md`, "The gate").

```bash
virsh --connect qemu:///session list --all      # win11 must read "shut off"
AV_GUEST_IMAGE=/home/accel/.local/share/libvirt/images/win11.qcow2 tools/gate.sh
```

The image is the `win11` **session** domain's disk, so the domain holds the
qcow2 lock while it runs and the suite will not start. `virsh shutdown` is not
enough while fast startup is on; shut down from the guest agent, or turn fast
startup off first with `powercfg /h off` (`docs/testing.md`, "Preparing an
image", step 1). Everything a run leaves is in `build-guest/`.

## What failure looks like

| Symptom | What it means |
|---|---|
| the device realizes, prints nothing, and no context can be created | `sub_apis` answered zero. `DeviceCapset::describe` returns without describing anything in that case (`src/device/capset/device_capset.cpp:19`), so the device is a plain virtio-gpu: `device_config(32, 12)` reads 0, `VIRTIO_GPU_F_CONTEXT_INIT` is not offered, `GET_CAPSET_INFO` answers `INVALID_PARAMETER` and `CTX_CREATE` answers `INVALID_PARAMETER` — exactly what `test_virtio_gpu_contexts.py:120-127` asserts for a device with no executor at all. Check the capset count before blaming the ring. |
| the workload runs but every buffer is copied on a GPU where research says zero-copy works | `import_kinds` answered zero for that sub-API and GPU. It is queried on the main loop at realize, before any context (`contracts/executor_abi.md` §B2), so it is decided by what the *instance-level* probe found, not by what the context later does. Cross-check the host itself with `tools/host/probe_host_memory.py --qemu build-qemu/qemu`, which prints one JSON object per GPU and exits 0 even when a probe is refused; if the probe says the path exists and `import_kinds` still says zero, the fault is in the library, not the host. |
| a context faults with code 6 on a frame that is not malformed | `virtio-gpu-rvk: context faulted, code 6` (`src/device/ring/ring_consumer.cpp:117`, `FaultCode::undecodable`, `src/proto/submission/fault_code.hpp:17`) is what the wire says for **both** a bad frame and an arena that refused — on purpose, because telling a guest the arena's size tells an attacker where the edge is (`contracts/executor_abi.md` §C). The two are distinguishable only in the log and in `DecodeArena`'s refusal count. `WireArena` has no such count today (`src/proto/wire/wire_arena.hpp:21-46`); adding one is this feature's, and until it is there a large legitimate command and an attack look identical. |
| `fuzz_executor_abi` will not configure | `cmake/check_fuzz_corpora.cmake` fails configure in **every** configuration when a target has no seeds: commit at least one file under `tests/corpus/fuzz_executor_abi/`. |

## What the design does not yet answer

Four things in `spec.md`, `plan.md` and `contracts/executor_abi.md` could not be
turned into a runnable command here:

1. **The conformance suite's name and entry point.** The plan gives the
   directory and the class (`ExecutorConformance`) but no target, no ctest name
   and no argument convention. `executor_conformance <library path>` above is a
   proposal.
2. **How SC-001's "under ASan and UBSan" is built.** `AV_QEMU` returns from
   `CMakeLists.txt` before sanitizer flags are set, and the library is loaded by
   a QEMU that is not sanitized. Whether the suite gets its own sanitized
   configuration, or runs the library out-of-QEMU against a stub host, is
   undecided.
3. **Which capset bits carry `import_kinds`.** FR-007 says the capset reports
   only the import paths the executor supports, but `CapsetInfo`'s feature bits
   today are feature 039's three probe bits
   (`src/proto/context/capset_info.hpp:22-27`) and nothing maps a per-sub-API
   `RVK_BLOB_*` mask onto them.
4. **How a workload's wire is encoded from Python.** The three SC-002 scenarios
   need generated Vulkan/GL/CL command bytes; `rvk_context.py` encodes only the
   echo executor's four ids, and no generator emits a Python encoder.
