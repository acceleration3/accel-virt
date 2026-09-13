# Implementation Plan: the generators, vendored

**Branch**: `063-render-server-integration` | **Date**: 2026-09-13 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/063-render-server-integration/spec.md`

## Summary

Three graphics APIs' worth of serialization is not something to write by hand,
and render-server already has a generator that reads the Khronos registries and
emits it. This feature takes **the generator and nothing else**: it is vendored
into this repository, owned here, and pointed at this project's own wire types;
what it emits is committed, reviewed and compiled like any other source, and a
gate re-runs it to prove the committed tree is what the generator would write.

Four things have to be true, and the third is the work:

1. **The generator is here**, under `tools/codegen/`, with the profile and
   protocol-id files that steer it. Nothing fetches render-server, at build time
   or ever.
2. **The registries are pinned**: `vk.xml`, `gl.xml` and `cl.xml` fetched by
   revision and verified by hash, like QEMU and the Windows kit. The build never
   needs them, because what it compiles is committed; only regenerating does.
3. **The emitters speak this project's language.** What render-server's
   generator emits today includes `rvk/protocol/buffer.hpp`, its logging, and
   in two backends `std::vector`. This repository has `WireReader` and
   `WireWriter` of its own, builds `-ffreestanding -fno-exceptions -fno-rtti`,
   and has no STL in `src/proto` or `src/device` at all. Owning the generator
   means changing what it emits, not wrapping what it emits in a shim that would
   outlive the reason for it.
4. **The gates judge machine-written code by rules it can keep**: the design and
   coverage gates exclude generated directories by an explicit rule, the
   compiler does not, and whatever decodes a frame from outside gets a fuzz
   target.

## Technical Context

**Language/Version**: Python 3 for the generator (vendored); C++23 for what it
emits, compiled under this repository's existing flags.

**Primary Dependencies**: the Khronos registries; the wire types this repository
already has in `src/proto/wire/`; feature 032's `rvk_executor.h`, which stays
this project's ABI.

**Testing**: the generator's own `--check` as a ctest gate; compilation of the
emitted sources in both the device and guest builds; a fuzz target over the
generated decoder with a committed corpus; and the existing image gates for any
guest artefact.

**Target Platform**: the generator runs on the developer's Linux machine; its
output compiles for the QEMU device (Linux x86-64, freestanding) and for the
Windows guest (x64, and x86 when a guest driver needs it).

**Performance Goals**: none of its own. The generated decoder is on the
submission path later; this feature only has to not make that impossible.

**Constraints**: `src/proto` and `src/device` have no STL and no exceptions, and
the guest driver has no C runtime at all. Generated code that lands there obeys
the same rules. A host-side executor library is an ordinary Linux shared object
and may use more, which is a decision to make per emitted file rather than once.

**Scale/Scope**: three registries; on render-server's numbers about 8 MB of
emitted C++ across 37 files for Vulkan alone, which is why it is committed and
gated rather than regenerated on every build.

## Constitution Check

| Principle | How this feature satisfies it |
|---|---|
| I. Specification precedes implementation | The spec was rewritten before any code when the scope changed from "consume render-server" to "take its generators". |
| II. The core is portable; the shim is not | Generated code targets `src/proto` and the host executor, neither of which is kernel-only; nothing generated reaches `src/km`. |
| III. Object-oriented, with state | This feature adds no C++ class of its own: it adds tooling and the code that tooling writes. The table below says so rather than inventing one. |
| IV. Test-first and exhaustive | The gate that proves the committed tree matches the generator is written before the tree is committed; the decoder's fuzz target and corpus land with the decoder. |
| V. Everything from outside is hostile | A generated decoder is exactly where a hostile frame lands first, which is why FR-006 requires the fuzz target there. |
| VI. Feature levels are phases | Vulkan first, then OpenGL and OpenCL; each is a phase that can stop cleanly. |
| VII. IRQL and locking are typed | Generated code runs at PASSIVE in the guest driver and in ordinary user space on the host; nothing it emits is reachable from a DPC. |
| VIII. No speculative structure | No shim headers standing in for render-server's types, and no abstraction over the three APIs until two of them are in. |
| IX. Reproducible and observable | The whole feature is one claim: a clean checkout plus a fetch reproduces the committed tree byte for byte, and the gate says which file disagrees. |

No violations.

## Classes this feature introduces

| Class | Kind | State it owns | Responsibility (no "and") |
|---|---|---|---|
| — | — | — | — |

**None, deliberately.** This feature is tooling and the sources that tooling
writes. The generator is Python, vendored under `tools/codegen/`; the C++ it
emits declares types the Khronos registries dictate, not types this project
designs. The classes that *use* the generated wire - the host executor's
backends, the guest transport - belong to features 042 and 043 and are named
there. An entry in this table for a class with no state would be a mistake
written down, which is what the constitution says about stateless classes.

## Project Structure

```text
tools/codegen/                       # the vendored generator, owned here
tools/codegen/profiles/*.toml        # which commands and ids, this project's
tools/fetch_khronos_registries.sh    # vk.xml, gl.xml, cl.xml by revision + hash
tools/check_generated.py             # re-runs the generator, fails on a difference
tools/check_design.py                # excludes generated directories
tools/check_coverage.py              # the same exclusion, named once

src/proto/generated/vulkan/          # committed, compiled, never hand-edited
src/proto/generated/opengl/
src/proto/generated/opencl/

tests/fuzz/fuzz_generated_decode.cpp # a hostile frame into the generated decoder
tests/corpus/fuzz_generated_decode/
```

**Structure Decision**: generated sources live under `src/proto/`, beside the
hand-written wire they are built on, because that is what they are: more of this
project's protocol. They are in a `generated/` directory so that one rule names
them for every gate that has to treat them differently.

## Phase 0: research

`research.md` answers what the generator needs to be lifted out of
render-server's tree, what its emitters assume about the runtime around them,
which of those assumptions this repository can satisfy as-is (`WireReader`,
`WireWriter`) and which need the emitters changed (`std::vector` in two
backends, render-server's logging, its buffer headers), where the registries
come from and at what revision, and how much emitted code there is.

## Phase 1: design

`data-model.md` describes the generator's inputs and outputs as data - registry,
profile, protocol ids, emitted files - and the rules that make a regeneration
reproducible. `contracts/` records what the emitted serialization must satisfy
to compile here: the wire types it may name, the headers it may include, what it
may not use. `quickstart.md` is how to fetch, generate, check and fuzz.

## Complexity Tracking

The one thing that will look like complexity and is not: the generator is
vendored rather than fetched. That was chosen deliberately - the code it emits
is this project's to own, and a pinned upstream whose generator we must patch
to use our own types is a coupling with no payoff.
