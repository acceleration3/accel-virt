# Implementation plan: conformance and the application suite in the guest (048)

**Branch**: `048-vulkan-conformance` · **Spec**: spec.md · **Research**: research.md

## Summary

Answer "does Vulkan in the guest behave like Vulkan on the host" with a number
that is a *comparison*, not a count: the same CTS subset run in the guest on
rvkgpu and on the host GPU natively, and every case that passes on the host and
does not in the guest named. Around that: a runner step that survives a case
crashing, a TDR and a guest bugcheck and carries on from the next case, a
committed ratchet so a lost case fails the build, and frames captured through
the device's scanout compared with committed references.

Everything here is host-side tooling and the guest runner. No new C++: the
guest already has the programs this runs (`rvkvkprobe.exe`, the D3D probes of
047), and what 048 adds is what pushes, runs, reads and judges them.

## What this feature cannot fetch, and what it does instead

The CTS is a build nobody here has downloaded, as DXVK and vkd3d-proton were
in 047. This machine has no network, and a URL and a hash written from memory
would pin whatever a guess named - which is indistinguishable from a tampered
archive at the point it matters. So:

- `tools/cts_pins.txt` documents its own shape and is otherwise **empty on
  purpose**, exactly as `tools/dxvk_pins.txt` is.
- `tools/fetch_cts.py` is written and tested over archives its tests build and
  serve through `file://`, so the tooling is proven either way.
- `run_guest_suite.py --cts` refuses before anything boots, naming the fetch to
  run, and `tools/compare_cts.py` says the comparison could not be made rather
  than passing quietly.

What that leaves genuinely proven here: every part that reads or judges
results, which is the part a wrong answer would come from.

## Technical context

Python 3.14 for the tooling (`tools/`), tested by `unittest` under
`tests/tools/` as sharded ctests labelled `tools`; the guest runner
`tools/guest/run_guest_suite.py` over the fake guest in
`tests/tools/fake_guest/`. The result format is dEQP's `.qpa` log, which the
CTS writes for every run and which records a case that never finished
(`#terminateTestCaseResult`) as distinctly as one that failed - that is what
makes "resume from the next case" possible at all. Frames come from the
device's scanout through the runner's existing QMP `Screendump` (FR-004), not
from a guest-side screenshot API.

## Classes this feature introduces

| Class | Kind | State it owns | Responsibility |
|---|---|---|---|
| `QpaResults` | object | one `.qpa` log's case names and statuses | What one CTS run did, case by case |
| `SuiteComparison` | object | a guest result set and a host one | Which cases the guest alone fails |
| `CtsRatchet` | object | the committed record of what the guest passes | Whether a run lost a case it used to pass |
| `CtsShards` | object | a subset list and a shard size | Which cases each guest run is given |
| `CtsPins` / `CtsFetch` | object | the pinned archive and what is on disk | The CTS build this repository runs |
| `FrameReference` | object | a captured frame, a reference and a tolerance | Whether a frame is the one committed |
| `CtsRun` | system (in the runner) | one guest's CTS steps | Running the subset in the guest and pulling its results |

## Phasing

1. **The result machinery** (`QpaResults`, `SuiteComparison`, `CtsRatchet`)
   with hostile `.qpa` inputs: everything that decides a verdict, testable with
   no CTS present.
2. **The fetch** (`CtsPins`, `CtsFetch`) and the empty pins.
3. **The runner's `--cts`**: sharding, per-case timeouts, resume after a crash
   or a bugcheck, `.qpa` and logs into `diag/`, and the refusal without pins.
4. **The application suite**: frames through the scanout, `FrameReference`,
   committed references for what the guest can already run.
5. **The gate**: `--cts` as a guest step when an image and the pins are both
   there, and the ratchet as the verdict.

## Constitution check

- **III (objects with state)**: every class above owns state; the tools hold no
  module-level state and no free functions beyond each script's `main`, which
  is its one ABI boundary.
- **IV (tests)**: every class is driven over files its tests write, including
  hostile `.qpa` inputs - a truncated log, a case that never ended, a status
  nobody defines, a name with a newline in it. The Python tools are held to the
  same branch coverage by their own tests; `tools/check_symbol_tests.py` covers
  the C++ surface, which this feature does not grow.
- **V (everything outside is hostile)**: a `.qpa` is written by a process that
  crashed halfway through often enough that it is the normal case, and the
  guest's copy of it arrives over the agent. It is parsed defensively.
- **VI (never report an unimplemented capability)**: a run without the pinned
  CTS reports that it could not be made, never a pass.
