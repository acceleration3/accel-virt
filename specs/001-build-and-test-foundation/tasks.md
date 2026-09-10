---
description: "Task list for feature 001: build and test foundation"
---

# Tasks: Build and test foundation

**Input**: `specs/001-build-and-test-foundation/` - plan.md, spec.md, research.md, data-model.md, contracts/commands.md, quickstart.md

**Tests**: Required. Constitution IV: tests are written first and must fail
before the implementation makes them pass. For this feature the "code under
test" is gates, so every gate gets an automated test that feeds it a violating
fixture and asserts the exact failure line from `contracts/commands.md`, plus
the passing case against the real repository. Python tool tests live in
`tests/tools/test_*.py` and run as the `tools_tests` ctest.

**Standing constraint**: do not run `tools/gen_specs.py` before T005 is done.
The spec's filled "Classes this feature introduces" table is inside the
generated block and would be erased.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: parallelisable (different files, no dependency on an incomplete task)
- **[Story]**: US1-US5 from spec.md

---

## Phase 1: Setup

- [X] T001 Add `__pycache__/`, `third_party/windows-kit/`, `third_party/.cache/` and `re/` to `.gitignore`, and untrack the committed bytecode with `git rm --cached -r tools/__pycache__`
- [X] T002 Register a `tools_tests` ctest in `CMakeLists.txt` running `python3 -m unittest discover -s ${CMAKE_SOURCE_DIR}/tests/tools -t ${CMAKE_SOURCE_DIR}` and create `tests/tools/__init__.py`; confirm `ctest --test-dir build -R tools_tests` runs (with zero tests unittest exits 5, `NO TESTS RAN`, and the ctest fails - kept, so broken discovery can never pass)

---

## Phase 2: Foundational (blocks every story)

**Purpose**: spec regeneration that cannot erase plan decisions (FR-019), and the Windows kit every kernel-side task needs (FR-015).

- [X] T003 [P] Write `tests/tools/test_gen_specs.py`: build a temporary root with `tools/features.py`-shaped input and `docs/ref/driver_init_table.md`, a `specs/001-*/spec.md` whose classes table has a filled row (`| Foo | object | a_ | b |`) and one spec with the placeholder row `| | object / system | | |`; run `tools/gen_specs.py --root <tmp>`; assert the filled row survives verbatim, the placeholder spec still has only the placeholder, and prose below `<!-- AUTOGEN:END -->` is unchanged. Must fail now (`--root` does not exist)
- [X] T004 [P] Write `tests/tools/test_windows_kit_overlay.py`: fixture tree `include/km/Main.h` containing `#include "driverSPECS.h"` and `include/sdk-shared/driverspecs.h` defining a macro; run `tools/windows_kit_overlay.py <fixture>`; assert `overlay.yaml` has `"case-sensitive": "false"`, `"overlay-relative": true`, `"use-external-names": false`, roots `/windows-kit/km`, `/windows-kit/wdk-shared`, `/windows-kit/sdk-shared` (missing root directories are skipped, not errors) with every file as an explicit `file` entry and relative `external-contents`; then compile a probe with `clang++ -fsyntax-only -ivfsoverlay <overlay> -I/windows-kit/km -I/windows-kit/sdk-shared` that includes `Main.h` and uses the macro, and assert success. Must fail now
- [X] T005 Implement `--root` and classes-table preservation in `tools/gen_specs.py` (research R15): before replacing the autogen block, capture the rows under `### Classes this feature introduces` that are not the placeholder `| | object / system | | |`, and write them back in place of the placeholder in the regenerated block. T003 passes. Then run `python3 tools/gen_specs.py` and confirm `git diff specs/` shows no change to `specs/001-build-and-test-foundation/spec.md`
- [X] T006 Implement `tools/windows_kit_overlay.py <kit dir>` writing `<kit dir>/overlay.yaml` per research R3. T004 passes
- [X] T007 Implement `tools/fetch_windows_kit.sh` per research R11 and `contracts/commands.md`: pins `Microsoft.Windows.WDK.x64 10.0.26100.6584` sha256 `c393d03dfb640b5c92f546b32f6770ef68cd3aaf691956e7d66d8e2c28a1b55e` and `Microsoft.Windows.SDK.CPP 10.0.26100.9169` sha256 `475269434dcd808a67853773272f972c3229c0e10c3ddc821290e70cc0f6904d` from `https://api.nuget.org/v3-flatcontainer/<id-lowercase>/<version>/<id-lowercase>.<version>.nupkg`; caches downloads in `third_party/.cache/`; verifies SHA-256 of cached and fresh files (mismatch: print both hashes, delete the file, exit 1); extracts WDK `c/Include/10.0.26100.0/km` -> `include/km`, WDK `c/Include/10.0.26100.0/shared` -> `include/wdk-shared`, SDK `c/Include/10.0.26100.0/shared` -> `include/sdk-shared`, WDK `c/Lib/10.0.26100.0/km/x64/{ntoskrnl,hal,displib,BufferOverflowK}.lib` -> `lib/x64`, WDK `LICENSE.txt`; writes `VERSION` (two `id version` lines); runs `tools/windows_kit_overlay.py`; builds in a temporary directory and renames over `third_party/windows-kit/` only on success; exits 0 without network when `VERSION` already matches
- [X] T008 Run `tools/fetch_windows_kit.sh`; confirm `third_party/windows-kit/` is about 33 MB with the layout in data-model.md; corrupt one byte of `third_party/.cache/*wdk*.nupkg`, delete `third_party/windows-kit/VERSION`, re-run and confirm it fails naming both hashes and leaves the kit directory intact; re-run to restore

