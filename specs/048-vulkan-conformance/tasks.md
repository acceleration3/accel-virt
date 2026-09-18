---
description: "Tasks: conformance and the application suite in the guest (048)"
---

# Tasks: conformance and the application suite in the guest

**Input**: plan.md, spec.md, research.md, data-model.md,
contracts/cts_verdict.md, quickstart.md

**Tests**: written first (constitution IV); every class driven over files its
tests write, including hostile `.qpa` logs - one that ends inside a case, a
status nobody defines, a name with a newline in it, a file that is not a log at
all. Commit at every **Checkpoint** once `ctest --test-dir build -LE tools -j6`
and the feature's own tools shard pass.

**What this feature cannot fetch.** The CTS archive is not on this machine and
there is no network to pin it from (plan.md). `tools/cts_pins.txt` carries its
shape and no pins, every part that needs the archive refuses naming the fetch,
and everything that decides a verdict is built and proven here.

## Phase 1: Foundational - the result machinery

Nothing downstream can be judged without it, and all of it is provable with no
CTS present.

- [X] T001 `QpaResults` (object) in `tools/cts_results.py`: one `.qpa` log's cases. `#beginTestCaseResult <name>` opens a case, `<Result StatusCode="...">` answers it, `#endTestCaseResult` closes it, `#terminateTestCaseResult <reason>` ends it without one, `#endSession` ends the run. A name is `[A-Za-z0-9_.-]+` at most 512 bytes; a duplicate keeps the last; a status nobody defines is kept as written and never silently a pass; the last case with no end is `unfinished`. `passed(name)` is `Pass`, `QualityWarning`, `CompatibilityWarning` or `Waiver` - what dEQP itself counts as not-a-failure - and `NotSupported` is neither
- [X] T002 [P] Tests `tests/tools/test_cts_results.py` over logs the test writes: every status, a case that never ended, a log truncated mid-record, a name with a newline and one past 512 bytes, a file that is not a log, an empty file, a duplicate case, and a session that never ended
- [X] T003 `SuiteComparison` (object) in `tools/cts_results.py`: a guest result set and a host one, answering `guest_only_failures`, `matching_failures`, `guest_only_passes` and `unjudged` (data-model.md). `NotSupported` on both sides matches; on one side only it is unjudged, and unjudged cases fail the comparison - the two runs were not the same subset
- [X] T004 [P] Tests in `tests/tools/test_cts_results.py`: a guest-only failure, a matching failure, a guest-only pass, a case on one side only, `NotSupported` both ways, and a comparison of two empty sets
- [X] T005 `CtsRatchet` (object) in `tools/cts_results.py` over `tools/cts_ratchet.txt` (committed, **empty**: no guest has run the CTS yet): `lost`, `gained` and `rewritten`. An empty record passes, because a record that claimed otherwise would be a claim nobody made
- [X] T006 [P] Tests in `tests/tools/test_cts_results.py`: a lost case fails, a gained case prints the line to add, a record with a name that is not a case name is refused, and the rewritten record is sorted and unique

**Checkpoint**: commit. The verdict is decidable without a CTS.

## Phase 2: US1 - a CTS subset in the guest (P1)

