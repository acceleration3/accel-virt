# Implementation plan: the feature set DXVK and vkd3d-proton need (047)

**Branch**: `047-vulkan-for-d3d-translation` · **Spec**: spec.md · **Research**: research.md

## Summary

Widen what the guest Vulkan ICD carries and reports until DXVK and
vkd3d-proton create devices on it, and prove it with their own required lists
and with D3D applications in the guest. Three mechanisms do the carrying:
descriptor update templates (which size an untyped blob, and which nothing in
the wire carries today), `vkMapMemory2` with placement (the ICD's own, because
placement decides where in the *guest's* address space pages land), and the
Win32 external memory and semaphore handle types (which no host driver on Linux
has, so the ICD and the device answer them between themselves). Everything else
is profile lines, policy rows, and one test per reported extension.

## Technical context

C++23 freestanding (constitution); the generator in `tools/codegen/` owns the
wire (`tools/codegen/profiles/vulkan.toml` is the input, `src/proto/generated/`
the committed output, `generated_sources` the gate); `src/icd/vulkan` for what
the ICD answers itself; `src/device/executor` needs no per-extension work
(`RealVulkanDispatch` resolves an extension command through
`vkGetInstanceProcAddr` and caches it). Tests over IcdRig and ScriptedHost,
then the real host GPU in `tests/loopback/`, then the guest's real-executor
boot. Pinned third-party archives follow `tools/fetch_vulkan_runtime.py`.

## What this feature does not build: the 32-bit ICD

FR-004, US1 scenario 3 and SC-001 name a 32-bit ICD and a 32-bit D3D9 run. The
repository already assigns that work elsewhere and says why:

- `specs/061-driver-package/spec.md:204-208` owns "The 32-bit Vulkan ICD
  (`VulkanDriverNameWow`, `%13%\WoW64\rvkvk32.json`)", with its own acceptance
  criterion.
- `specs/045-vulkan-icd/research.md:252-255` deferred it there: "it needs an
  x86 toolchain, decorated `__stdcall` exports and host-visible chunks below
  4 GiB".
- `specs/041-kmd-host-visible-memory/tasks.md:79-107` (T023) established that
  the last of those is not the miniport's to control at all.

Building it here would duplicate 061 and would be a second feature's worth of
work: a second toolchain and external project, an x86 `_chkstk`, the 64-bit
division helpers a no-CRT x86 image needs (`_aulldiv`, `_aullrem`, `_allmul`,
which `TextBuffer::append_decimal` and `trace_text.cpp` would call), decorated
exports and import libraries, `VulkanDriverNameWow` and a second manifest in
the INF, an x86 loader pin, and a `check_guest_image.py` that accepts PE32.

What **is** 047's of FR-004 is delivered here: the reported set is decided by
tables that do not depend on pointer width, and the two things a WoW64 process
needs from the ICD - `VK_KHR_map_memory2` and `VK_EXT_map_memory_placed` - are
implemented and tested (`tools/codegen/profiles/vulkan.toml:141-174` records
what their absence does to a 32-bit DXVK). The 32-bit *run* of SC-001 is 061's
acceptance, and this plan says so rather than leaving it implied.

## Approach, by user story

**US1, DXVK (P1).** The extensions DXVK requires are in the generated set
already, except the two the classifier withdrew and one it was never asked
for. So: teach the wire descriptor update templates, add
`VK_EXT_image_view_min_lod` and `VK_EXT_mutable_descriptor_type` to the
profile, implement `vkMapMemory2`/`vkUnmapMemory2` as local ICD entries with
`VkMemoryMapPlacedInfoEXT`, and give every extension the ICD is to report a
policy row and the test that row names. The list DXVK treats as required is
read out of a pinned source archive by a new extractor, and a test fails when
a required name is not reported (FR-001).

**US2, vkd3d-proton (P2 in order, P1 in priority).** Push descriptors fall out
of the template work; mutable descriptor types and image view min LOD are
profile lines. Its required list is extracted the same way from its own pinned
archive. `VK_EXT_descriptor_buffer` stays out (FR-002, and
`vulkan.toml:101-113` records the two days it cost), `VK_EXT_host_image_copy`
stays withheld (nothing defines the generator's `host_image_copy_bytes` hook).

**US3, Win32 external objects (P2).** The generated code for the two Win32
extensions names `HANDLE` and four structures the vendored headers do not
declare, so the types come first: a portable record beside
`win32_surface_create_record.hpp`, which is the pattern this repository already
chose for a win32 structure the wire must carry. The handle a guest gets is the
ICD's own, and an import in the same process finds the host object again;
cross-process identity is 053's.

**The guest run.** Three probe programs - D3D9 and D3D11 through DXVK, D3D12
through vkd3d-proton - in the shape of `rvkvkprobe`: no C runtime, one `ok`
line per step, a BMP of what they drew, run in the console session
(`ConsoleProgram`, feature 046). They need the layers' DLLs, which are pinned
release archives.

## Constitution check

- **I**: the spec is written; this plan records the one place two specs
  disagreed (the 32-bit ICD) rather than choosing silently.
- **III**: every class in the spec's table owns state. The new ones are a
  descriptor-template table on each side, a placed-mapping record, an external
  handle table, and the extractors' parsers (Python, outside the C++ rules).
- **IV**: every reported extension has the test its policy row names; the
  template blob and the Win32 records are bytes from outside, so each gets a
  hostile table and a fuzz target.
- **VI**: nothing is reported that is not implemented and tested. The two
  exclusions above stay exclusions, and the 32-bit deferral is recorded.
- **VIII**: no speculative structure - the template mechanism exists because
  three commands need it, not for a future one.

## Complexity tracking

| Deviation | Why | Rejected alternative |
|---|---|---|
| The 32-bit ICD is not built here | 061 owns it, 041 T023 blocks part of it | building it twice |
| A weak generator hook sizes template blobs | the size is the template's, not the call's | carrying the blob as opaque bytes of unknown length |
| Win32 structures as portable records | the vendored headers have no `vulkan_win32.h` | vendoring a platform header the registry pin does not cover |
