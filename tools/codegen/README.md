# The generators

`av_codegen` reads a Khronos registry and writes this project's serialization,
dispatch and protocol ids as C++. It is run in place:

```bash
PYTHONPATH=tools/codegen python3 -m av_codegen \
    --xml third_party/khronos/vk.xml \
    --profile tools/codegen/profiles/vulkan.toml \
    --ids tools/codegen/profiles/vulkan_ids.toml \
    --header third_party/khronos/include/vulkan/vulkan_core.h \
    --out src/proto/generated/vulkan
```

`--check` re-emits into a temporary directory and exits non-zero on any
difference; `tools/check_generated.py` is that, for every API, as a ctest gate.

## Where it came from, and what that means

It began as render-server's `codegen/av_codegen`, taken at revision
`89936e50b6145e75af3553e27d5c8a251787dd62`. **Only the generator was taken.**
None of render-server's runtime is used here - not its buffers, not its
logging, not its client or server - and the emitters are changed to emit this
project's types instead (`src/proto/wire/`), which is the work feature 063
describes.

So this is not vendored code to be kept in sync with an upstream. It is this
repository's, edited here, and the only thing it owes its origin is the
acknowledgement above.

## What must stay true

- **Standard library only.** The generator imports nothing outside Python's
  standard library, which is what lets the gate run on any machine with a
  Python 3.11. A dependency here would make the gate unrunnable where it
  matters most.
- **Never hand-edit the output.** A file under `src/proto/generated/` is what
  this wrote; fix the emitter and regenerate. `tools/check_generated.py` is what
  makes that rule enforceable rather than remembered.
- **A regeneration that changes a protocol id is a wire change**, and
  `docs/protocol.md` changes in the same commit.

## The profiles

`profiles/<api>.toml` selects the commands and extensions that cross the wire;
`profiles/<api>_ids.toml` maps each to its id. Both began as render-server's and
are this project's to edit - a line added to either is a change to what this
project's guests can call, which is why they are committed and reviewed rather
than derived.

## Open: a readable-but-wrong length the refusal does not catch (found by 053)

The generator's safety property is that it **refuses a length it cannot read**
rather than guessing one - a command whose `len` is a `COMPSIZE` the rules do
not understand fails the run and has to be explained in the profile. That is
what keeps an attacker-chosen length out of a decoder, and it works.

It does not cover a length the rules *can* read and read **wrongly**.

`COMPSIZE(pname)` is a form the rules understand: they emit
`pname_count(pname)`, whose `default:` in `gl_sizes.h` is **1**. For a scalar
query that default is right and saves a table entry per enum. For a query whose
data is bytes it is a buffer overrun waiting to happen:
`glGetUnsignedBytevEXT(GL_DEVICE_UUID_EXT, data)` writes **16** bytes, and
nothing in the generator would have said so. Feature 053 found this while
measuring what it would cost to generate the rest of `GL_EXT_memory_object`;
the command is not generated today, so nothing is wrong in the tree - but the
next person to add it inherits the hole.

The sizes, measured against the registry, for whoever adds them:

| pname | bytes |
|---|--:|
| `GL_DEVICE_UUID_EXT` (0x9597) | 16 |
| `GL_DRIVER_UUID_EXT` (0x9598) | 16 |
| `GL_DEVICE_LUID_EXT` (0x9599) | 8 |

**The suggested rule**, narrower than "refuse every unknown pname", which would
make the profile carry an entry for every scalar query in the registry: keep
the default for a value-typed output - `GLint *`, `GLfloat *`, `GLboolean *` -
where a wrong count is a wrong *answer*; refuse it for a `GLubyte *` output,
where a wrong count is a write past what the decode arena handed out. A byte
array whose length the profile has not stated is the only case where the
default is dangerous rather than merely optimistic.

This is the generator's to fix, not any one feature's.
