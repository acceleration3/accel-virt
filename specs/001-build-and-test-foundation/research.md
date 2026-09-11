# Research: Build and test foundation

Every decision here was checked by running it on this machine (clang 22.1.8,
CMake, Ninja, lld-link, llvm-readobj) on 2026-09-11, not by reading about it.

## R1. Integer and size types in core

**Decision**: `src/core` includes `<stdint.h>` and `<stddef.h>` and uses
`uint64_t`, `uint32_t`, `size_t` unqualified. Tests and harness keep
`<cstdint>`; the types are identical.

**Rationale**: `<cstdint>` and `<cstddef>` are C++ library headers. The
kernel kit ships `km/crt/cstddef` but no `cstdint`, and no C++ library
exists in the kernel. `<stdint.h>` resolves to clang's builtin header when no
hosted one is on the path (`/X` removes it), and `<stddef.h>` to the kit's
`km/crt/stddef.h`. All three core sources compiled clean with `/W4 /WX
-Wextra -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast` after the
change; before it they failed with `'cstdint' file not found`.

**Alternatives**: a project `av_types.hpp` re-exporting the types (a header
with no purpose but to rename; rejected); `-ffreestanding` (not accepted by
clang-cl directly, and unnecessary once `/X` is set).

## R2. Kernel compile flags

**Decision**: `clang-cl --target=x86_64-pc-windows-msvc /std:c++latest /kernel
/GR- /EHs-c- /Zl /X /D_AMD64_ /DAMD64 /D_WIN64 /W4 /WX -Wextra -Wconversion
-Wsign-conversion -Wshadow -Wold-style-cast`, with `/GS` left at its default.

**Rationale**: `/kernel` sets the kernel code model and rejects constructs the
kernel cannot run; `/GR-` and `/EHs-c-` are the project's `-fno-rtti
-fno-exceptions`; `/Zl` omits default CRT library names from objects; `/X`
drops every system include path so a hosted header cannot be found by
accident. Without `_AMD64_` `ntdef.h` stops with `"No Target Architecture"`.
Under clang-cl `-Wall` means `-Weverything` (it failed on "`long long` is
incompatible with C++98"), so warnings are `/W4` plus the project's explicit
`-W` list.

Verified negative cases, each a hard error:

| Probe | Result |
|---|---|
| `#include <vector>` | `fatal error: 'vector' file not found` |
| `throw 1;` | `cannot use 'throw' with exceptions disabled` |
| `typeid(a)` | error, no `<typeinfo>` |
| `new int(3)` | link: `undefined symbol: operator new(unsigned __int64)` |
| call to a non-kernel C function | link: `undefined symbol: strnlen_not_in_kernel` |

The last two fire from an object file whose code nothing references, with
`/OPT:REF` on: lld-link resolves every symbol of every object given on the
command line before discarding sections. So the link is a complete check of
core even before any DDI calls into it.

## R3. Kit headers on a case-sensitive filesystem

**Decision**: The fetch step writes a clang VFS overlay (`overlay.yaml`,
`case-sensitive: false`, `overlay-relative: true`, `use-external-names:
false`) that lists every kit header as an explicit file entry under virtual
roots `/windows-kit/km`, `/windows-kit/wdk-shared`, `/windows-kit/sdk-shared`.
The compiler gets `/clang:-ivfsoverlay<kit>/overlay.yaml` and
`-imsvc/windows-kit/...`.

**Rationale**: The SDK's `kernelspecs.h` includes `"DriverSpecs.h"`; the file
is `driverspecs.h`. Findings, in order:

1. A `directory-remap` overlay does not help: lookups inside it fall through
   to the real, case-sensitive filesystem.
2. With `use-external-names: true` (the default), a quoted include is searched
   next to the *real* path of the includer, outside the overlay.
3. Explicit per-file entries with `use-external-names: false` resolve every
   include. 962 files, 180 KB of JSON, and `overlay-relative` keeps it valid
   if the repository moves.

**Alternatives**: lower-casing the extracted kit (breaks includes written in
mixed case the other way); a symlink per case variant (needs a full include
scan, and misses future includes); `ciopfs` (a FUSE mount - external
dependency, root setup).

## R4. Linking the driver image

**Decision**: `lld-link /DRIVER /SUBSYSTEM:NATIVE /ENTRY:GsDriverEntry
/NODEFAULTLIB /MANIFEST:NO /OPT:REF`, linking `ntoskrnl.lib hal.lib
BufferOverflowK.lib displib.lib` from the kit.

