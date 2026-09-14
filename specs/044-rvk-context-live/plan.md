# Implementation Plan: rvk contexts in a real guest

**Branch**: `044-rvk-context-live` | **Date**: 2026-09-14 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/044-rvk-context-live/spec.md`

## Summary

043 carried a hand-written echo command from a guest process to the host and back.
044 carries real Vulkan and OpenCL streams, encoded in the guest by the generated
clients, to `librvk_executor.so` on a real host GPU, and shows a malformed stream
faults only its own context. Research changed four things and added one:

1. The emitters write freestanding C++, so the generated Vulkan and OpenCL
   clients build into the no-CRT `rvktest.exe`.
2. **Three guest-to-host memory-safety holes in the executor are closed**: OpenCL
   handles are validated like Vulkan's, and `clEnqueueReadBuffer` and
   `glReadPixels` sizes are checked; OpenCL and OpenGL executor fuzz targets are
   added.
3. A transport `NOP` (`0x00000001`) the device answers itself gives a no-op's
   latency and a scenario that runs under either executor.
4. Vulkan's pattern round trip moves to 045 (mapping), host-visible OpenCL buffers
   to 052, and OpenGL's clear/read-back to 050 (where its client links); 044
   proves the Vulkan object chain on the host GPU, OpenCL vector add, and an
   OpenGL context set up on the host.
5. The runner records the executor and GPU, and the gate boots the guest twice:
   the suite with the echo library, then `rvktest.exe rvk` with the real one.

## Technical Context

**Language/Version**: C++23 freestanding (guest suite, device core, generated
code); Python 3 (emitters, runner).

**Primary Dependencies**: 043's transport (`KmtTransport`, `GuestConnection`);
042's executor library and generated server dispatch; 063's generators;
QEMU device (`build-qemu`).

**Testing**: L1 over `MockKmt` for every scenario; executor unit tests and two new
fuzz targets for the hardening; `check_generated` for the emitter change;
qtest for NOP; L5 `run_guest_suite.py --rvk` with the real library.

**Target Platform**: Windows guest (user mode, no CRT) and Linux host (QEMU
process).

**Constraints**: no STL/CRT in guest code; command body ≤ 1024 bytes and answer ≤
4064 bytes (043's transport); nothing a guest sends may reach a host ICD
unvalidated (constitution V).

**Scale/Scope**: five guest scenarios, one device command, three executor
checks, one emitter change for two APIs, runner and gate changes.

## Constitution Check

| Principle | How this feature satisfies it |
|---|---|
| I. Specification precedes implementation | research.md records every scope change with its reason; 045, 050 and 052's specs carry what moved |
| II. Core portable, shim translation only | NOP is decided in `src/device/execution`; the executor checks live in generated server code and `src/device/executor`, not the GPU shim |
| III. Object-oriented, with state | every scenario is an object class like 037's; the OpenCL registry owns its handles |
| IV. Test-first and exhaustive | each scenario over MockKmt at 100% branches; the hardening gets hostile-input tests and fuzz targets |
| V. Everything from outside is hostile | the three holes are fixed before any real stream reaches the executor from a guest |
| VI. Feature levels are phases | capability reporting unchanged |
| VII. IRQL and locking | no kernel change |
| VIII. No speculative structure | NOP has a caller (the scenario and latency); nothing is built for a mapping path that does not exist |
| IX. Reproducible and observable | the verdict records executor, GPU and latency |

No violations.

## Classes this feature introduces

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| `TransportCommand` | object (device) | the batch's reply writer | Answer a command of the transport's own API |
| `ClHandleRegistry` | system (device) | the OpenCL objects this executor made | Say whether a wire value is a live OpenCL handle |
| `VulkanScenario` | object (guest) | the transport and the handles it made | Build the Vulkan object chain on the host GPU |
| `OpenClScenario` | object (guest) | the transport, the kernel and its buffers | Run vector add on the host GPU |
| `IsolationScenario` | object (guest) | two transports | Show a faulted context changes no other |

`TransportScenario` (043) gains the NOP, its latency and a sub-API: run on an OpenGL context it is the OpenGL scenario, so no `OpenGlScenario` class exists; `GuestSuite::run_rvk` runs the rvk scenarios, so no `RvkSuite` exists either.

## Project Structure

```text
tools/codegen/rvk_codegen/emit/, opencl/emit.py   # freestanding includes and builtins; regenerate
tools/fetch_khronos_registries.py                 # vk_video headers from the pinned tarball
src/um/CMakeLists.txt, src/um/imports/ntdll.def   # generated VK/CL clients and strlen
src/proto/submission/, docs/protocol.md           # NOP
src/device/execution/                             # TransportCommand
src/device/executor/, generated CL/GL server      # ClHandleRegistry, read size checks
tests/fuzz/fuzz_executor_opencl.cpp, _opengl.cpp  # new
src/guest/suite/                                  # the scenarios and RvkSuite
src/um/guest_program.cpp                          # `rvktest.exe rvk`
tools/guest/run_guest_suite.py, tools/gate.sh     # --rvk, executor recorded, second boot
```

## Complexity Tracking

| Violation | Why needed | Simpler alternative rejected because |
|---|---|---|
| Two guest boots in the gate | the TDR scenario needs the echo library's stall injection and the rvk scenarios need the real library | loading both in one device is not supported by the device's one `executor=` |