**Checkpoint**: kit present, spec regeneration safe.

---

## Phase 3: User Story 1 - A green run means the core runs in the guest kernel (P1) MVP

**Goal**: `cmake --build build` compiles `src/core` for the Windows kernel and links `rvkgpu.sys`; `ctest` verifies the image.

**Independent test**: add `#include <vector>` to `src/core/alignment.hpp` - the build fails naming `vector`; revert - `driver_image` passes.

### Tests first

- [X] T009 [P] [US1] Write `tests/tools/test_kernel_build.py`: for each probe, write a throwaway CMake project into a temp dir (`add_executable(probe probe.cpp)`, `SUFFIX .sys`), configure it with `-DCMAKE_TOOLCHAIN_FILE=cmake/windows_kernel_toolchain.cmake -DAV_WINDOWS_KIT_DIR=third_party/windows-kit`, build, and assert: a probe with `#include <vector>` fails with output containing `'vector' file not found`; a probe calling `fopen` declared by hand fails containing `undefined symbol: fopen` (not `strlen`: `ntoskrnl` exports it); a probe using `typeid` is rejected; a probe doing `new int(1)` fails containing `operator new`; a probe with `throw 1;` fails containing `exceptions disabled`; a probe that is `#include <ntddk.h>` plus `#include <dispmprt.h>` plus a one-statement `DriverEntry` builds. Skip with a clear message only if `third_party/windows-kit/VERSION` is absent - configure is where that fails (T016). Must fail now (no toolchain file)
- [X] T010 [P] [US1] Write `tests/tools/test_check_driver_image.py`: using the T009 passing probe as the good image, assert `tools/check_driver_image.py <good>` prints `driver_image: ok`; build a bad image with `llvm-dlltool -m i386:x86-64 -D KERNEL32.dll -d <def with ExitProcess>` plus `lld-link /SUBSYSTEM:CONSOLE /ENTRY:main /NODEFAULTLIB` and assert exit 1 with lines `subsystem IMAGE_SUBSYSTEM_WINDOWS_CUI, want IMAGE_SUBSYSTEM_NATIVE` and `imports KERNEL32.dll, not a kernel module`; assert a missing file exits 1 naming the path. Must fail now

### Implementation