**Rationale**: `GsDriverEntry` in `BufferOverflowK.lib` initialises the `/GS`
security cookie and then calls `DriverEntry`, exactly as the WDK's own build
does. The resulting image: `IMAGE_FILE_MACHINE_AMD64`,
`IMAGE_SUBSYSTEM_NATIVE`, imports only `ntoskrnl.exe` (`KeBugCheckEx`, the
`/GS` failure path). `displib.lib` supplies `DxgkInitialize` from feature 006;
unreferenced until then, it costs nothing.

## R5. The driver image check

**Decision**: `tools/check_driver_image.py` runs `llvm-readobj --file-headers
--coff-imports` and fails unless the machine is AMD64, the subsystem is
native, and every imported module is in `{ntoskrnl.exe, hal.dll}`
(case-insensitive). Every violation is printed with its value.

**Rationale**: Link success proves symbols resolved, not that they resolved
to the kernel: a stray user-mode import library on the link line would link
and then fail to load. The allow-list is the kernel-provided modules the
driver is permitted to bind to. `llvm-nm --undefined-only displib.lib` shows
its imports are `ntoskrnl` exports (`IoGetDeviceObjectPointer`,
`IoBuildDeviceIoControlRequest`, `ExAllocatePoolWithTag`, ...), so feature 006
should not need to widen it. `displib` also references the control-flow-guard
symbols (`_guard_dispatch_icall`, `__guard_check_icall_fptr`); where those
resolve is feature 006's link to discover, and this gate will report it.

## R6. Two compilers, one source tree

**Decision**: The host build adds the driver as a CMake `ExternalProject`
whose source directory is `src/km/`, configured with
`cmake/windows_kernel_toolchain.cmake`, `BUILD_ALWAYS ON`. `src/km/CMakeLists.txt`
globs `../core/*.cpp` and `*.cpp` with `CONFIGURE_DEPENDS`.

**Rationale**: One CMake project cannot hold two C++ compilers. Prototyped:
CMake's Windows platform module works for clang-cl on Linux given
`CMAKE_SYSTEM_NAME Windows`, `CMAKE_LINKER lld-link`, `CMAKE_AR llvm-lib`,
`CMAKE_RC_COMPILER llvm-rc`, `CMAKE_MT llvm-mt`, `CMAKE_TRY_COMPILE_TARGET_TYPE
STATIC_LIBRARY` (compiler detection cannot link a user-mode exe without a
CRT), and an empty `CMAKE_CXX_STANDARD_LIBRARIES`. Three traps found:

- CMake de-duplicates repeated option words, turning `-imsvc a -imsvc b` into
  `-imsvc a b`. Use the joined form `-imsvc<dir>`.
- The MSVC platform module appends `/RTC1` (needs the CRT), `/EHsc`,
  `/DWIN32` and `kernel32.lib` to the `*_INIT` variables *after* the
  toolchain file runs, so the kernel flags live in
  `cmake/windows_kernel_flags.cmake`, loaded through
  `CMAKE_USER_MAKE_RULES_OVERRIDE_CXX`, which runs later.
- CMake links every executable with `CMAKE_CXX_CREATE_CONSOLE_EXE`
  (`/subsystem:console`) after the linker flags, silently overriding
  `/SUBSYSTEM:NATIVE`. The override clears it. Found by the `driver_image`
  gate's own test on its first run.

`cmake --build build` therefore compiles core twice - host and kernel - and a
kernel-only failure fails the build, which fails the entry point.

**Alternatives**: hand-written `add_custom_command` per source (reimplements
dependency scanning); a separate top-level configure step in `tools/gate.sh`
only (then plain `cmake --build build` no longer proves the kernel build, the
exact gap this feature exists to close).

## R7. Checked vs release in the driver

**Decision**: The driver sub-build defines `AV_CHECKED=1`, like the host
build.

**Rationale**: Constitution IV.2 wants invariants checked on entry and exit in
debug builds, and every driver built before feature 035 is a development
build. A release configuration is a second setting with no user yet
(Principle VIII); it arrives when a release is cut.

## R8. Where each gate runs

