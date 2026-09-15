# accel-virt Constitution

**Version**: 2.5.3 · **Ratified**: 2026-09-10 · **Last amended**: 2026-09-15

A paravirtual GPU for Windows guests on QEMU/KVM: a WDDM 3.2 kernel-mode
display miniport (`rvkgpu.sys`), a custom in-process QEMU device
(`virtio-gpu-rvk-pci`), and the shared wire protocol between them. Command
serialization is generated here from the Khronos registries, by the generator
vendored in `tools/codegen/`; this project owns all of it.

## Core Principles

### I. Specification precedes implementation

No code lands without a spec in `specs/NNN-*/spec.md` that states what it must
do and how it will be proven. Specs describe observable behaviour and
acceptance criteria, never implementation. The spec-kit flow is the workflow:
`/speckit-specify` → `/speckit-clarify` → `/speckit-plan` → `/speckit-tasks` →
`/speckit-implement`. A spec that cannot state its acceptance criteria is not
ready to plan.

### II. The core is portable; the shim is not

All decision-making logic lives in `src/core/` (guest) and `src/device/`
(host), compiles as freestanding C++23 with **no** WDM, dxgk, QEMU or libc
dependency, and is reachable from a native Linux test binary.

`src/km/` is the Windows kernel shim and `src/device/qemu/` is the QEMU glue.
Both are **translation only**: unpack a DDI or device callback into core types,
call core, pack the result. A shim file may contain no branch that is not
mandated by the platform ABI. If you are tempted to put an `if` in the shim,
the condition belongs in core.

This is not architectural taste. It is what makes Principle III possible.

### III. Object-oriented, with state (NON-NEGOTIABLE)

**Every unit of behaviour is a class that owns state. There are no static
member functions, no stateless classes, and no free-function dumping grounds.**

If a group of functions threads the same handle, buffer, bounds or platform
through their parameter lists, that parameter is a member. `Alignment` owns an
alignment. `ByteRange` owns a span. `GpuVirtualAddressSpace` owns a table and a
`Platform&`. None of them receive what they own as an argument.

Two kinds of class, and everything is one of them:

- **Object classes** — one instance per real thing. `Adapter`, `Device`,
  `Context`, `Allocation`, `VirtualRange`, `MonitoredFence`, `VirtQueue`,
  `Doorbell`, `Edid`.
- **System classes** — a long-lived service owning a collection, injected where
  it is needed. `GpuVirtualAddressSpace`, `Scheduler`, `PagingEngine`,
  `DisplayEngine`, `TimeoutDetector`.

Dependencies are injected at construction, never reached for. Nothing consults
a global, a singleton, or file-scope state.

The only exceptions are the ABI boundaries C requires, and each is one line
with no logic: the `DxgkDdi*` entry points in `src/core/ddi/`, which forward to
the adapter behind the context pointer, and the `extern "C"` entry points QEMU
and libFuzzer call. The DDI entry points live in core, over mirror types whose
layout `src/km` proves against the WDK, so the L2 harness drives the same entry
points `dxgkrnl` calls.

Six pieces of process-lifetime state exist because the ABI leaves no
alternative, and nothing else may: `DxgkDdiAddDevice` and `DxgkDdiUnload`
receive no context, so they reach the driver's root through the single pointer
`MiniportBinding` holds; the kernel runs no global constructors, so
`DriverEntry` constructs that root in the shim's static storage; QEMU's C
glue cannot hold a C++ object, so the device module keeps its devices in a
`DeviceModule` in the ABI translation unit's static storage
(`src/device/abi/entry_points.cpp`), handing the glue opaque handles; the
executor library's `rvk_executor_ops()` takes no argument, so its root lives in
that entry point's translation unit (`src/device/shim/executor_ops.cpp`); and the
Windows Vulkan loader calls `vk_icdGetInstanceProcAddr(NULL, ...)` with no
context, so the Vulkan ICD's root lives in its exports' translation unit
(`src/um/vulkan/rvkvk_exports.cpp`, feature 045), and the generated wrappers'
hooks and the ICD's own entry points, which are handed no context either, reach
it through the single pointer `IcdBinding` holds (`src/icd/vulkan/icd_hooks.cpp`).
An object whose
lifetime cannot be a scope is held in `ObjectStorage<T>`; an object whose count
has no fixed bound - a device, an allocation - lives in an `ObjectPool<T>`,
which constructs it in platform-allocated chunks at an address that stays valid
until it is destroyed. These two templates are the only places a placement-new
expression may appear; `tools/check_design.py` enforces it.