- [X] T011 [US1] Replace `#include <cstdint>`/`<cstddef>` with `<stdint.h>`/`<stddef.h>` and drop the `std::` qualifier from `uint8_t`/`uint32_t`/`uint64_t`/`size_t` in `src/core/platform.hpp`, `src/core/alignment.hpp`, `src/core/alignment.cpp`, `src/core/byte_range.hpp`, `src/core/byte_range.cpp`, `src/core/gpu_virtual_address_space.hpp`, `src/core/gpu_virtual_address_space.cpp` (research R1); tests and harness unchanged; `ctest --test-dir build` still passes
- [X] T012 [US1] Create `cmake/windows_kernel_toolchain.cmake` and, because the MSVC platform module appends user-mode flags to `*_INIT` after the toolchain runs, `cmake/windows_kernel_flags.cmake` loaded via `CMAKE_USER_MAKE_RULES_OVERRIDE_CXX`, which also clears `CMAKE_CXX_CREATE_CONSOLE_EXE` (otherwise `/subsystem:console` overrides `/SUBSYSTEM:NATIVE` - caught by T010) (research R2, R3, R4, R6): `CMAKE_SYSTEM_NAME Windows`, `CMAKE_SYSTEM_PROCESSOR AMD64`, `CMAKE_CXX_COMPILER clang-cl`, `CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc`, `CMAKE_LINKER lld-link`, `CMAKE_AR llvm-lib`, `CMAKE_RC_COMPILER llvm-rc`, `CMAKE_MT llvm-mt`, `CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY`; per-configuration flag variables set without `/RTC1`; compile flags `/kernel /GR- /EHs-c- /Zl /X /D_AMD64_ /DAMD64 /D_WIN64 /DAV_CHECKED=1 /W4 /WX -Wextra -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast /clang:-ivfsoverlay<kit>/overlay.yaml -imsvc/windows-kit/km/crt -imsvc/windows-kit/km -imsvc/windows-kit/wdk-shared -imsvc/windows-kit/sdk-shared` (joined `-imsvc<dir>` form, never separate words); linker flags `/DRIVER /SUBSYSTEM:NATIVE /ENTRY:GsDriverEntry /NODEFAULTLIB /MANIFEST:NO /OPT:REF`; `CMAKE_CXX_STANDARD_LIBRARIES` set to the four kit libraries under `<kit>/lib/x64`; `<kit>` from `AV_WINDOWS_KIT_DIR`, forwarded to try-compile via `CMAKE_TRY_COMPILE_PLATFORM_VARIABLES`. T009 passes
- [X] T013 [P] [US1] Create `src/km/driver_entry.cpp`: `#include <ntddk.h>`, `extern "C" DRIVER_INITIALIZE DriverEntry;` and a definition with unnamed parameters whose body is the single statement `return STATUS_NOT_SUPPORTED;` (FR-005; contracts/commands.md). No comment beyond one line saying feature 006 replaces the body
- [X] T014 [US1] Create `src/km/CMakeLists.txt`: `project(rvkgpu CXX)`, C++23, `file(GLOB ... CONFIGURE_DEPENDS ../core/*.cpp ../proto/*.cpp *.cpp)`, `add_executable(rvkgpu ...)` with `SUFFIX .sys` and include directory `../core`; nothing else - all flags come from the toolchain file
- [X] T015 [US1] Implement `tools/check_driver_image.py <image>` (research R5): run `llvm-readobj --file-headers --coff-imports`, require `IMAGE_FILE_MACHINE_AMD64` and `IMAGE_SUBSYSTEM_NATIVE`, allow imported modules `{ntoskrnl.exe, hal.dll}` case-insensitively, print one violation line per contract, `driver_image: ok` on success. T010 passes
- [X] T016 [US1] In `CMakeLists.txt`: add cache path `AV_WINDOWS_KIT_DIR` (default `${CMAKE_SOURCE_DIR}/third_party/windows-kit`); at configure, `FATAL_ERROR` naming the directory, the first missing file, and `run tools/fetch_windows_kit.sh` if any of `VERSION`, `overlay.yaml`, `include/km/ntddk.h`, `lib/x64/ntoskrnl.lib` is missing or `VERSION` does not contain both pinned `id version` lines; add `ExternalProject_Add(rvkgpu SOURCE_DIR src/km BINARY_DIR ${CMAKE_BINARY_DIR}/driver CMAKE_ARGS -G Ninja -DCMAKE_TOOLCHAIN_FILE=... -DAV_WINDOWS_KIT_DIR=... BUILD_ALWAYS ON INSTALL_COMMAND "" BUILD_BYPRODUCTS ${CMAKE_BINARY_DIR}/driver/rvkgpu.sys)`; add ctest `driver_image` running `tools/check_driver_image.py ${CMAKE_BINARY_DIR}/driver/rvkgpu.sys`
- [X] T017 [US1] Verify: `cmake --build build` builds host and kernel core; `ctest --test-dir build` passes including `driver_image` and `tools_tests`; `cmake -S . -B /tmp/<scratch> -DAV_WINDOWS_KIT_DIR=/nonexistent` fails with the contract message; temporarily add `#include <vector>` to `src/core/alignment.hpp` and confirm `cmake --build build` fails in the `rvkgpu` step naming `vector`, then revert