| Configuration | Directory | Adds | Gates in its `ctest` |
|---|---|---|---|
| default | `build/` | ASan + UBSan | unit tests, `design_rules`, `symbol_tests`, `driver_image`, `self_contained`, `tools_tests` |
| coverage | `build-coverage/` | `-fprofile-instr-generate -fcoverage-mapping` | the above, plus `coverage_gate` (branch % and `AV_UNTESTABLE` ratchet) |
| fuzz | `build-fuzz/` | `-fsanitize=fuzzer-no-link` on libraries, `fuzzer` on targets | the above, plus `fuzz_<target>_smoke` |

`tools/gate.sh` configures, builds and runs `ctest` in all three, stopping at
the first failing configuration. `tools/coverage.sh` is deleted; its job is
the coverage row.

**Coverage inside ctest**: CMake test fixtures order the steps. A
`coverage_clean` test (`FIXTURES_SETUP coverage_clean`) deletes old profiles;
every unit test `FIXTURES_REQUIRED coverage_clean` and `FIXTURES_SETUP
coverage_profiles`; `coverage_gate` `FIXTURES_REQUIRED coverage_profiles`. A
failing unit test still fails ctest, and the gate reports any test binary with
no `.profraw` as a failure rather than computing a percentage over fewer
binaries.

**Unlinked sources**: a `.cpp` under `src/` that no test references is dropped by the linker and never appears in `llvm-cov` output, so it would pass unmeasured. The gate lists every such file (`not linked into any test binary`). Found while verifying the device build rule.

**Coverage measurement**: `llvm-cov export` JSON, summed over files whose path
is under `src/core`, `src/device` or `src/proto` - `.cpp` and `.hpp` both. The
old script's `find -name "*.c"` matched nothing, so its `AV_UNTESTABLE` count
was always 0. Uncovered branches are printed as `file:line:col`.

