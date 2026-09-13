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
| one of the `proto/` interface headers section F names | the runtime the emitted client and ICD call, which this repository owes them |

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

## F. What the generated code asks of this repository (T022)

The emitters were ported to this repository's wire, but the client and ICD
emitters still wrote `#include "rvk/..."` and `::rvk::` names: headers from
render-server, a project this one does not use and will never build. A
generated tree that names them is coupled to a foreign tree by accident. T022
ends that. Two of them were deleted; the rest were **renamed**, and what the
generated code calls on each is written down here, so the feature that
eventually writes the header inherits a stated contract instead of guessing
from a diff.

**063 writes none of these headers.** It stops naming someone else's. Until
the owning feature lands, a generated file that includes one of them does not
compile - which is the honest state, not a regression: those files were
uncompilable before too, against headers that were not even in the tree.

### Deleted outright, with nothing in their place

| Was | Called | Now |
|---|---|---|
| `rvk/instr/log.hpp` | `RVK_TRACE`, `RVK_WARN` | gone |
| `rvk/instr/profiler_macros.hpp` | `RVK_PROF_SCOPE` | gone |

A decode failure is already a `return false` and a refusal is already a
returned status, so nothing about the generated code's behaviour depended on
either. This project logs at the decision point (principle IX), and for the
generated dispatch that point is the hand-written call site feature 042 wires
up - not a macro sprayed through a hundred thousand machine-written lines.

### Renamed, one header per thing, `av::` throughout

| Was | Is | Owed by |
|---|---|---|
| `rvk/protocol/transport.hpp` | `proto/client_transport.hpp` | 043 |
| `rvk/client/blob_arena.hpp` | `proto/client_blob_arena.hpp` | 043 |
| `rvk/client/stub_report.hpp` | `proto/icd_stub_report.hpp` | 045 |
| `rvk/icd/connection.hpp` | `proto/icd_connection.hpp` | 045 |
| `rvk/icd/handle_registry.hpp` | `proto/icd_handle_registry.hpp` | 045 |
| `rvk/apis/opengl/icd/support.hpp` | `proto/gl_icd_support.hpp` | 050 |
| `rvk/apis/opengl/sync_table.hpp` | `proto/gl_sync_table.hpp` | 042 |

The generated ICD wrappers moved with them: they were emitted into
`namespace rvk::icd::generated`, render-server's own ICD namespace, and are
now `av::vkicd::generated` - the same `av::<api>` shape as `av::vkproto`,
`av::clproto` and `av::glproto` beside them. OpenGL's `rvk::glicd` becomes
`av::glicd` for the same reason.

#### `proto/client_transport.hpp` - 043

Every generated client wrapper takes one by reference as its first parameter
(`rvk_vkXxx`, `rvk_clXxx`, `rvk_glXxx`), and calls exactly this much of it:

| Member | Shape at the call site |
|---|---|
| `av::ClientTransport` | a class, passed as `::av::ClientTransport&`; never copied, never stored |
| `.send_async(id, payload)` | `id` is a `kCmd_*` constant (`std::uint32_t`); the result is discarded with `(void)` |
| `.send_sync(id, payload, &reply)` | `reply` is the writer the wrapper then reads the answer out of |
| `av::TransportResult` | an enum with an `Ok` enumerator, compared with `!=` and `==` |

`payload` is where a request's bytes live, and that is 043's decision, not
this feature's. What the generated client does today is default-construct
`::av::WireWriter payload;`, write every field into it, and pass
`payload.bytes()`; it reads the reply back with `::av::WireReader r{reply.bytes()}`.
**`av::WireWriter` has neither of those**: it is a fixed-capacity writer over
a block the caller already owns (`WireWriter(uint8_t*, size_t)`), it does not
grow, and it has no `bytes()`. So 043 owes three things that arrive together -
where a request's bytes live (a ring slot the transport hands out is the
obvious answer), how the writer is handed that block, and how the filled
extent reaches `send_*` and comes back as something a `WireReader` can be
built over. Until it decides, these call sites do not compile, and the
emitters change with the decision rather than ahead of it.

#### `proto/client_blob_arena.hpp` - 043

OpenGL only: the path that sends a texture through lent memory instead of
through the frame.

| Member | Shape at the call site |
|---|---|
| `av::ClientBlobArena::Lease` | a value type, default-constructed `{}`, with `.ok()` and `.data` |
| `ClientBlobArena::for_transport(t)` | reaches the arena belonging to a transport |
| `.ready()` | whether lending is available at all |
| `.take(std::size_t)` | answers a `Lease` |
| `.give_back(lease)` | the frame was never sent |
| `.retire_after_last(lease)` | the frame was sent; the host still has to read it |

