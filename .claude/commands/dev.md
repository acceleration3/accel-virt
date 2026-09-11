---
description: Load all accel-virt context and start (or continue) development on a feature
argument-hint: "[feature number, e.g. 005 — omit to pick the next unblocked one]"
allowed-tools: Bash, Read, Glob, Grep, Edit, Write, Skill, TodoWrite
---

You are developing **accel-virt**: a paravirtual GPU for Windows guests on
QEMU/KVM. Before writing anything, load the context below. Do not skim it — the
rules in it are gates, not suggestions, and work that violates them gets
rejected by the build.

## 1. Load the context

Read these, in this order:

1. `CLAUDE.md` — the C++ standard for this repo (decrapify-cpp, adapted for
   freestanding kernel-mode).
2. `.specify/memory/constitution.md` — the nine principles. III (object-oriented
   with state) and IV (test-first and exhaustive) are marked NON-NEGOTIABLE and
   are enforced by build gates.
3. `docs/testing.md` — the five test layers and why a display miniport can be
   unit tested on Linux at all.
4. `docs/roadmap.md` — the 35 features, their status and dependencies.
5. `README.md` — layout and build commands.

The design this implements lives in the knowledge base in `docs/kb/`:

- `docs/kb/11-implementation-plan.md` — the architecture and milestones. Read this.
- `docs/kb/02-kmd-contract.md`, `docs/kb/04-memory.md`,
  `docs/kb/05-scheduling.md`, `docs/kb/06-display.md` — the WDDM mechanisms.
- `docs/kb/10-warp-kmd.md` — `basicrender.sys`, Microsoft's own hardware-free
  WDDM miniport, reverse engineered. The closest existing thing to what we build.
- `docs/kb/12-re-cookbook.md` — how to answer a question by reverse engineering
  instead of guessing.
- `docs/ref/driver_init_table.md` — all 188 DDI slots and the WDDM level each
  arrived in.

Normative headers (these outrank everything above when they disagree):
`third_party/windows-kit/include/{km,wdk-shared,sdk-shared}`, the pinned WDK
and SDK 10.0.26100 subset fetched by `tools/fetch_windows_kit.sh`.

Nothing outside this repository is referenced; the `self_contained` gate keeps
it that way.

## 2. Check the environment

From the repository root:

```bash
git status --short && git log --oneline -3
[ -f third_party/windows-kit/VERSION ] || tools/fetch_windows_kit.sh
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ >/dev/null && cmake --build build 2>&1 | tail -3
ctest --test-dir build 2>&1 | tail -3
ls specs/ | head -40
```

Tools available, and what each is for:

| | |
|---|---|
| `clang++` / `clang-cl` / `lld-link` | native tests, and the driver cross-build (`rvkgpu.sys`, built by every `cmake --build build`) |
| `cmake` + `ninja`, `ctest` | build and the test entry point of each configuration |
| `llvm-cov`, `llvm-profdata` | the `coverage_gate` test in `build-coverage/` — the 100% branch gate |
| `llvm-pdbutil` | Microsoft PDBs, public symbols only, no type info |
| Ghidra 12.1.2 at `/opt/ghidra` | needs `JAVA_HOME=/usr/lib/jvm/java-26-openjdk`; the system default JDK 17 is too old |
| `ghidra` MCP (`pyghidra-mcp`) | if its tools are absent this session, drive Ghidra headless with `tools/re/q.py` instead. Never run both against the project at once — Ghidra locks it. |
| `qemu-system-x86_64` 11.1.1, `virsh` | the host side; `virtio-gpu-gl-pci` has `blob`, `hostmem`, `venus`, `drm_native_context` |

Ghidra project `re/ghidra-proj` (git-ignored): `dxgkrnl.sys`, `dxgmms2.sys`,
`basicrender.sys`, `basicdisplay.sys`, `vkrnlintvsp.sys`, `vpci.sys`,
`d3d12core.dll`, `d3d12.dll`, `dxcore.dll`, `dxgi.dll`, symbolized. If it is
absent, `tools/re/build_workspace.sh` rebuilds it from the pinned
`tools/re/binaries.txt`.

