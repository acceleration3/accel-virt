# Implementation Plan: Build and test foundation

**Branch**: `001-build-and-test-foundation` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/001-build-and-test-foundation/spec.md`

## Summary

Close the gap between what the constitution says the build enforces and what
it actually enforces. The host build gains a second compilation of `src/core`
for the Windows kernel (clang-cl, `/kernel`, no hosted headers), linked into
`rvkgpu.sys` and inspected; that immediately forces core off `<cstdint>`.
Coverage, the `AV_UNTESTABLE` ratchet and fuzz smoke move inside `ctest`,
across three build configurations driven by one committed command. The
design gate splits so `tools/check_symbol_tests.py` exists as documented. The
repository becomes self-contained: the Windows kit is fetched by pinned hash
into a git-ignored directory, the knowledge base and RE scripts move in, and a
gate keeps external paths out.

## Technical Context

**Language/Version**: C++23 freestanding subset (`-fno-exceptions -fno-rtti`,
no standard library) for `src/`; CMake 3.24+ for the build; Python 3 and POSIX
shell for gates and fetch scripts.

**Primary Dependencies**: clang 22 / clang-cl / lld-link / llvm-lib /
llvm-readobj / llvm-cov / llvm-profdata; Ninja; WDK `10.0.26100.6584` and SDK
`10.0.26100.9169` NuGet packages (fetched, research R11); Ghidra 12.1.2 +
pyghidra for the optional RE workspace.

**Storage**: files only - `third_party/windows-kit/`, `re/`, `build*/`, all
git-ignored.

**Testing**: `ctest` in each of three configurations; `tools/gate.sh` runs all
three.

**Target Platform**: Linux x86-64 host (tests, device); Windows 11 x64 kernel
(driver image).

**Project Type**: kernel-mode driver + in-process device library + shared
protocol, one source tree.

**Performance Goals**: default-configuration `ctest` under 60 s after the
first build (SC-003).

**Constraints**: no Microsoft-licensed file committed; no tracked path outside
the repository; kernel build must not see any hosted header.

**Scale/Scope**: 3 core sources today, 35 features ahead; ~960 kit headers; 10
RE binaries.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-checked after Phase 1 design.*

| Principle | Status | Note |
|---|---|---|
| I. Spec precedes implementation | Pass | Spec filled, clarified (6 answers), checklist green |
| II. Portable core, translation-only shim | Pass | `src/km/driver_entry.cpp` is one statement, no branch. Core compiles for both targets with no host/kernel conditional (FR-004) |
| III. Object-oriented, with state | Pass | No new C++ class, static, or free function. `DriverEntry` is the documented `extern "C"` ABI exception |
| IV. Test-first and exhaustive | Pass | Every gate has a demonstrated failing case (quickstart section 3) written before its implementation. No new code in `src/core` beyond header changes, so coverage stays 100% |
| V. Everything from outside is hostile | Pass | Kit packages and RE binaries verified by SHA-256 before use |
| VI. Levels are phases | Pass | `DriverEntry` refuses to load; nothing is reported |
| VII. IRQL typed | N/A | No new function in core |
| VIII. No speculative structure | Pass | No device target without device sources; no release configuration without a release; `coverage.sh` deleted, not kept beside `gate.sh` |
| IX. Reproducible and observable | Pass, and the point | One fetch command plus one gate command from a clean clone; every gate prints the value that failed it |
| Technology constraints | Pass | clang only; CMake + Ninja; ctest the entry point; no dependency added to `src/core` or `src/km` |

Known inconsistencies in the constitution itself, **not** fixed here (recorded
for a PATCH amendment): Principle II says "freestanding C11"; IV.2 names
`bool <module>_invariants(const <module>_t *)` where the code uses
`invariants_hold()`; VII names `src/core/locks.h` where `docs/testing.md` says
`lock_order.hpp` and neither exists.

**Post-design re-check**: unchanged. Phase 1 added no class, no interface and
no configuration setting beyond the three build configurations the
clarification chose.

## Classes this feature introduces

Filled into the spec's table. The table lives in the generated header block;
`tools/gen_specs.py` carries filled rows across regeneration (FR-019, T005):

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| *none* | - | - | This feature adds build rules, gate scripts, a kit fetch, and one `extern "C"` ABI entry point (`DriverEntry`). A C++ class here would own nothing. |

## Project Structure

### Documentation (this feature)

```text
specs/001-build-and-test-foundation/
├── plan.md              # this file
├── research.md          # R1-R15, each verified on this machine
├── data-model.md        # artifacts, pins, gates and their validity rules
├── quickstart.md        # clean-clone run, fast loop, make-every-gate-fail table
├── contracts/
│   └── commands.md      # gate.sh, fetch, gate scripts, configure settings, DriverEntry
├── checklists/
│   └── requirements.md
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
CMakeLists.txt                       host targets, gates, kit check, corpus check,
                                     rvkgpu ExternalProject, coverage fixtures
