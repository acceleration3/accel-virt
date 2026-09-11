<!-- decrapify-cpp: rules for writing new C++ in this repo. Same standard the
     12 cleanup passes enforce, applied before the code exists.
     Adapted for freestanding kernel-mode C++ - see "Adaptations" at the end. -->

# accel-virt

A paravirtual GPU for Windows guests on QEMU/KVM: a WDDM 3.2 display miniport
(`rvkgpu.sys`), a custom in-process QEMU device (`virtio-gpu-rvk-pci`), and the
wire protocol between them. Serialization comes from `render-server`'s
`librvk_codec`, an external C-ABI library found through pkg-config.

**Read before writing code:** `.specify/memory/constitution.md` (the rules that
govern the project), `docs/testing.md` (how a display miniport is unit tested),
`docs/roadmap.md` (what to build, in order).

## C++ rules

**Everything is an object with state.** No static member functions. No class
that is only a namespace for static methods. No free-function dumping grounds.
If a group of functions threads the same handle, buffer, bounds or platform
through their parameter lists, that parameter is a member: make it a class and
inject its dependencies at construction. `Alignment` owns an alignment,
`ByteRange` owns a span, `GpuVirtualAddressSpace` owns a table - none of them
take what they own as a parameter.

The two exceptions are ABI boundaries that C requires, and they are one line
each: the `DxgkDdi*` entry points in `src/core/ddi/`, which forward to the
adapter behind the context pointer (or, for AddDevice and Unload, the root
registered in `MiniportBinding`), and the `extern "C"` entry points that
libFuzzer and QEMU call. No logic lives in either. `MiniportBinding`'s pointer
and the kernel shim's static storage for the root are the only process-lifetime
state (constitution III).

Genuinely stateless operators and ADL hooks stay free functions. Nothing else
does.

**Object classes and system classes.** An object class is one instance per real
thing - `Adapter`, `Device`, `Context`, `Allocation`, `VirtualRange`,
`MonitoredFence`, `VirtQueue`. A system class is a long-lived service that owns
a collection and is injected where it is needed - `GpuVirtualAddressSpace`,
`Scheduler`, `PagingEngine`, `DisplayEngine`, `TimeoutDetector`. Both hold
state. A class with no members is a mistake being written down.

**Files.** Each class lives in the semantic folder for its area (`src/core/memory/`,
`src/core/display/`, ... - README.md lists them), and its test mirrors that path under
`tests/unit/`. Includes name the path from `src/` or `tests/` -
`"core/display/edid.hpp"`, `"harness/vidsch/fake_vidsch.hpp"` - never a relative
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

**Tests**: see `.specify/memory/constitution.md`, Principle IV. Every branch,
invariants after every mutation, hostile input, and a fuzz target for anything
that reads bytes it did not write. This is not aspirational; `tools/gate.sh`
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

- **Private data members** do not apply to DDI records: the structs in
  `src/core/ddi/` named `*Record` that overlay memory dxgkrnl owns
  (`DXGKRNL_INTERFACE`, `DXGK_CHILD_DESCRIPTOR`, ...). Their fields are public,
  keep the WDK's names so they grep to the header, and every offset is proven in
  `src/km/ddi_record_layout.cpp`. A record declares no member functions;
  `tools/check_design.py` enforces both.

Tightened, at the project's request:

- **No static methods at all**, and no stateless classes. The standard rules
  allow stateless free functions; this repo allows only operators and ADL hooks.
