# Quickstart: validating the build and test foundation

Contracts are in [`contracts/commands.md`](contracts/commands.md); pinned
inputs in [`research.md`](research.md) R11-R12.

## Prerequisites

clang/clang-cl/lld-link/llvm-readobj/llvm-cov/llvm-profdata (LLVM 22), CMake
>= 3.24, Ninja, Python 3, curl, unzip, sha256sum. Network access once.

## 1. Clean clone, nothing beside it

```bash
git clone <repo> /tmp/av-clean && cd /tmp/av-clean
tools/fetch_windows_kit.sh          # ~270 MB download, 33 MB kept
tools/gate.sh                       # expect: exit 0, all three configurations pass
```

Covers SC-001, SC-006, US5.1.

## 2. Fast loop

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build                 # host core AND kernel core
ctest --test-dir build              # unit, design_rules, symbol_tests, driver_image, self_contained
```

Expect under 60 seconds after the first build (SC-003).

## 3. Make each gate fail once (SC-002)

Run each on a scratch branch, observe the named failure, revert.

| Change | Expected failure |
|---|---|
| add `#include <vector>` to `src/core/alignment.hpp` | `cmake --build build`: `'vector' file not found` from the kernel build |
| call `fopen` (declared by hand) from a core function | `cmake --build build`: lld-link `undefined symbol: fopen` |
| add `static int f();` to a core class | `design_rules`: names the method |
| add a public method no test names | `symbol_tests`: names class and method |
| add a `../<sibling checkout>/x` link to `README.md` | `self_contained`: names file and line |
| link `kernel32.lib` into the driver | `driver_image`: names `KERNEL32.dll` |
| delete `TEST(alignment_divides, is_false_for_an_invalid_alignment)` | `coverage_gate`: `src/core/alignment.cpp:16:12: false branch never taken` |
| add `AV_UNTESTABLE("x")` to a core file | `coverage_gate`: `AV_UNTESTABLE marks 1 > baseline 0`, lists the mark |
| make `fuzz_address_space` trap on an input byte | `fuzz_address_space_smoke`: `deadly signal`, reproducer written under `build-fuzz/corpus/` |
| remove `tests/corpus/fuzz_address_space/` | configure: names the target |
| move the kit directory away | configure: names the path and the fetch command |

## 4. New files need no build edits (US4)

```bash
printf 'namespace av { class Probe { public: int v_ = 0; }; }\n' > src/core/probe.hpp
printf '#include "probe.hpp"\nint av_probe_branch(int x) { return x ? 1 : 0; }\n' > src/core/probe.cpp
cmake --build build-coverage && ctest --test-dir build-coverage
```

Expect the host build, the kernel build and `coverage_gate` all to see
`probe.cpp` (and `design_rules`/`symbol_tests` to object to `Probe`). Delete
both files.

## 5. Regenerating specs keeps plan decisions (FR-019)

```bash
python3 tools/gen_specs.py && git diff --exit-code specs/001-build-and-test-foundation/spec.md
```

Expect no diff: the filled classes table survives.

## 6. RE workspace (US5.4, optional, ~15 minutes of analysis)

```bash
export GHIDRA_INSTALL_DIR=/opt/ghidra JAVA_HOME=/usr/lib/jvm/java-26-openjdk
tools/re/build_workspace.sh
$AV_PYGHIDRA tools/re/q.py re/ghidra-proj wddm dxgkrnl.sys count
```
