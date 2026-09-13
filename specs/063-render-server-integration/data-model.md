# Data model: what the generator reads and writes

No runtime entities: this feature's data is files, and its invariant is that one
set of them determines the other exactly.

## Inputs

| Input | Where it lives | Pinned how | Changing it means |
|---|---|---|---|
| `vk.xml`, `gl.xml`, `cl.xml` | `third_party/khronos/` (git-ignored) | revision + hash in `tools/fetch_khronos_registries.sh` | a wire change: the regenerated output differs and the gate says so |
| the API headers the output compiles against | the same fetch | the same | `pNext` structs appear or vanish from the emitted chain handling |
| `profiles/*.toml` | `tools/codegen/profiles/` (committed) | this repository's to edit | commands enter or leave the wire |
| `protocol_ids/*.toml` | `tools/codegen/profiles/` (committed) | this repository's to edit | a wire id moves, which `docs/protocol.md` must match (FR-007) |
| the generator itself | `tools/codegen/rvk_codegen/` (committed) | this repository's to edit | every emitted file may change |

## Output

| Directory | Compiled into | Rules it keeps |
|---|---|---|
| `src/proto/generated/vulkan/` | the device build and the guest build | freestanding: no STL, no exceptions, no RTTI; includes only `<cstdint>`-style headers, the API header, and this repository's `proto/wire/` |
| `src/proto/generated/opengl/` | the same | the same |
| `src/proto/generated/opencl/` | the same | the same |
| `src/host/generated/` (later, if needed) | the host executor only | an ordinary Linux shared object: more is allowed, and the directory is what says so |

## The invariant

**The committed output is what the generator writes.** `tools/check_generated.py`
runs the generator with `--check` against every committed directory. It fails
naming the file that differs; it skips, saying why, when the registries have not
been fetched, because a developer who has not fetched them has not broken
anything.

Two corollaries the tasks enforce:

- a change to the generator, a profile or an id file is committed together with
  the regenerated sources, never apart;
- nothing hand-written lives under a `generated/` directory, because the gates
  that exclude those directories would stop judging it.

## State transitions

```
registries fetched ──generate──▶ emitted tree ──commit──▶ committed tree
        │                              ▲                        │
        └──────────── --check ─────────┴─────── compared ───────┘
```

A regeneration that changes nothing is the normal case and produces no diff. A
regeneration that changes something is a wire change, and the diff is the
review.