**Checkpoint**: MVP - core is proven kernel-compatible on every build.

---

## Phase 4: User Story 2 - One entry point runs every gate (P1)

**Goal**: three configurations, one command, every gate inside `ctest`.

**Independent test**: delete one `TEST` from `tests/unit/test_alignment.cpp`; `tools/gate.sh` fails in the coverage configuration naming the uncovered branch.

### Tests first

- [X] T018 [P] [US2] Write `tests/tools/test_check_design.py`: fixture `src/core/bad.hpp` with `class Loose { public: static int f(); int x_; };` and `class Empty { public: void g(); };`; assert `tools/check_design.py <fixture>` exits 1 with `src/core/bad.hpp: Loose::f is static` and `src/core/bad.hpp: class Empty has no data members`; assert a pure-abstract class is not reported; assert it prints `design_rules: ok` for the real repository. Must fail now (message format differs, and untested-symbol output must no longer appear)
- [X] T019 [P] [US2] Write `tests/tools/test_check_symbol_tests.py`: fixture with `src/core/x.hpp` declaring `class X { public: int named(); int unnamed(); private: int hidden(); int v_; };` and `tests/unit/test_x.cpp` mentioning `X` and `named`; assert exit 1 with exactly `src/core/x.hpp: X::unnamed has no test naming it` (not `hidden`, not `named`); assert `src/core/platform.hpp` in the real repo is excluded; assert `symbol_tests: ok` for the real repository. Must fail now (script absent)
- [X] T020 [P] [US2] Write `tests/tools/test_check_coverage.py` against the evaluation half of `tools/check_coverage.py` (fed an `llvm-cov export` JSON fixture, a source tree and a baseline file, with no LLVM tools run): 100% over `src/core/a.cpp` and `src/core/a.hpp` with a `tests/unit/test_a.cpp` entry at 50% passes (test files excluded); an uncovered branch prints `src/core/a.cpp:41:9: false branch never taken` (or `true`; each direction is reported) and `branch coverage 75.00% < 100%`; a `.hpp` branch counts; `src/device` and `src/proto` absent is zero branches, not an error; a listed test binary with no `.profraw` prints `test_a: no profile written`; two `AV_UNTESTABLE(` marks with baseline `0` prints `AV_UNTESTABLE marks 2 > baseline 0` then each mark as `file:line: reason`; zero marks with baseline `1` passes and prints `lower tests/untestable_baseline.txt to 0`. Must fail now

### Implementation

