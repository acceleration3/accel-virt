# Quickstart: fetching, generating, checking

## Once, per machine

```bash
tools/fetch_khronos_registries.sh      # vk.xml, gl.xml, cl.xml by revision + hash
```

A second run is a no-op. Without it everything still builds; only regenerating
and the generated-tree gate need the registries, and the gate says so when they
are missing.

## Generating

```bash
python3 -m rvk_codegen --xml third_party/khronos/vk.xml \
    --profile tools/codegen/profiles/vulkan.toml \
    --ids tools/codegen/profiles/vulkan_ids.toml \
    --header third_party/khronos/include/vulkan/vulkan_core.h \
    --out src/proto/generated/vulkan
```

Run from `tools/codegen/`, or with `PYTHONPATH=tools/codegen`. The OpenGL and
OpenCL halves are `python3 -m rvk_codegen.opengl` and `.opencl` with their own
profiles.

Commit what changes, together with whatever made it change.

## Checking

```bash
ctest --test-dir build -R generated_sources   # the gate, which is --check
ctest --test-dir build -E tools_tests         # everything else, ~5 s
tools/gate.sh                                 # the full verdict
```

`generated_sources` re-runs every generator into a temporary directory and
compares. It fails naming the file that differs, and skips - saying why - when
the registries have not been fetched.

## What failure looks like

| Symptom | What it means |
|---|---|
| `generated_sources` names a file | the committed tree and the generator disagree: regenerate and commit, or revert the edit |
| `generated_sources` skipped | the registries are not fetched; run the fetch tool |
| a compile error inside `src/proto/generated/` | an emitter emitted something this project's flags refuse - fix the emitter, never the output |
| the fuzz target traps | the generated decoder read past a frame; the corpus entry that did it is the bug report |