- [X] T007 [US1] `tools/compare_cts.py`: the CLI of contracts/cts_verdict.md - exit 0 with `cts: N compared, 0 guest-only failures`, exit 1 naming every guest-only failure one per line with the matching-failure count, exit 2 when either side could not be read or the sets are not the same cases. Tests `tests/tools/test_compare_cts.py`
- [X] T008 [US1] `tools/check_cts_ratchet.py`: exit 0 while every recorded case still passes, exit 1 naming every case lost, and every gain printed with the line to add. Tests in `tests/tools/test_compare_cts.py`
- [X] T009 [P] [US1] `CtsShards` (object) in `tools/cts_shards.py`: the subset file (blank lines and `#` comments dropped), `per_shard` at least 1, `shards()` covering every case exactly once in order, and `after(name)` answering the tail past a case that died. Tests `tests/tools/test_cts_shards.py`, including a subset of one, a shard size larger than the subset, and a case that is not in the list
- [ ] T010 [US1] `CtsPins` and `CtsFetch` in `tools/fetch_cts.py`, in the shape of `tools/fetch_layers.py`: a pinned archive and one `file` line per binary, nothing written when a hash disagrees, `--check`. `tools/cts_pins.txt` documents its own shape and **is otherwise empty**, as `tools/dxvk_pins.txt` is. Tests `tests/tools/test_fetch_cts.py` over archives the test builds and serves through `file://`
- [X] T011 [US1] `tools/cts_subset.txt`, `CtsSubset` in `tools/cts_subset.py` and the gate `tools/check_cts_subset.py`. **Patterns, not case names**: one dEQP group is thousands of cases and the list changes with every release, so the file holds globs and the runner keeps what the CTS's own case list matches. Each line carries `for: <extension>`, and the gate reads the ICD's policy table and fails naming any reported extension with no group - and any group for an extension nobody reports, which is a subset that drifted from the driver. Registered as the ctest `cts_subset`. Tests `tests/tools/test_cts_subset.py`
- [ ] T012 [US1] `run_guest_suite.py --cts`: refused before anything boots without the pinned CTS, naming the fetch; otherwise pushes the CTS and one shard's case list at a time, runs each with the CTS's own per-case timeout, pulls `TestResults.qpa` before the next shard, and copies every log into `diag/`. Tests in `tests/tools/test_run_guest_suite.py`
- [ ] T013 [US1] Resume: a shard whose log ends inside a case is run again from the case after it, at most `--cts-restarts` times, with the case that died recorded as `Crash` rather than dropped; a guest that bugchecked is rebooted through the runner's existing rescue path first. Tests over the fake guest: a crash mid-shard, a shard that crashes twice, and a shard that crashes past the restart limit
- [ ] T014 [US1] The run's verdict: the shards' results merged into one `.qpa`-shaped set, compared against the host's when `--cts-host` names one, and held to the ratchet either way. Tests in `tests/tools/test_run_guest_suite.py`

**Checkpoint**: commit.

## Phase 3: US2 - the application suite (P1)

- [ ] T015 [US2] `FrameReference` (object) in `tools/frame_reference.py`: a captured PPM, a committed reference and a per-channel tolerance; `differs` counts the pixels outside it, a size mismatch is a failure naming both sizes, and a mismatch writes the capture, the reference and a difference image side by side. Tests `tests/tools/test_frame_reference.py` over PPMs the test writes
- [ ] T016 [US2] The runner captures each application's frame through the device's scanout (`Screendump`, FR-004) at a scripted point, not through a guest screenshot API, and compares it with `specs/048-vulkan-conformance/references/<name>.ppm`. Tests in `tests/tools/test_run_guest_suite.py`
- [ ] T017 [US2] The applications the guest can already run - `rvkvkprobe.exe`'s present step (046) and, when the layers are fetched, 047's three D3D probes - are the suite's first entries; vkcube and the layer samples join it when their pins are filled, and the runner says which are skipped rather than passing quietly

**Checkpoint**: commit.

## Phase 4: US3 and the gate

- [ ] T018 [US3] `tools/gate.sh` runs `--cts` as a guest step when both a prepared image and the pinned CTS are there, and says it skipped when either is missing (FR-006)
- [ ] T019 docs: `docs/testing.md` (the conformance run, what the verdict is and why it is a comparison, and the ratchet) and `docs/kb/` if the CTS's shape needs writing down
- [ ] T020 Coverage and the tools shards; `AV_GATE_JOBS=6 AV_GUEST_IMAGE=... tools/gate.sh`; merge; push

## Dependencies

- Phase 1 blocks everything: the comparison is what every later step reports.
- T010 blocks T012 (the refusal is the fetch's), and T009 blocks T013.
- Phase 3 is independent of Phase 2 and can be built beside it.

## Implementation strategy

US1 is the MVP, and its first half - the result machinery - is the half that
decides whether an answer is right. It lands first and is proven without the
CTS. The rest is the machinery around an archive this machine cannot fetch,
built so that the day someone fetches it the run is already written.