**Fuzz linking**: `main` moves out of the harness into `tests/harness/framework/test_main.cpp`, linked only into unit tests. While it lived in `test_registry.cpp` the fuzz build failed to link (`multiple definition of 'main'` against libFuzzer's), so the fuzz command in the README had never worked. Smoke runs write new units to `build-fuzz/corpus/<target>/` and read the committed corpus, which stays unchanged. In the fuzz configuration `av_core` and `av_harness` are
compiled with `-fsanitize=fuzzer-no-link,address,undefined`; each fuzz target
links them. The hand-copied source list goes away.

**Corpus rule**: at configure time, every `tests/fuzz/fuzz_<name>.cpp` must
have a non-empty `tests/corpus/fuzz_<name>/`, in every configuration, or
configuration fails naming the target.

## R9. Design gate and symbol-test gate

**Decision**: Split `tools/check_design.py` into `check_design.py` (Principle
III: static member functions, classes with no data members) and
`check_symbol_tests.py` (Principle IV.5: public class or method no test
names). Both import the header scan from `tools/header_scan.py`.

**Rationale**: The constitution, `CLAUDE.md` and all 35 specs name
`tools/check_symbol_tests.py`. Two rules, two responsibilities, two failure
messages; one parser.

## R10. The ratchet

**Decision**: `tests/untestable_baseline.txt` holds one integer (today `0`).
`coverage_gate` counts `AV_UNTESTABLE(` in `src/`; above the baseline it fails
listing every mark with file, line and reason; below it passes and prints
"lower the baseline to N".

## R11. Kit acquisition

**Decision**: `tools/fetch_windows_kit.sh` downloads from
`api.nuget.org/v3-flatcontainer`:

| Package | Version | Size | SHA-256 |
|---|---|--:|---|
| `Microsoft.Windows.WDK.x64` | 10.0.26100.6584 | 110,872,506 | `c393d03dfb640b5c92f546b32f6770ef68cd3aaf691956e7d66d8e2c28a1b55e` |
| `Microsoft.Windows.SDK.CPP` | 10.0.26100.9169 | 160,512,239 | `475269434dcd808a67853773272f972c3229c0e10c3ddc821290e70cc0f6904d` |

It verifies each hash, extracts into a temporary directory, and moves the
result into `third_party/windows-kit/` only when complete:

```
third_party/windows-kit/
  include/km/          WDK c/Include/10.0.26100.0/km      (649 files, incl. km/crt)
  include/wdk-shared/  WDK c/Include/10.0.26100.0/shared  (10)
  include/sdk-shared/  SDK c/Include/10.0.26100.0/shared  (303)
  lib/x64/             ntoskrnl.lib hal.lib displib.lib BufferOverflowK.lib
  overlay.yaml         R3
  LICENSE.txt          the WDK licence, kept beside what it governs
  VERSION              the two package ids and versions
```

33 MB on disk. Verified: core, `DriverEntry` and a `dispmprt.h` +
`DXGKDDI_INTERFACE_VERSION_WDDM3_2` probe all compile from this subset alone.
Downloads are cached in `third_party/.cache/` so a re-fetch is offline.

**Rationale**: The WDK licence grants use for developing drivers, not
redistribution, so the kit stays out of git (clarification). Pinning by hash
makes the fetch reproducible and tamper-evident.

## R12. Reverse-engineering workspace

**Decision**: Copy `msfetch.py`, `import.py`, `q.py`, `pdbsyms.py`,
`ddi_extract.py` into `tools/re/`. Add `tools/re/binaries.txt` (file name,
SHA-256) and `tools/re/build_workspace.sh`, which fetches each pinned binary
and PDB into `re/bin/`, and imports them into `re/ghidra-proj/` (project name
`wddm`). `msfetch.py` gains selection by SHA-256 (winbindex already keys its
index by SHA-256).

Pinned set - the binaries the knowledge base was derived from:

| File | Version | SHA-256 |
|---|---|---|
| `dxgkrnl.sys` | 10.0.26100.9444 | `ab420f6672308ead7fae9698bd7a5597a4f32213f32766b8e78c49210d6ad6b2` |
| `dxgmms2.sys` | 10.0.26100.9444 | `c45c5a50ff293324a4a398766c9b5acd27c149a5954b5d09f128758b30ce72fa` |
| `basicrender.sys` | 10.0.26100.8972 | `cbac7c00ac64c28fa56b735ab4065c9d95fcb992f84b00100309f8737daa819d` |
| `basicdisplay.sys` | 10.0.26100.8521 | `75ab8fd5c899616e70b6252b6c7291463f7750de5866fbb5b6f8b0d21daf3eea` |
| `vkrnlintvsp.sys` | 10.0.26100.8972 | `708180011a660c546e9c320895a5a1909e992325cc2d676e5058a997ee3f28c8` |
| `vpci.sys` | 10.0.26100.8972 | `5582d06d0cae7c1e2d4df659824ec9e7434c64b3f17738ef723c82db08f389ec` |
| `d3d12core.dll` | 10.0.26100.9278 | `70710de807f188a85a3f5ad3a729a41bfa7a92b7621ceeaf38059f5640399267` |
| `d3d12.dll` | 10.0.26100.9278 | `6b32181599d4d6373f0b49905236b122ed5545c5d061ab4350a80cc095dc15e4` |
| `dxcore.dll` | 10.0.26100.9278 | `7dbabbd215d1e93ba8904540033d3bb71dc795c0c58371c8905583e105311b3c` |
| `dxgi.dll` | - | `f777789c1e5d7a64f3fcfebbda9623df5c0cf8b178c89c78974aa0b5fbeaac30` |

`get_wdk_libs.sh` is not copied; `fetch_windows_kit.sh` replaces it.

## R13. Knowledge base documents

**Decision**: `kb/*.md` move to `docs/kb/`; `kb/ref/ddi_by_version.json` joins
the two `docs/ref/` files already here. Paths inside the documents are
rewritten: `headers/wdk-26100/...` becomes `third_party/windows-kit/include/...`,
`tools/X.py` becomes `tools/re/X.py`, `ghidra-proj` becomes `re/ghidra-proj`,
`bin/msft` becomes `re/bin`, `pdb/` becomes `re/bin` (PDBs sit beside their
binary), `kb/ref/` becomes `docs/ref/`. The `headers/wdk-28000` reference in
the cookbook (diffing the next WDK) is reworded to fetch that version with a
changed pin, not to a directory that does not exist.

## R14. The self-containment gate

**Decision**: `tools/check_self_contained.py` scans `git ls-files` for
the external knowledge-base checkout by name, home-directory paths, and any relative path that
climbs above the repository root, or `/home/`, outside `.gitignore`. Run as
the `self_contained` ctest.

**Rationale**: FR-016 is otherwise a one-time cleanup that drifts back the
next time a document is pasted in.

## R15. Preserving the classes table

**Decision**: `tools/gen_specs.py` reads the existing "Classes this feature
introduces" rows from a spec before replacing the autogen block and writes
them back; only an unfilled placeholder row is regenerated.

**Rationale**: The table is inside the generated block. Without this, the
first regeneration after a `/speckit-plan` erases the plan's class decisions -
it would erase this feature's the moment `gen_specs.py` runs.