```bash
export GHIDRA_INSTALL_DIR=/opt/ghidra JAVA_HOME=/usr/lib/jvm/java-26-openjdk
PY=~/.local/share/uv/tools/pyghidra-mcp/bin/python
[ -d re/ghidra-proj/wddm.rep ] || tools/re/build_workspace.sh
$PY tools/re/q.py re/ghidra-proj wddm dxgkrnl.sys funcs 'Doorbell'
$PY tools/re/q.py re/ghidra-proj wddm basicrender.sys dec WarpKMSubmitCommandVirtual
```

## 3. Pick the work

`$ARGUMENTS` names a feature number if given. If it is empty, read
`docs/roadmap.md`, find the lowest-numbered feature whose dependencies are all
complete and which is not yet implemented, and say which one you picked and
why before starting.

Then follow the spec-kit flow, using the skills, not by hand:

```
/speckit-specify    only if specs/NNN-*/spec.md still has an empty
                    "User Scenarios" section
/speckit-clarify    if anything in the spec is ambiguous — cheaper now than later
/speckit-plan       MUST fill in the spec's "Classes this feature introduces"
                    table before any code is written
/speckit-tasks      break the plan down
/speckit-implement  execute
```

### Git

There is no remote; `master` is the only long-lived branch and holds only
features that passed every gate.

1. Start each feature from an up-to-date `master` on a branch named for its
   spec directory: `git switch -c NNN-short-name`.
2. Commit at each `**Checkpoint**` in `tasks.md` without waiting to be asked,
   once `ctest --test-dir build` passes at that point. Conventional-commit
   subject (`feat(scope): ... (NNN)`), a body saying what changed and why.
3. When every task is `[X]` and `tools/gate.sh` passes, fast-forward it in:
   `git switch master && git merge --ff-only NNN-short-name && git branch -d NNN-short-name`.
4. Never commit kit files, `re/`, build directories, or anything the gates
   reject; never rewrite `master` history.

A feature counts as complete for "pick the work" when its `tasks.md` is all
`[X]` and it is merged into `master`.

## 4. The rules you will be judged against

**Design (constitution III, `CLAUDE.md`).** Everything is a class that owns
state. No static member functions. No stateless class used as a namespace. No
free functions outside the two ABI boundaries — the `DxgkDdi*` thunks in
`src/km/` and `extern "C"` entry points — and each of those is one line with no
logic. If functions thread the same handle, buffer or platform through their
parameters, that parameter is a member. Dependencies injected at construction;
never reach for a global. Name each new class and say whether it is an **object
class** (one per real thing) or a **system class** (a service owning a
collection) before writing it.

**Testing (constitution IV).** For every function: every branch covered,
invariants asserted after every mutation, and hostile input fed to anything
that reads bytes it did not write, plus a libFuzzer target with a committed
corpus. Tests are written before the implementation and must fail first.

**Both are build gates**, not review notes:

```bash
ctest --test-dir build   # unit tests (ASan+UBSan), design_rules, symbol_tests,
                         # driver_image, self_contained, tools_tests
tools/gate.sh            # the full verdict: the above, plus coverage_gate
                         # (100% branches, AV_UNTESTABLE ratchet) and fuzz smoke
```

`tools/check_design.py` (static methods, stateless classes) and
`tools/check_symbol_tests.py` (public API no test names) run inside `ctest`;
run either directly for a faster answer.

`src/core/gpu_virtual_address_space.cpp` with
`tests/unit/test_gpu_virtual_address_space.cpp` is the worked example of the
standard. Match it.

**Two more things that are easy to get wrong:**

- The driver always declares `DXGKDDI_INTERFACE_VERSION_WDDM3_2` (`0x11007`).
  WDDM levels are implementation *phases*; capabilities gate what is live.
  Never report a capability that is not implemented and tested.
- When the question is "what does `dxgkrnl` do with this", read `dxgkrnl.sys`
  in Ghidra. When it is "what is the minimum that works", read
  `basicrender.sys`. When it is "what are the fields of this struct", read the
  WDK header — do not disassemble for something that is written down.

## 5. Report before you start

State: which feature, why it is next, which classes you will add and of which
kind, what the first failing test will be, and anything in the spec you had to
interpret. Then begin.