- [X] T021 [US2] Extract the header scanning (class, method, static, member, access regexes and the per-class walk) from `tools/check_design.py` into `tools/header_scan.py`, returning per-class records (name, file, has_state, all_pure, public methods, static methods)
- [X] T022 [US2] Reduce `tools/check_design.py` to Principle III using `tools/header_scan.py`, with the violation lines from contracts/commands.md. T018 passes
- [X] T023 [US2] Create `tools/check_symbol_tests.py` (Principle IV.5) using `tools/header_scan.py`, keeping the existing `EXCLUDE` entry for `src/core/platform.hpp` with its reason and the rule that private methods are not required. T019 passes
- [X] T024 [US2] Create `tests/untestable_baseline.txt` containing `0`
- [X] T025 [US2] Implement `tools/check_coverage.py <build dir>` (research R8, R10): list `test_*` executables in the build dir, require `<build>/coverage/<name>.profraw` for each, `llvm-profdata merge -sparse`, `llvm-cov export` over all binaries, then the evaluation T020 tests; print `coverage_gate: ok` with the percentage and mark count on success. T020 passes
- [X] T026 [US2] In `CMakeLists.txt`: add ctest `symbol_tests`; `FATAL_ERROR` if `AV_COVERAGE` and `AV_FUZZ` are both `ON`; under `AV_COVERAGE`, add test `coverage_clean` (`cmake -E rm -rf` then `make_directory` of `${CMAKE_BINARY_DIR}/coverage`, `FIXTURES_SETUP coverage_clean`), give every unit test `FIXTURES_REQUIRED coverage_clean` and `FIXTURES_SETUP coverage_profiles`, add `coverage_gate` running `tools/check_coverage.py ${CMAKE_BINARY_DIR}` with `FIXTURES_REQUIRED coverage_profiles`
- [X] T027 [US2] Move `main` out of `tests/harness/test_registry.cpp` into `tests/harness/test_main.cpp`, linked only into unit tests - with `main` inside the harness the fuzz build failed to link (`multiple definition of 'main'` against libFuzzer), so the documented fuzz command had never worked. New smoke units go to `${CMAKE_BINARY_DIR}/corpus/<name>` so the committed corpus stays read-only. Rewrite the fuzz block of `CMakeLists.txt` (FR-010, FR-011): under `AV_FUZZ`, compile `av_core` and `av_harness` with `-fsanitize=fuzzer-no-link,address,undefined -fno-sanitize-recover=all`; each `tests/fuzz/fuzz_*.cpp` becomes an executable linking `av_harness` with `-fsanitize=fuzzer,address,undefined` - no restated source list; smoke test `fuzz_<name>_smoke` runs the corpus with `-max_total_time=$ENV{AV_FUZZ_SECONDS}` defaulting to `60`, and passes only on exit 0
- [X] T028 [US2] Create `tools/gate.sh` per contracts/commands.md: for `default` (`build/`), `coverage` (`build-coverage/`, `-DAV_COVERAGE=ON`), `fuzz` (`build-fuzz/`, `-DAV_FUZZ=ON`) run configure with `-G Ninja -DCMAKE_CXX_COMPILER=clang++`, build, `ctest --output-on-failure -j$(nproc)`; stop at the first failing configuration and print its name last; delete `tools/coverage.sh`
- [X] T029 [US2] Verify: `tools/gate.sh` exits 0; then, one at a time on a scratch change and reverted, confirm the quickstart section 3 rows for the design, symbol-test, coverage (`delete one TEST`), untestable ratchet and fuzz crash cases fail with the named output; record the default-configuration `ctest` wall time against SC-003 (< 60 s)

**Checkpoint**: every gate lives in `ctest`; `tools/gate.sh` is the verdict.

---

## Phase 5: User Story 5 - A clean checkout of this repository is all you need (P1)

**Goal**: no tracked path leaves the repository; KB and RE tooling live here.

**Independent test**: `git clone` the repo into an empty directory, run `tools/fetch_windows_kit.sh` and `tools/gate.sh`; both pass, and `self_contained` passes.

### Tests first

- [X] T030 [P] [US5] Write `tests/tools/test_check_self_contained.py`: create a temporary git repository with a committed `README.md` linking into the external knowledge-base checkout on line 3, a root-level file naming a sibling project by relative path, a script containing a home-directory path, a spec linking `CLAUDE.md` by a relative path that stays inside the repository, a binary file, and an untracked file naming the knowledge base (the test assembles these strings so it does not trip the gate itself); assert exit 1 with `README.md:3: references <that path>` and lines for the sibling and home paths, one report per token per line, and nothing for the in-repository link, the binary or the untracked file; assert `self_contained: ok` on the real repository after T034. Must fail now

### Implementation

