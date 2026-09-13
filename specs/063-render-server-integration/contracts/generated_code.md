# Contract: what generated code may be

This is what the emitters are changed to satisfy, and what a reviewer checks a
regenerated tree against.

## A. What a file under `src/proto/generated/` may include

| Allowed | Why |
|---|---|
| `<cstdint>`, `<cstddef>`, `<cstring>` | freestanding-safe, already used across `src/proto` |
| the API's own header (`vulkan_core.h`, `GL/gl.h`, `CL/cl.h`) | the types on the wire are the API's |
| `proto/wire/wire_reader.hpp`, `proto/wire/wire_writer.hpp` | this repository's wire, which the emitted serialization is written against |
| another generated header in the same directory | the generator's own structure |

Nothing else. In particular: no `<vector>` or any other STL container, no
exceptions, no RTTI, no logging header, and nothing from render-server.

## B. What the emitted serialization must do

| Rule | Enforced by |
|---|---|
| Every read of a frame goes through `WireReader`, which bounds-checks and latches its failure | the emitter, and the fuzz target |
| A decode failure is a value, never a trap or an exception | the project's `av::Status` convention; the emitted code returns the reader's status |
| A command id that is not in the table decodes to nothing and says so | the generated dispatch's default case |
| Sizes taken from the frame are checked before they are used to advance | `WireReader`'s own contract |

## C. What the gates do with it

| Gate | Treatment |
|---|---|
| the compiler | full: `-Wall -Wextra -Werror -Wconversion`, freestanding, no exceptions or RTTI |
| `tools/check_design.py` | skips `generated/` directories by an explicit rule, because a registry's shape is not this project's design |
| `tools/check_coverage.py` | the same exclusion; the hand-written code that calls into the generated code is covered as ever |
| `tools/check_generated.py` | re-runs the generator and fails on any difference |
| the fuzz gate | a target drives the generated decoder with a committed corpus |

## D. What a regeneration must not do

- change a protocol id without `docs/protocol.md` changing with it;
- leave a hand-written file inside a `generated/` directory;
- emit a file that only compiles in one of the two builds that include it;
- succeed while a selected command is absent from the registry - the generator
  fails naming it instead.
