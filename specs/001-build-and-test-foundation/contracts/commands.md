# Command contracts

The externally visible interface of this feature is a set of commands and
configure-time settings. Each contract below is what a caller may rely on.
All commands run from any working directory and resolve the repository root
from their own location.

## `tools/gate.sh`

The one command for the full verdict.

```
tools/gate.sh                 # default, then coverage, then fuzz
```

| Exit | Meaning |
|---|---|
| 0 | every gate in every configuration passed |
| 1 | a configuration failed to configure, build, or pass `ctest`; its name and the failing test names are printed last |

Environment: `AV_FUZZ_SECONDS` (default `60`) is passed to fuzz smoke tests.
Lowering it is for local iteration; the gate's contract is the default.

## `tools/fetch_windows_kit.sh`

```
tools/fetch_windows_kit.sh
```

| Exit | Meaning |
|---|---|
| 0 | `third_party/windows-kit/` holds the pinned subset and `overlay.yaml` |
| 1 | download failed, hash mismatch (both hashes printed), or extraction failed; any existing kit is untouched |

Idempotent: if `third_party/windows-kit/VERSION` already matches the pins it
exits 0 without network access.

## Configure-time settings (top-level `CMakeLists.txt`)

| Variable | Default | Contract |
|---|---|---|
| `AV_WINDOWS_KIT_DIR` | `${CMAKE_SOURCE_DIR}/third_party/windows-kit` | Configure fails with the path, the missing file, and `run tools/fetch_windows_kit.sh` if the kit is absent, incomplete, or a different version |
| `AV_SANITIZE` | `ON` | ASan + UBSan on host targets |
| `AV_COVERAGE` | `OFF` | adds profile instrumentation and `coverage_gate` |
| `AV_FUZZ` | `OFF` | adds fuzzer instrumentation and `fuzz_*_smoke` |

Configure also fails when a fuzz target has no non-empty corpus directory,
naming the target.

## Gate scripts

Each takes the repository root as its first argument (default `.`), prints
`<gate>: ok` on success (`coverage_gate` adds the percentage and mark count), and on failure prints one line per violation to
stderr and exits 1.

| Script | Extra arguments | Violation line |
|---|---|---|
| `tools/check_design.py` | - | `src/core/x.hpp: X::method is static` / `src/core/x.hpp: class X has no data members` |
| `tools/check_symbol_tests.py` (class) | - | `src/core/x.hpp: class X has no test naming it` |
| `tools/check_symbol_tests.py` | - | `src/core/x.hpp: X::method has no test naming it` |
| `tools/check_self_contained.py` | - | `README.md:16: references <path outside the repository>` |
| `tools/check_driver_image.py` | `<path to rvkgpu.sys>` | `subsystem IMAGE_SUBSYSTEM_WINDOWS_CUI, want IMAGE_SUBSYSTEM_NATIVE` / `imports KERNEL32.dll, not a kernel module` |
| `tools/check_coverage.py` | `<build dir>` | `src/core/x.cpp:41:9: false branch never taken` / `test_x: no profile written` / `src/device/y.cpp: not linked into any test binary` / `branch coverage 98.50% < 100%` / `AV_UNTESTABLE marks 2 > baseline 0` followed by each mark |

## `tools/re/build_workspace.sh`

```
tools/re/build_workspace.sh
```

Requires `GHIDRA_INSTALL_DIR`, `JAVA_HOME` (JDK 21+), and a Python with
`pyghidra` in `AV_PYGHIDRA` (default `~/.local/share/uv/tools/pyghidra-mcp/bin/python`).

| Exit | Meaning |
|---|---|
| 0 | `re/bin/` holds every manifest binary with its PDB, and `re/ghidra-proj/wddm` has each imported and analysed |
| 1 | a fetch failed, a hash mismatched, or an import failed; the file is named |

Re-running skips binaries already present with the right hash and programs
already imported.

## `DriverEntry`

```cpp
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT, PUNICODE_STRING);
```

Returns `STATUS_NOT_SUPPORTED` without touching its arguments, until feature
006 replaces the body with the `DxgkInitialize` call. One statement.