- [X] T031 [US5] Implement `tools/check_self_contained.py` (research R14) over `git ls-files`: flag the knowledge-base checkout by name, home-directory paths, and any relative path token beginning `../` that resolves outside the repository root from the file's own directory; skip binary files; register ctest `self_contained` in `CMakeLists.txt`. T030 passes for the fixture cases
- [X] T032 [P] [US5] Copy `kb/*.md` from the knowledge base into `docs/kb/` and `kb/ref/ddi_by_version.json` into `docs/ref/`; rewrite paths inside them per research R13 (`headers/wdk-26100/c/Include/10.0.26100.0/...` -> `third_party/windows-kit/include/...`, `tools/X.py` -> `tools/re/X.py`, `ghidra-proj` -> `re/ghidra-proj`, `bin/msft` and `pdb/` -> `re/bin`, `kb/ref/` -> `docs/ref/`, `tools/get_wdk_libs.sh` -> `tools/fetch_windows_kit.sh`); reword the `headers/wdk-28000` cookbook passage to "change the pins in `tools/fetch_windows_kit.sh` to the next WDK and re-run `tools/re/ddi_extract.py`"
- [X] T033 [P] [US5] Copy `msfetch.py`, `import.py`, `q.py`, `pdbsyms.py`, `ddi_extract.py` into `tools/re/`; add `--sha256 <hex>` selection to `tools/re/msfetch.py get` (match the winbindex index key, fail if absent) and make it verify the downloaded file's SHA-256; create `tools/re/binaries.txt` with the ten `name sha256` pairs from research R12; create `tools/re/build_workspace.sh` per contracts/commands.md (fetch each into `re/bin/` with its PDB renamed to the binary's base name, skip present-and-matching files, then `$AV_PYGHIDRA tools/re/import.py re/ghidra-proj wddm re/bin/<each>`)
- [X] T034 [US5] Remove every external reference: `README.md` (codec link becomes the named `render-server` C-ABI library found via pkg-config; KB link -> `docs/kb/11-implementation-plan.md`), `CLAUDE.md` line 9 (`../render-server` -> `render-server`, a C-ABI library found via pkg-config), `docs/testing.md` line 136 (`re/ghidra-proj`, built by `tools/re/build_workspace.sh`), `.claude/commands/dev.md` (all KB, header, Ghidra and `q.py` paths -> `docs/kb/`, `docs/ref/`, `third_party/windows-kit/include/`, `re/ghidra-proj`, `tools/re/q.py`; add `tools/fetch_windows_kit.sh` and `tools/re/build_workspace.sh` to the environment section), `src/core/gpu_virtual_address_space.hpp` comment -> `docs/kb/04-memory.md`, `tools/gen_specs.py` reference-material block -> `docs/kb/`, `third_party/windows-kit/include/`, `re/ghidra-proj`; then run `python3 tools/gen_specs.py` (safe after T005) and confirm all 35 specs updated and 001's classes table intact
- [X] T035 [US5] Verify: `ctest --test-dir build -R 'self_contained|tools_tests'` passes; copy the tracked-or-unignored file set (`git ls-files -co --exclude-standard`) into an empty scratch directory with no sibling projects, `git init` it, run `tools/fetch_windows_kit.sh` and `tools/gate.sh` there, both exit 0 (SC-006); run `python3 tools/re/msfetch.py get basicrender.sys --sha256 cbac7c00ac64c28fa56b735ab4065c9d95fcb992f84b00100309f8737daa819d --outdir <scratch>` and confirm the file and hash (full workspace build is optional, per quickstart section 6)

**Checkpoint**: the repository stands alone.

---

## Phase 6: User Story 3 - The device side has a build home (P2)

**Goal**: `src/device` and `src/proto` sources are picked up by the host build and every gate the moment they exist, with no placeholder source.

**Independent test**: add a scratch `src/device/probe.cpp` containing `void f() { throw 1; }`; the build fails; remove it; it passes.

- [X] T036 [US3] In `CMakeLists.txt`, build `av_core` from `file(GLOB ... CONFIGURE_DEPENDS src/core/*.cpp src/proto/*.cpp src/device/*.cpp)` with include directories `src/core`, `src/proto`, `src/device` (empty globs are legal); keep the fuzz and unit targets linking it. No `if`, no placeholder source, no shared object (that is feature 026)
- [X] T037 [US3] Verify: with a scratch `src/device/probe.cpp` containing `void f() { throw 1; }`, `cmake --build build` fails with `cannot use 'throw' with exceptions disabled`; replace its body with a class `Probe` with one data member and a branch, and confirm `design_rules`, `symbol_tests` and (in `build-coverage`) `coverage_gate` all report it; delete the file and confirm everything passes. Found: `coverage_gate` passed with the untested `probe.cpp`, because a source no test references is dropped by the linker and never reaches llvm-cov; `tools/check_coverage.py` now reports `<file>: not linked into any test binary` for every `.cpp` absent from the coverage data, test-first in `tests/tools/test_check_coverage.py`

---

## Phase 7: User Story 4 - A new source file needs no build edits (P3)

**Goal**: every glob is live, and a fuzz target without a corpus cannot configure.

**Independent test**: quickstart section 4.

### Tests first

