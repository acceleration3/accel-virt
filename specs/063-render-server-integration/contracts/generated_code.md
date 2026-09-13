# Contract: what generated code may be

This is what the emitters are changed to satisfy, and what a reviewer checks a
regenerated tree against.

## A. What a file under `src/proto/generated/` may include

| Allowed | Why |
|---|---|
| `<cstdint>`, `<cstddef>`, `<cstring>` | freestanding-safe, already used across `src/proto` |
| the API's own header (`vulkan_core.h`, `GL/gl.h`, `CL/cl.h`) | the types on the wire are the API's |
| `proto/wire/wire_reader.hpp`, `proto/wire/wire_writer.hpp`, `proto/wire/wire_arena.hpp` | this repository's wire, which the emitted serialization is written against |
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

## D. Where an array the frame describes lives (T016)

Two emitters still built their arrays out of `std::vector`:
`server_dispatch` (the host's `dispatch_command`) and `icd_entries` (the
guest's ICD wrappers). Both were **changed**. Neither was moved into a
host-only directory where more would be allowed, because neither needed
more: every one of those vectors was per-call scratch storage for an array
whose length the caller already has.

A heap container is also the wrong shape on the merits. On the server side
the length arrives in a frame a hostile guest wrote, so `resize(n)` is an
allocation the attacker sizes - the one thing a decoder in the QEMU process
must not offer (principle V).

**What `server_dispatch` emits now.** `av::WireArena` - a block the caller
owns, handed to a decode, which the generated deserializers already take -
in place of every `std::vector`. A `std::vector<T> x_storage` is now a
`T* x_storage` with a `std::size_t x_storage_count` beside it, and the
`resize(n)` is:

    x_storage_count = static_cast<std::size_t>(n);
    if (x_storage_count != 0) {
        x_storage = arena.allocate<T>(x_storage_count);
        if (x_storage == nullptr) return false;
        ...zeroed, unless the next line overwrites every byte...
    }

A length the block cannot hold, or one whose size overflows, answers null
and the command is refused - the same outcome as any other malformed frame.
A zero length hands the driver a null pointer, which is what Vulkan asks for
and what an empty vector's `data()` never guaranteed. The count is
`std::size_t` because a byte array's length is a `size_t` on the wire, and
narrowing it to 32 bits would have let a huge length allocate a small block
while the driver was still told the large one.

The arena is the caller's, so `dispatch_command` takes it:

    bool dispatch_command(ServerDispatch&, std::uint32_t cmd_id,
                          av::WireReader&, av::WireWriter& reply,
                          av::WireArena& arena);

which is the same shape the OpenCL dispatch uses. The emitter also stopped
emitting `#include <vector>`, `#include "rvk/instr/log.hpp"` and the
`RVK_TRACE` calls that header served: that logging is render-server's, and a
decode failure is already a `return false`.

**What `icd_entries` emits now.** The ICD is guest user-mode code with no
arena in scope and no length from the wire: the two `std::vector<std::uint64_t>`
were staging for an array of raw server handles whose count the *application*
chose. They are now a fixed block on the stack, `std::uint64_t
staged[kMaxStagedHandles]`, with the count checked against it first and a
larger request answered `VK_ERROR_OUT_OF_HOST_MEMORY` - a refusal Vulkan
already defines, rather than an allocation. `std::array` and `<span>` went
the same way: the table is a plain array, and `wrapper_entries()` returns an
`EntryTable` value carrying the pointer and the count with `begin`, `end` and
`size`, so a caller ranges over it exactly as it ranged over the span.

**The rule for every emitter after this one**: an array whose length comes
off the wire is arena storage, and an array whose length comes from the
application is a fixed block with a stated maximum and a refusal. Neither is
a heap container, and nothing generated may include one.

## E. What a regeneration must not do

- change a protocol id without `docs/protocol.md` changing with it;
- leave a hand-written file inside a `generated/` directory;
- emit a file that only compiles in one of the two builds that include it;
- succeed while a selected command is absent from the registry - the generator
  fails naming it instead.
