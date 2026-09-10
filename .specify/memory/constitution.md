# accel-virt Constitution

**Version**: 1.0.0 · **Ratified**: 2026-09-10 · **Last amended**: 2026-09-10

A paravirtual GPU for Windows guests on QEMU/KVM: a WDDM 3.2 kernel-mode
display miniport (`rvkgpu.sys`), a custom in-process QEMU device
(`virtio-gpu-rvk-pci`), and the shared wire protocol between them. Command
serialization comes from `render-server`; this project is everything else.

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
(host), compiles as freestanding C11 with **no** WDM, dxgk, QEMU or libc
dependency, and is reachable from a native Linux test binary.

`src/km/` is the Windows kernel shim and `src/device/qemu/` is the QEMU glue.
Both are **translation only**: unpack a DDI or device callback into core types,
call core, pack the result. A shim file may contain no branch that is not
mandated by the platform ABI. If you are tempted to put an `if` in the shim,
the condition belongs in core.

This is not architectural taste. It is what makes Principle III possible.

### III. Test-first and exhaustive (NON-NEGOTIABLE)

**For every function in `src/core/`, `src/device/` and `src/proto/`, tests must
exercise every branch, assert the module's invariants, and feed it garbage.**

1. **Every branch.** Branch coverage of `src/core`, `src/device` and
   `src/proto` is 100%, measured by `llvm-cov`. A branch that genuinely cannot
   be reached from a test is marked `AV_UNTESTABLE("reason")` at the site. The
   coverage gate counts those marks, fails if the count rises without a spec
   change, and every mark is reviewed.

2. **Invariants.** Every module that owns state exports
   `bool <module>_invariants(const <module>_t *)`, total and side-effect free.
   Tests call it after **every** mutating operation, not once at the end.
   Debug builds check it on entry and exit of every public function
   (`AV_INVARIANT_SCOPE`). An invariant function that can never return false is
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

Tests are written before the implementation, and they must fail before they
pass.

### IV. Everything from outside is hostile

Bytes from the guest, the host, a user-mode process or a device register are
attacker-controlled. The QEMU device decodes guest-authored bytes inside the
QEMU process, next to the host GPU driver: a memory-safety bug there is a
guest-to-host escape.

Every read is bounds-checked against a length the reader owns. Every
deserializer returns a status. A length or offset from outside is validated
before it is used in arithmetic, and the arithmetic is overflow-checked.
Trusting a value because "the driver on the other side wrote it" is forbidden;
the other side may be malicious or may be an older version.

### V. WDDM feature levels are phases, not shipped versions

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

### VI. IRQL and locking are typed, not documented

Every core function declares its maximum IRQL in its signature's annotation
macro (`AV_AT_PASSIVE`, `AV_AT_DISPATCH`, `AV_AT_ISR`). The native test build
tracks a simulated IRQL and aborts if a function calls something with a lower
maximum, allocates at DISPATCH, or blocks at DISPATCH. Lock order is declared
once in `src/core/locks.h` and checked at runtime in debug builds.

The submission path never blocks, never allocates and never takes a mutex.
That is a testable property, and it is tested.

### VII. No speculative structure

No interface with one implementation, no abstraction for a second backend that
does not exist, no configuration flag with one setting. Add it when the second
case arrives. A feature level that the roadmap marks `stub` gets a stub that
returns the correct "not supported" status and a test that proves it, not a
partial implementation.

### VIII. Reproducible and observable

Every build is reproducible from a clean checkout with one command. Every
failure path logs once, at the point of decision, with the value that caused
it. The driver's log reaches the host from `DriverEntry` onward, before any
device exists.

## Technology constraints

- **Language: C11** for `src/core`, `src/km`, `src/device`, `src/proto` and
  `tests/`. Not C++. The guest side is kernel-mode, the host side is a QEMU
  device, and the protocol headers are shared by both plus a C++ consumer;
  C11 is the only language all three speak without ceremony. C++ lives in
  `render-server`, behind a C ABI.
- **Toolchain**: clang/LLVM. Native tests and fuzzing with the host clang;
  the driver cross-built with `clang-cl` + `lld-link` against the WDK/SDK
  headers and import libraries. `gcc` is not supported.
- **Build**: CMake + Ninja. `ctest` is the only test entry point.
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
6. Invariant functions exist for new stateful modules and are called by tests.
7. The feature level's capability reporting matches what is implemented.
8. Documentation that the change makes wrong is fixed in the same change.

## Governance

This constitution supersedes ad-hoc practice. Amendments are a PR that edits
this file, states the rationale, and bumps the version: MAJOR for removing or
redefining a principle, MINOR for adding one or materially expanding guidance,
PATCH for clarification.

Principle III is not subject to schedule pressure. If a deadline and this
document conflict, the deadline moves or the scope shrinks.
