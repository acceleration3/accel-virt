<!-- decrapify-cpp: rules for writing new C++ in this repo. Same standard the
     12 cleanup passes enforce, applied before the code exists.
     Adapted for freestanding kernel-mode C++ - see "Adaptations" at the end. -->

# accel-virt

A paravirtual GPU for Windows guests on QEMU/KVM: a WDDM 3.2 display miniport
(`avgpu.sys`), a custom in-process QEMU device (`virtio-gpu-av-pci`), and the
wire protocol between them. Serialization is generated here from the Khronos
registries, by the generator vendored in `tools/codegen/`; what it writes is
committed under `src/proto/generated/` and never hand-edited.

**Read before writing code:** `docs/constitution.md` (the rules that
govern the project), `docs/architecture.md` (the code map: which directory
holds what, and which may include which), `docs/testing.md` (how a display
miniport is unit tested), `docs/roadmap.md` (what to build, in order).

## Repository organization

`docs/architecture.md` is the map; these are the rules that keep it true.

- **Organize by subsystem and domain.** A file lives in the directory of the
  binary and subsystem that owns it (`src/miniport/vidpn/`,
  `src/executor/d3d/`, `src/icd/opengl/`), not beside whatever calls it and
  not by technical kind. A developer who knows the feature should be able to
  predict the directory.
