# Research: taking the generators

Everything here was read out of `render-server` at revision
`89936e50b6145e75af3553e27d5c8a251787dd62`, out of this repository, or out of
the pinned Khronos material; each finding names where it came from.

## 1. What the generator is

`codegen/rvk_codegen` is a Python package run in place - `python3 -m
rvk_codegen` - with siblings `rvk_codegen.opengl` and `rvk_codegen.opencl`. Its
own `pyproject.toml` says what matters most for taking it: *"It imports nothing
outside the standard library, which is what keeps that true."* There is no
dependency list, no build backend, no package name. About 4,600 lines of Python
for the Vulkan half.

It reads a Khronos registry (`--xml`), a profile that selects commands and
extensions (`--profile`), a protocol-id map (`--ids`), and optionally the header
the output will compile against (`--header`, which keeps it from emitting
`pNext` structs the header does not declare). It writes a directory (`--out`),
and `--check` re-emits into a temporary directory and diffs - which is exactly
the gate this feature needs, already written.

Its emit backends are `ids`, `command_names`, `serialize`, `client_dispatch`,
`server_dispatch`, `server_backends`, `icd_entries`, `supported_extensions`,
plus a `width_audit`. Nothing declares which files a backend writes; `--check`
compares the whole directory.

**Decision: vendor it, do not fetch it.** A generator whose emitters must be
changed to speak this project's language is not an upstream to track; it is
source this project owns. Being stdlib-only makes that free of consequences.

## 2. What its output assumes today

From `generated/vulkan/` at the same revision:

| Include | Who provides it now | What this project does |
|---|---|---|
| `<cstdint>`, `<cstring>`, `<cstddef>` | the toolchain | unchanged |
| `<vulkan/vulkan_core.h>` | Vulkan-Headers | a pinned registry input |
| `rvk/protocol/buffer.hpp`, `buffer_reader.hpp` | render-server's runtime | **replaced** by this repository's `src/proto/wire/wire_reader.hpp` and `wire_writer.hpp` |
| `rvk/instr/log.hpp` | render-server's instrumentation | **removed**; this project logs through its own `Platform` and `TextBuffer` |
| `<vector>` | the STL | **removed** where the output lands in `src/proto` or `src/device` |

The STL is not spread through the output: `std::vector` appears in exactly two
emitters, `server_dispatch.py` and `icd_entries.py`, and in two emitted files.
**`serialize`, the backend that decodes bytes from outside, is already
STL-free** - which is the one that matters most, because it is where a hostile
frame lands.

This repository has no `std::vector` or `std::string` anywhere in `src/proto` or
`src/device`, and `rvk_device` compiles `-O2 -ffreestanding` under the
project-wide `-fno-exceptions -fno-rtti` (`CMakeLists.txt:26`, `:69`).

**Decision: change the emitters, do not shim the includes.** Headers placed at
`rvk/protocol/...` to satisfy machine-written code would be a permanent
compatibility layer for a coupling this feature exists to remove. The emitters
are ours now; they emit our includes and our types.

**Decision: where an emitted file lands decides what it may use.** The
serialization and protocol ids go to `src/proto/generated/`, freestanding and
STL-free. A server dispatch that only ever runs inside the host executor - an
ordinary Linux shared object - may use more, and says so in its own directory.
Nothing generated goes anywhere near `src/km`.

## 3. The registries

render-server vendors `cl.xml`, `gl.xml` and `wgl.xml` under `external/` and
takes `vk.xml` from Vulkan-Headers. This project fetches instead, by revision
and hash, the way `tools/fetch_qemu.sh` and `tools/fetch_windows_kit.sh`
already do, into `third_party/khronos/` (git-ignored).

**Decision: pin each registry to a tag, not a branch.** A registry that moves
under a regeneration is a wire change that has to be reviewed as one, and a tag
is what makes the diff mean that.

## 4. What steers the generator

`profile.toml` carries `api_version` and lists of extensions to include and
commands to exclude - render-server's excludes are the platform-tied entry
points whose create-info cannot cross a process boundary (`xcb_connection_t*`,
`wl_display*`). `protocol_ids.toml` maps commands to wire ids.

**Decision: the profile starts as render-server's and becomes this project's.**
Taking it as-is gives a working generation on day one and a diff that shows
exactly what this project later decides to drop; starting from an empty profile
would mean choosing hundreds of commands before anything compiles. The ids file
is the one to guard: `docs/protocol.md` documents what this project puts on the
wire, and FR-007 makes the two agree.

## 5. Scale

37 emitted files and about 8.6 MB of C++ for Vulkan alone. That is why the
output is committed rather than generated during the build: a build that needs
Python and three registries to produce 8 MB of C++ is a build that fails on a
machine missing any of them, and a reviewer cannot see a wire change that no
commit contains.

## 6. Alternatives considered

- **Fetch render-server pinned and patch it** (what this feature first said).
  Refused by the project: the code is not to be reused, only the generators.
- **Write generators from scratch against `vk.xml`.** The registry walk, the
  `pNext` chain handling and the width audit are the hard parts and they already
  exist; rewriting them would be months for no difference in output.
- **Generate during the build.** Refused above: it moves 8 MB of reviewable
  source out of review and makes Python and three registries build dependencies.
- **Shim headers at render-server's include paths.** Refused above: a permanent
  layer standing in for the coupling this feature removes.