The full standard, including naming, ownership, error handling and the
freestanding-kernel adaptations, is in [`CLAUDE.md`](../../CLAUDE.md) and is
binding on every feature.

### IV. Test-first and exhaustive (NON-NEGOTIABLE)

**For every function in `src/core/`, `src/device/` and `src/proto/`, tests must
exercise every branch, assert the module's invariants, and feed it garbage.**

1. **Every branch.** Branch coverage of `src/core`, `src/device` and
   `src/proto` is 100%, measured by `llvm-cov`. A branch that genuinely cannot
   be reached from a test is marked `AV_UNTESTABLE("reason")` at the site. The
   coverage gate counts those marks, fails if the count rises without a spec
   change, and every mark is reviewed.

2. **Invariants.** Every class that owns state has
   `bool invariants_hold() const`, total and side-effect free.
   Tests call it after **every** mutating operation, not once at the end.
   Checked builds verify it on entry and exit of every mutating public
   function (`InvariantScope`). An invariant function that can never return false is
   not an invariant; delete it and write a real one.

3. **Garbage data.** Every function that reads bytes it did not write —
   guest memory, host memory, a ring, a virtqueue, a descriptor, a DDI struct,
   a user-mode buffer — gets both:
   - a table-driven hostile-input test covering, at minimum: zero length,
     truncated at every field boundary, length larger than the buffer,
     misaligned base, off-by-one indices, integer overflow in every computed
     size or offset, self-referential or cyclic links, maximum and minimum
     values of every scalar, and a byte-flip sweep of a valid input;
   - a libFuzzer target in `tests/fuzz/` with a committed seed corpus.

   The required outcome for hostile input is always the same: a returned error,
   no write outside the destination, no unbounded loop, no crash under ASan and
   UBSan. Never an assert — a malformed frame is a reachable state, not a bug.

4. **Tests name what they cover.** `TEST(gpuva_map, rejects_overlap_at_upper_bound)`,
   not `TEST(gpuva, test3)`. A reviewer must be able to map a test to a branch.

5. **The build enforces it.** `tools/check_symbol_tests.py` fails the build if
   any exported core symbol has no test referencing it. Coverage and fuzz
   smoke runs are gates, not reports.

6. **A test states a behaviour, not a line number.** 100% branch coverage is
   the floor, never the goal. Every test exists to fail when the code is
   wrong: it asserts the observable outcome, and the invariant that outcome
   implies, at values chosen because they are the interesting ones - the
   boundary, the wrap, the overflow, the refusal, the sequence a real caller
   performs. The question to ask of a test is "if I flipped this comparison or
   deleted this line, would it fail?" If not, it is not a test.

   These turn the gate green and catch nothing, and are rejected at review:
   calling a function and asserting only that it returned; sweeping arguments
   to walk branches without judging what each one produced; asserting a value
   the test computed the same way the implementation does; a `invariants_hold`
   that cannot return false; a hostile-input case whose expected outcome was
   read off the implementation instead of decided from the contract.

   If a branch cannot be made to matter to a caller, that is evidence about
   the branch, not about the test. Delete the branch rather than decorate it.

Tests are written before the implementation, and they must fail before they
pass.

### V. Everything from outside is hostile

Bytes from the guest, the host, a user-mode process or a device register are
attacker-controlled. The QEMU device decodes guest-authored bytes inside the
QEMU process, next to the host GPU driver: a memory-safety bug there is a
guest-to-host escape.

Every read is bounds-checked against a length the reader owns. Every
deserializer returns a status. A length or offset from outside is validated
before it is used in arithmetic, and the arithmetic is overflow-checked.
Trusting a value because "the driver on the other side wrote it" is forbidden;
the other side may be malicious or may be an older version.

### VI. WDDM feature levels are phases, not shipped versions

The driver always declares `DXGKDDI_INTERFACE_VERSION_WDDM3_2` (`0x11007`) and
always builds against WDDM 3.2 headers. Windows 11 will not load a driver that
declares an older level, so the historical levels are **implementation
ordering**, not deliverables.

