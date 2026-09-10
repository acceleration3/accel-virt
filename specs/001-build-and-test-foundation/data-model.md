# Data model: Build and test foundation

This feature adds no runtime data. Its entities are build artifacts, pinned
inputs and gate verdicts. Each is listed with what it holds, where it lives,
and the rule that makes it valid.

## Build configuration

| Field | Values |
|---|---|
| name | `default`, `coverage`, `fuzz` |
| directory | `build/`, `build-coverage/`, `build-fuzz/` |
| options | `AV_SANITIZE` always `ON`; `AV_COVERAGE` `ON` only in `coverage`; `AV_FUZZ` `ON` only in `fuzz` |
| gates | see research R8 |

Rule: `AV_COVERAGE` and `AV_FUZZ` are never both `ON`; configuration fails if
they are.

## Gate

| Field | Meaning |
|---|---|
| ctest name | `design_rules`, `symbol_tests`, `driver_image`, `self_contained`, `tools_tests`, `coverage_gate`, `fuzz_<target>_smoke`, and one per `tests/unit/test_*.cpp` |
| command | a script in `tools/` or a test binary |
| verdict | exit 0 pass, non-zero fail |
| failure output | one line per violation, carrying the offending value (Principle IX) |

Rule: a gate that did not run is not a pass. `coverage_gate` fails on a
missing profile; `tools/gate.sh` fails if any configuration fails to
configure or build.

## Driver image

`build/driver/rvkgpu.sys`, produced by the `rvkgpu` external project.

| Property | Required value |
|---|---|
| machine | `IMAGE_FILE_MACHINE_AMD64` |
| subsystem | `IMAGE_SUBSYSTEM_NATIVE` |
| imported modules | subset of `{ntoskrnl.exe, hal.dll}`, case-insensitive |
| unresolved symbols | none (link fails otherwise) |
| sources | every `src/core/*.cpp` and `src/km/*.cpp` |

## Windows kit

`third_party/windows-kit/`, git-ignored, written only by
`tools/fetch_windows_kit.sh`.

| Field | Value |
|---|---|
| packages | two `(id, version, sha256)` triples, research R11 |
| `include/km`, `include/wdk-shared`, `include/sdk-shared` | header subsets |
| `lib/x64` | `ntoskrnl.lib`, `hal.lib`, `displib.lib`, `BufferOverflowK.lib` |
| `overlay.yaml` | case-insensitive VFS overlay over the three include roots |
| `VERSION` | the two `id version` lines, compared by configure |
| `LICENSE.txt` | the WDK licence |

Rules: the directory is replaced atomically, never partially written;
configure fails if `VERSION` does not match the pins in the fetch script, or
if any required file is missing.

## Untestable baseline

`tests/untestable_baseline.txt`: one non-negative integer.

Rules: count of `AV_UNTESTABLE(` in `src/` > baseline -> fail; < baseline ->
pass with a request to lower it; = -> pass.

## Seed corpus

`tests/corpus/fuzz_<name>/`, one per `tests/fuzz/fuzz_<name>.cpp`.

Rule: exists and contains at least one file, checked at configure time.

## RE manifest

`tools/re/binaries.txt`: one line per binary, `<file name> <sha256>`, `#`
comments allowed.

Rules: the workspace script fetches exactly these; a downloaded file whose
SHA-256 differs is deleted and the script fails. Output goes to `re/bin/`
(binary with its PDB beside it, same base name) and `re/ghidra-proj/wddm`,
both git-ignored.

## Classes table row (spec header)

| Field | Meaning |
|---|---|
| Class, Kind, State it owns, Responsibility | as in every spec's autogen block |

Rule: `tools/gen_specs.py` preserves filled rows across regeneration; the
placeholder row `| | object / system | | |` is the only one it may write.