cmake/
├── windows_kernel_toolchain.cmake   clang-cl + lld-link for the kernel build (R6)
├── windows_kernel_flags.cmake       kernel flags, applied after CMake's MSVC defaults (R6)
└── check_fuzz_corpora.cmake         every fuzz target has seeds (R8)
src/
├── core/                            <stdint.h>/<stddef.h>, std:: dropped (R1)
├── km/
│   ├── CMakeLists.txt               driver sub-project: globs ../core and km (R6)
│   └── driver_entry.cpp             extern "C" DriverEntry -> STATUS_NOT_SUPPORTED
├── device/                          (empty; the rule exists, the target arrives with 026)
└── proto/                           (empty)
tests/
├── untestable_baseline.txt          0 (R10)
├── harness/test_main.cpp            main(), unit tests only (libFuzzer brings its own)
├── tools/                           tests for every gate script, run as tools_tests
└── ...                              unchanged
tools/
├── gate.sh                          one command, three configurations (R8)
├── fetch_windows_kit.sh             pinned NuGet fetch, SHA-256, subset, overlay (R11, R3)
├── windows_kit_overlay.py           writes overlay.yaml
├── header_scan.py                   shared header parser (R9)
├── check_design.py                  Principle III only
├── check_symbol_tests.py            Principle IV.5 (new)
├── check_self_contained.py          FR-016 (R14)
├── check_driver_image.py            subsystem + import allow-list (R5)
├── check_coverage.py                branch % + ratchet (R8, R10)
├── gen_specs.py                     preserves the classes table; in-repo references (R15)
├── coverage.sh                      deleted
└── re/
    ├── binaries.txt                 pinned RE inputs (R12)
    ├── build_workspace.sh
    └── msfetch.py import.py q.py pdbsyms.py ddi_extract.py
docs/
├── kb/                              01-...12-*.md, README.md (R13)
└── ref/                             + ddi_by_version.json
third_party/windows-kit/             git-ignored, fetched
re/                                  git-ignored, built
```

**Structure Decision**: one repository, one CMake entry point. The kernel
build is a sub-project because it needs a different compiler; it is not a
second place to add sources - both builds glob the same `src/core`.

## Documentation this change makes wrong, fixed in the same change

`README.md` (build commands, layout, KB and codec links), `CLAUDE.md` (gate
names, codec path), `docs/testing.md` (Ghidra path, `lock_order.hpp` is left
as is - constitution amendment), `.claude/commands/dev.md` (every KB path,
`coverage.sh`, Ghidra commands), `src/core/gpu_virtual_address_space.hpp`
(KB comment), `tools/gen_specs.py` output in all 35 specs.

## Complexity Tracking

| Addition | Why needed | Simpler alternative rejected because |
|---|---|---|
| CMake `ExternalProject` for the kernel build | one CMake project cannot hold two C++ compilers | a separate configure only in `gate.sh` leaves `cmake --build build` unable to catch a kernel-only break (R6) |
| Generated VFS overlay over kit headers | kit headers include each other with the wrong case | renaming or symlinking kit files is incomplete and mutates vendor content (R3) |
| Three build configurations | ASan, profile instrumentation and libFuzzer cannot share one set of objects | one directory needs ExternalProject sub-builds of itself and makes every run minutes long (clarification) |