`for_transport` is a static reached for, which principle III does not allow in
hand-written code. Generated code is exempt from `check_design.py`, but 043
still owns the spelling: if the arena becomes a member the wrapper is given,
the emitter changes with it.

#### `proto/icd_connection.hpp` - 045

| Member | Shape at the call site |
|---|---|
| `av::icd::Connection` | held as `Connection*`; `conn->transport()` yields the `ClientTransport&` a wrapper sends over |
| `av::icd::acquire_default_connection()` | answers `Connection*`; null means `VK_ERROR_INITIALIZATION_FAILED`. One call site: `vkCreateInstance` |
| `av::icd::release_connection(Connection*)` | two call sites: `vkCreateInstance`'s failure path and `vkDestroyInstance` |

#### `proto/icd_handle_registry.hpp` - 045

| Member | Shape at the call site |
|---|---|
| `av::icd::wrap_server_handle<H>(std::uint64_t, Connection*)` | answers an `H`. `H` is one of the five dispatchable handles: `VkInstance`, `VkPhysicalDevice`, `VkDevice`, `VkQueue`, `VkCommandBuffer` |
| `av::icd::connection_of(H)` | answers `Connection*` for any wrapped dispatchable handle |
| `av::icd::destroy_wrapper(H)` | one call site, `vkDestroyInstance` |
| `av::icd::record_target(VkCommandBuffer)` | answers a `ClientTransport&` - the command buffer's own batch, which appends instead of sending. Every `vkCmd*` wrapper uses it and touches no `Connection` |

`handle_to_wire` is the other half of this and already exists in the generated
tree: `vk_handle_default.cpp` defines a weak identity for each of the five,
and the ICD overrides it to unwrap. A wrapper therefore passes wrapped handles
straight through to `rvk_vkXxx`; the unwrap happens once, in serialization.

#### `proto/icd_stub_report.hpp` - 045

| Member | Shape at the call site |
|---|---|
| `av::icd::stub_called(api, entry_point, why)` | three string literals; OpenCL's `cl_icd_stubs.h` calls it once per refused entry point |
| `av::icd::payload_dropped(api, command, offset)` | OpenGL only: an encode that overran its buffer, reported once with how far it got |

#### `proto/gl_sync_table.hpp` - 042 · `proto/gl_icd_support.hpp` - 050

Both are OpenGL's. The sync table's members are already `av::`-namespaced -
`av::glproto::mint_sync_token`, `sync_for`, `bind_sync`, `forget_sync` - so
only the include path was foreign. The support header is the OpenGL ICD's own
entry points (`av::glicd::wire`, `skip_bind_texture`, `pixel_bytes`,
`level_bytes`, `compressed_bytes`, `offsets_of`, `framebuffer_name`,
`flush_persistent_mappings`, `forget_bindings`, and the rest of that set); 050
owns the list, and a generated call site is the only caller of each.

### What a later emitter change must not do

- name a header outside this repository, in an include or a comment;
- put generated code in a namespace that is not `av::`;
- re-introduce a logging or profiling macro into generated output - if a
  generated path needs to say something, the hand-written caller says it.

## G. What feature 042 settled, and what it changed here

**The no-STL line did not move**, which is what section D left open. The host
executor is built freestanding like everything else here - no standard library,
no exceptions, no RTTI - so the `server_dispatch` emitter does not get its
`std::vector` back, and the arena storage that replaced it is what the emitted
code uses on the host as well as anywhere else.

Four changes to what the generators emit came out of running the output rather
than reading it, and each is a rule for every emitter here:

| What | Why |
|---|---|
| Every handle crosses through `handle_to_wire` and comes back through `wire_to_handle` | The inbound direction used to be a `reinterpret_cast`, so a guest naming any address had the host dereference it. A fuzz target reached `vkDestroyInstance` with a guest-chosen pointer in seconds |
| A decoded field is read through `as_number` wherever its value is used | A field off the wire holds whatever a guest sent, and loading an enum-typed one whose value the enum does not have is undefined. Length expressions did exactly that |
| A string off the wire is terminated by the decoder, and an empty one in an array is an empty string rather than a null | The driver `strcmp`s what it is handed, and the elements of a string array are strings it reads |
| An extension entry point is resolved at first use, never linked | Which extension symbols a loader exports is a property of the machine. One missing OpenCL symbol made the whole executor library unloadable on a host whose loader had no GL sharing |

The OpenGL sync hooks became fields on `RealGl` for the reason `rvk_blob`
already was one: a parameterless hook cannot reach one context's state, and
this project has no file-scope place to keep it.