A feature level is complete when its DDI slots are implemented or explicitly
declined, the capabilities it depends on are reported truthfully, and its
tests pass. Capabilities gate behaviour; the declared version never changes.

Never report a capability that is not implemented and tested. Half the failure
modes in this domain are a driver claiming support and then behaving
incorrectly, which surfaces as a silent failure far from the cause.

### VII. IRQL and locking are typed, not documented

Every public core function declares its maximum IRQL on entry with
`platform_.require_irql_at_most(...)`. The native test build tracks a simulated
IRQL, and the mock records a violation - failing the test unless the test
claims it - when a function is called above its ceiling, allocates at
DISPATCH, or blocks at DISPATCH. Lock ranks are declared once in
`src/core/platform/lock_rank.hpp`; acquisition order and LIFO release are checked at
runtime in the native test build, and by Driver Verifier in the guest.

The submission path never blocks, never allocates and never takes a mutex.
That is a testable property, and it is tested.

### VIII. No speculative structure

No interface with one implementation, no abstraction for a second backend that
does not exist, no configuration flag with one setting. Add it when the second
case arrives. A feature level that the roadmap marks `stub` gets a stub that
returns the correct "not supported" status and a test that proves it, not a
partial implementation.

### IX. Reproducible and observable

Every build is reproducible from a clean checkout with one command. Every
failure path logs once, at the point of decision, with the value that caused
it. The driver's log reaches the host from `DriverEntry` onward, before any
device exists.

## Technology constraints

- **Language: C++23**, freestanding subset — `-fno-exceptions -fno-rtti`, no
  C++ standard library, no heap `new`/`delete` (in-place construction only in
  `ObjectStorage` and `ObjectPool`). `basicrender.sys`, Microsoft's own
  WDDM miniport, is C++ classes with virtuals, so this is the supported way to
  write one. RAII is not a style preference here: it removes cleanup branches,
  which is what makes Principle IV's 100% branch coverage affordable.
  The QEMU device is C++ behind a C ABI, exactly as QEMU already links
  `rutabaga_gfx`: `src/device` builds `librvk_device.a` with `rvk_device.h`, and
  only the glue in `src/device/qemu/` - copied into QEMU's `hw/display/` by
  `tools/build_qemu.sh` - is C, because QEMU's meson builds no C++ in modules.
- **Toolchain**: clang/LLVM. Native tests and fuzzing with the host clang;
  the driver cross-built with `clang-cl` + `lld-link` against the WDK/SDK
  headers and import libraries. `gcc` is not supported.
- **Build**: CMake + Ninja. `ctest` is the only test entry point.
- **Kernel subset, enforced by the native build too**: the tests compile with
  the same `-fno-exceptions -fno-rtti` and no standard library, so nothing can
  depend on something the guest kernel will not have.
- **Sanitizers**: the native test build runs ASan + UBSan by default. A test
  suite that cannot run clean under both does not merge.
- **No dependency** may be added to `src/core` or `src/km`. `src/device` may
  depend only on QEMU's own headers and `librvk_codec`'s C ABI.

## Definition of done

A task is done when all of the following hold:

1. Its spec's acceptance criteria are demonstrated by an automated test.
2. `ctest` passes, including sanitizers.
3. Branch coverage of touched core files is 100%, or new `AV_UNTESTABLE` marks
   are justified in the PR.
4. Every new externally-fed function has a fuzz target that survives a
   60-second smoke run.
5. `tools/check_symbol_tests.py` passes.
6. Invariant methods exist on new stateful classes and are called by tests.
7. No new static member function, stateless class, or free function outside the
   two documented ABI boundaries.
8. The feature level's capability reporting matches what is implemented.
9. Documentation that the change makes wrong is fixed in the same change.
10. From feature 037 on, `tools/gate.sh` with `AV_GUEST_IMAGE` set passes: the
    driver installs in a real Windows guest under Driver Verifier, the adapter
    starts, and the in-guest suite reports no failure. The four Linux
    configurations cannot see a wrong belief about `dxgkrnl`, and that is the
    failure mode this project has.

## Governance

This constitution supersedes ad-hoc practice. Amendments are a PR that edits
this file, states the rationale, and bumps the version: MAJOR for removing or
redefining a principle, MINOR for adding one or materially expanding guidance,
PATCH for clarification.

Principles III and IV are not subject to schedule pressure. If a deadline and this
document conflict, the deadline moves or the scope shrinks.