- [X] T038 [P] [US4] Write `tests/tools/test_fuzz_corpora.py`: fixture with `tests/fuzz/fuzz_a.cpp`, `tests/fuzz/fuzz_b.cpp`, `tests/fuzz/fuzz_c.cpp`, a non-empty `tests/corpus/fuzz_a/`, an empty `tests/corpus/fuzz_b/`, and no `tests/corpus/fuzz_c/`; run `cmake -DAV_SOURCE_DIR=<fixture> -P cmake/check_fuzz_corpora.cmake`; assert non-zero exit and output naming `fuzz_b` and `fuzz_c` but not `fuzz_a`; assert the real repository passes. Must fail now

### Implementation

- [X] T039 [US4] Create `cmake/check_fuzz_corpora.cmake`: for each `tests/fuzz/fuzz_*.cpp` under `AV_SOURCE_DIR`, require `tests/corpus/<name>/` with at least one file, collecting every missing target into one `FATAL_ERROR` message naming them all; `include()` it from `CMakeLists.txt` in every configuration with `AV_SOURCE_DIR` set to `CMAKE_SOURCE_DIR`, and add the corpus directories to `CMAKE_CONFIGURE_DEPENDS`. T038 passes
- [X] T040 [US4] Verify quickstart section 4: add scratch `src/core/probe.hpp`/`probe.cpp` with no build-file edits and confirm the host build, the `rvkgpu` kernel build and `coverage_gate` all see it; add a scratch `tests/fuzz/fuzz_probe.cpp` without a corpus and confirm configure fails naming it; delete all scratch files

---

## Phase 8: Polish and cross-cutting

- [X] T041 [P] Update `README.md` "Build and test" and "Layout" to the contracts: `tools/fetch_windows_kit.sh`, the fast loop, `tools/gate.sh`, the three configurations table, `cmake/`, `src/km/`, `third_party/windows-kit/` (fetched), `re/` (built), `docs/kb/`, `tools/re/`; remove `tools/coverage.sh` and the hand-rolled fuzz commands
- [X] T042 [P] Update `CLAUDE.md` Tests paragraph (`tools/gate.sh`, `tools/check_design.py`, `tools/check_symbol_tests.py` are the gates) and `.claude/commands/dev.md` sections 2 and 4 (environment check runs `tools/fetch_windows_kit.sh` if the kit is missing; the gate block lists `ctest --test-dir build`, `tools/gate.sh`, and the two check scripts; drop `coverage.sh`)
- [X] T043 Run `tools/gate.sh`, `python3 tools/gen_specs.py && git diff --exit-code specs/`, and `rg -n 'coverage\.sh' $(git ls-files)` (expect no hits; `self_contained` covers external paths); walk every row of quickstart section 3 (each was demonstrated in T017, T029, T037 and T040, and the gate-script rows are automated in `tests/tools/`; the spec-quality checklist is read-only during implementation and is not edited)
- [X] T044 Append to `specs/001-build-and-test-foundation/spec.md` Out of Scope the follow-up constitution PATCH amendment list (II "freestanding C11", IV.2 invariant signature, VII `locks.h`) so it is tracked, not forgotten (already present in Out of Scope since /speckit-specify)

---

## Dependencies and execution order

```
Phase 1 (T001-T002)
  -> Phase 2 (T003-T008)          T005 before any gen_specs run; T007-T008 before any kernel task
       -> US1 (T009-T017)          MVP
            -> US2 (T018-T029)     driver_image and tools_tests must exist for gate.sh to mean anything
                 -> US5 (T030-T035) T035's clean clone runs gate.sh
                 -> US3 (T036-T037) needs coverage_gate for its verification
                 -> US4 (T038-T040) needs the fuzz block from T027
                      -> Polish (T041-T044)
```

US3, US4 and US5 are independent of each other once US2 is done; only
`CMakeLists.txt` edits (T031, T036, T039) must be serialised.

## Parallel examples

- Phase 2: T003 and T004 together; then T005 and T006 together.
- US1: T009 and T010 together; T013 alongside T012.
- US2: T018, T019, T020 together.
- US5: T030, T032, T033 together.
- Polish: T041 and T042 together.

## Implementation strategy

1. **MVP** = Phases 1-3. After T017 a plain `cmake --build build` refuses any
   core change that would not run in the guest kernel - the largest single
   risk this feature exists to remove.
2. US2 turns the gates from scripts into the verdict.
3. US5 makes the result reproducible outside this machine.
4. US3 and US4 are small rule changes verified by scratch files.
5. Each checkpoint is a natural commit point on branch
   `001-build-and-test-foundation`; commits are made when the user asks.