- **The top level is one directory per binary or layer**: `core` (the
  foundation every binary links), `proto` (what crosses a boundary),
  `miniport` + `km` (avgpu.sys), `device` (the QEMU device), `executor`
  (libav_executor.so), `icd` + `um` (the guest's user-mode drivers), `guest`
  (kmt, transport and the guest programs). Search this architecture before
  introducing a new top-level directory, and add one only for a new binary or
  layer.
- **Prefer shallow directories.** Two levels under `src/` is the norm. Do not
  create a directory for one small class without a strong reason; put it
  beside its owner.
- **No dumping grounds.** Do not create `utils`, `helpers`, `misc`, `common`
  or `shared` directories. `icd/common` and `um/common` exist because the
  user-mode drivers genuinely share them: anything there has at least two
  users among those drivers, and a file with one user moves to that user.
  Likewise `src/core` holds only what more than one binary needs.
- **Keep code near its owner.** A class used by one subsystem lives in that
  subsystem even when it looks generic; it moves outward only when a second
  owner uses it.
- **Respect the layers**: `core` includes nothing else; `proto` only `core`;
  nothing outside `src/km` includes `miniport`; nothing in `device` includes
  `executor`; `icd` never includes `guest/suite` or `guest/probe`.
- **Directory names are rules.** A directory named `shim` is exempt from the
  coverage gate and holds translation only; `generated` holds generator
  output; `src/miniport/ddi/` and `src/icd/d3d/ddi/` are record directories.
  Never reuse those names for anything else.
- **Moving files** is `git mv` plus every path the build and gates name
  (`docs/architecture.md`, "Paths the build and the gates name"), with the
  unit test moved to the mirrored path in the same change.

## C++ rules

**Everything is an object with state.** No static member functions. No class
that is only a namespace for static methods. No free-function dumping grounds.
If a group of functions threads the same handle, buffer, bounds or platform
through their parameter lists, that parameter is a member: make it a class and
inject its dependencies at construction. `Alignment` owns an alignment,
`ByteRange` owns a span, `GpuVirtualAddressSpace` owns a table - none of them
take what they own as a parameter.

The two exceptions are ABI boundaries that C requires, and they are one line
each: the `DxgkDdi*` entry points in `src/miniport/ddi/`, which forward to the
adapter behind the context pointer (or, for AddDevice and Unload, the root
registered in `MiniportBinding`), and the `extern "C"` entry points that
libFuzzer and QEMU call. No logic lives in either. `MiniportBinding`'s pointer,
the kernel shim's static storage for the root, the device module's
`DeviceModule`, the executor library's root behind `av_executor_ops()`, the
Vulkan ICD's root behind `vk_icdGetInstanceProcAddr` and `IcdBinding`'s pointer to
it, `scope_binding`'s pointer to the binary's trace table (with the device
module's `DeviceTraceRoot` beside `DeviceModule`), avtest.exe's
`ProcessTrace` in `src/um/avtest/program_entry.cpp`'s storage, and the OpenGL ICD's
root behind the `Drv*` exports (`src/um/opengl/avgl_exports.cpp`) and
`GlIcdBinding`'s pointer to it (`src/icd/opengl/gl_hooks.cpp`), and the D3D
user-mode driver's root behind `OpenAdapter10_2` and `OpenAdapter12`
(`src/um/d3d/avumd_exports.cpp`) with `UmdBinding`'s pointer to it
(`src/icd/d3d/umd_binding.cpp`), and av-renderer's `RendererProcess` behind its
`main` (`src/device/shim/renderer_main.cpp`) are the only
process-lifetime state (constitution III). Each is a C entry point with
nowhere to put a `this`, and each is one `ObjectStorage` in the single
translation unit defining those entry points.

Genuinely stateless operators and ADL hooks stay free functions. Nothing else
does.

**Object classes and system classes.** An object class is one instance per real
thing - `Adapter`, `Device`, `Context`, `Allocation`, `VirtualRange`,
`MonitoredFence`, `VirtQueue`. A system class is a long-lived service that owns
a collection and is injected where it is needed - `GpuVirtualAddressSpace`,
`Scheduler`, `PagingEngine`, `DisplayEngine`, `TimeoutDetector`. Both hold
state. A class with no members is a mistake being written down.

**Files.** Each class lives in the semantic folder for its area (`src/miniport/memory/`,
`src/miniport/display/`, ... - docs/architecture.md lists them), and its test mirrors that path under
`tests/unit/`. Includes name the path from `src/` or `tests/` -
`"miniport/display/edid.hpp"`, `"harness/vidsch/fake_vidsch.hpp"` - never a relative
one. One class per header, named after the class in snake_case; nested and
private helper types stay with their owner. A header declares, it does not
define - no function bodies inside a class declaration, not one-line getters,
not constructors. Templates go in the header because they must. Every header
compiles standalone and includes what it names. Forward declare wherever the
type is only a pointer, reference, or parameter/return; include it for a base
class, a by-value member, or `sizeof`. Never `using namespace` at header scope.

**Before writing a function, grep for it.** Same behavior under a different
name already in the repo is the default case, not the rare one. Reuse it.

**One responsibility per class**, statable in a sentence with no "and". A method
that touches no member of its own class belongs on another class. Parameters
that always travel together are a class.

**No speculative structure.** No interface with one implementation, no template
with one instantiation, no factory for one product, no config flag with one
setting, no virtual hook nothing overrides. `Platform` is virtual because it has
two real implementations - `KernelPlatform` and `MockPlatform` - and the mock is
what makes the constitution's testing rule possible.

**Dispatch**: known at compile time -> template or policy parameter. Chosen at
runtime or at construction -> virtual interface. Never a bool parameter
selecting behavior.

**Comments: default is none.** Write *why*, never *what* - a constraint, a
workaround with its link, a tradeoff, an invariant not in the type. No
restating the next line, no section banners, no Doxygen echoing parameter
names, no "Step 1/2/3" narration. Unclear code gets a better name or a split.
Never leave commented-out code; git has it.

**Every function opens with its trace scope**: `AV_SCOPE("Class::method");`
as the first line of every function definition in `src/`, naming the function
as written (docs/trace.md). `tools/scope_marks.py --apply` writes them;
`ctest -R scope_marks` fails a definition without one. The exceptions are the
one-line ABI entry points, `constexpr` functions, `src/core/trace/` itself,
every `TraceHost` and the clock it reads (a scope there recurses), and the
wire's per-member primitives in `src/proto/wire/` (they cannot block, and
their scopes tripled a traced decode); a mark in an exempt file fails too.

**Names**: classes `PascalCase`, methods and locals `snake_case`, private data
members trailing underscore. No `Manager`, `Helper`, `Utils`, `Handler`,
`Impl`, `data`, `info`, `result`, `temp`, `process()`, `handle()`, `doWork()`.
Name what it is or does. Magic numbers are named constants at the scope owning
them. Functions stay under ~50 lines and 3 nesting levels - guard clauses and
early return first, extract second.

**Errors are values.** Every fallible operation returns `av::Status`. There are
no exceptions in this codebase: the guest side is kernel-mode and the device
side decodes hostile input, so both build with `-fno-exceptions -fno-rtti`.
A malformed frame, a bad handle or an out-of-range address is a reachable state
and returns a status; it never asserts. Never swallow a failure - no ignored
return; `[[nodiscard]]` on anything returning `Status`. Cleanup is RAII via
`ScopeExit`, never repeated per branch, because every repeated cleanup path is
another branch the coverage gate makes you test.

**Ownership**: rule of zero is the target. Owning a raw resource means wrapping
it in a handle type; declaring one of the five means declaring all five
consistently. A polymorphic base gets a protected non-virtual destructor (see
Adaptations). Copy and assignment
are `= delete` unless copying is meaningful. Data members are private; expose
the operation callers actually perform, not a getter/setter pair. `const`,
`explicit`, `[[nodiscard]]` wherever they hold.

**Construction is two-phase where it must be.** Without exceptions a constructor
cannot fail, so a class that acquires a fallible resource has a trivial
constructor and an `initialize(...)` returning `Status`. Keep the fallible work
in `initialize`, not spread across both.

**Allocation**: there is no C++ standard library here. No `std::vector`,
`std::string`, `std::map`, `std::function`, no `new`/`delete`. Memory comes
from `Platform::allocate` with an explicit `Pool`, and the caller owns
releasing it. An object whose lifetime is not a scope lives in
`ObjectStorage<T>`; an object with no fixed bound on its count (a device,
an allocation) lives in `ObjectPool<T>`. Those two are the only places a
placement-new expression is allowed. A fixed maximum means a fixed-size member array, not a heap
allocation. Nothing allocates on the submission path.

**IRQL is part of the signature.** Every public method calls
`platform_.require_irql_at_most(...)` naming its ceiling. Anything reachable at
`Irql::dispatch` must not allocate, block, or take a mutex - and the mock
enforces all three, so a violation is a failing unit test rather than a
bugcheck in a VM.

**Tests**: see `docs/constitution.md`, Principle IV. Every branch,
invariants after every mutation, hostile input, and a fuzz target for anything
that reads bytes it did not write. Coverage is the floor, not the goal: a test
asserts the outcome a caller would care about, at the values that are actually
interesting, and must fail if a comparison is flipped or a line deleted. A test
that only walks a branch without judging what it produced is not a test. This is not aspirational; `tools/gate.sh`
runs every gate - `tools/check_design.py`, `tools/check_symbol_tests.py`, the
kernel cross-build, 100% branch coverage and fuzz smoke - and fails the build.

## Adaptations from the standard decrapify-cpp rules

Dropped, because this codebase is freestanding kernel-mode C++:

- **STL guidance** (`std::array`, `string_view`, `span`, `std::map` -> sorted
  vector, `.reserve()`, `.clear()` hoisting). There is no standard library.
  Replaced by the **Allocation** rule above.
- **Move-vs-copy guidance** (`std::move`, sink parameters by value, `noexcept`
  move constructors, NRVO). Nothing here owns heap memory through a value type
  yet, so this is speculative. Reinstate it if a container class appears.
- **`std::optional` / `std::expected` / throw** for errors. Replaced by
  `av::Status`, since neither type is available and exceptions are off.

Changed, because the kernel has no heap operators:

- **Virtual destructor on a polymorphic base** becomes a `protected`
  non-virtual destructor. A virtual destructor makes the compiler emit a
  deleting destructor that calls `operator delete`, which does not exist in the
  kernel, so the driver fails to link. No object here is ever destroyed through
  a base pointer, and `protected` makes trying a compile error.

Changed, because the DDI boundary is C structures:

- **Private data members** do not apply to DDI records: the structs named
  `*Record` in a DDI record directory, which overlay memory their caller owns.
  `src/miniport/ddi/` holds dxgkrnl's (`DXGKRNL_INTERFACE`,
  `DXGK_CHILD_DESCRIPTOR`, ...), proven in `src/km/ddi_record_layout.cpp`;
  `src/icd/d3d/ddi/` holds the Direct3D runtimes'
  (`D3D10DDIARG_OPENADAPTER`, `DXGI1_6_1_DDI_BASE_FUNCTIONS`, ...), proven in
  `src/um/d3d/umd_record_layout.cpp`. Their fields are public and keep the
  WDK's names so they grep to the header. A record declares no member
  functions; `tools/check_design.py` enforces both, and
  `tools/header_scan.py`'s `RECORD_DIRECTORIES` is the list.

Tightened, at the project's request:

- **No static methods at all**, and no stateless classes. The standard rules
  allow stateless free functions; this repo allows only operators and ADL hooks.
