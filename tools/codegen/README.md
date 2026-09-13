# The generators

`rvk_codegen` reads a Khronos registry and writes this project's serialization,
dispatch and protocol ids as C++. It is run in place:

```bash
PYTHONPATH=tools/codegen python3 -m rvk_codegen \
    --xml third_party/khronos/vk.xml \
    --profile tools/codegen/profiles/vulkan.toml \
    --ids tools/codegen/profiles/vulkan_ids.toml \
    --header third_party/khronos/include/vulkan/vulkan_core.h \
    --out src/proto/generated/vulkan
```

`--check` re-emits into a temporary directory and exits non-zero on any
difference; `tools/check_generated.py` is that, for every API, as a ctest gate.

## Where it came from, and what that means

It began as render-server's `codegen/rvk_codegen`, taken at revision
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
