# Research: the feature set DXVK and vkd3d-proton need (047)

What was established before planning, with where it is written down. Findings
from reading this repository and the live guest of feature 046; nothing here is
a design decision on its own.

## 1. The translation layers sit above the runtime, not below it

`docs/kb/07-usermode.md:16-34`: vkd3d-proton ships a replacement `d3d12.dll`
and DXVK replacement `d3d11.dll`/`dxgi.dll`/`d3d9.dll`; they call Vulkan
directly, so nothing in this feature touches the D3D UMD DDI (features
055-057). What they need from this project is a Vulkan ICD whose reported
extension and feature set is large enough for their device creation to succeed,
and whose reports are truthful (constitution VI).

## 2. Features and property chains already reach the host untouched

The generated ICD entry points forward `vkGetPhysicalDeviceFeatures` and
`vkGetPhysicalDeviceFeatures2` straight to the host
(`src/proto/generated/vulkan/vk_icd_entries.cpp:107,155`), and
`PhysicalDeviceView` only overwrites the API version and writes the adapter's
LUID into a node the application labelled
(`src/icd/vulkan/physical_device_view.hpp:1-12`). So a *feature* vkd3d-proton
requires - dynamic rendering, synchronization2, timeline semaphores, buffer
device address, descriptor indexing, mutable descriptor types,
demote-to-helper, image view min LOD, push descriptors - is answered by the
host GPU as soon as the structure that carries it is in the generated
serializers, which follows from its extension being in the generator's
profile. **This feature is therefore mostly about the extension set, not about
feature reporting.**

## 3. What the ICD reports today, and what a new row costs

`src/icd/vulkan/icd_root.cpp:105-108`: exactly two device extensions are
reported, `VK_EXT_memory_budget` and `VK_KHR_swapchain`, each with the test
that proves it. `tools/check_extension_policy.py:102-140` holds every row to
two things: the name must be in the generator's implemented list, and the named
test must exist as a `TEST(suite, case)` under `tests/`. So each extension this
feature reports costs a row *and a test that exercises it through the ICD*.

Three names are withheld by `src/icd/common/extension_policy.cpp:10`
(`VK_KHR_surface`, `VK_KHR_win32_surface`, `VK_EXT_external_memory_host`) and
feature 046 added `HostExtensionList`
(`src/icd/vulkan/host_extension_list.hpp`), which keeps the extensions the ICD
implements itself out of what the host is asked to enable.

## 4. The generator implements 68 extensions, and withdraws two it is asked for

The profile is one TOML list, `tools/codegen/profiles/vulkan.toml`
(`[extensions].include`), and its own comments carry what running DXVK taught:
`vulkan.toml:114-125` ("each name missing from it is a device DXVK declines -
'No adapters found'"), `:101-113` (`VK_EXT_descriptor_buffer` deliberately
excluded, "two days of blank frames"), `:141-156` (why `VK_KHR_map_memory2` is
listed: wow64 needs a mapping placed at an address it chose),
`:159-174` (`VK_EXT_map_memory_placed` is the ICD's to append, because
placement decides where in the *guest's* address space pages appear).

`src/proto/generated/vulkan/vk_supported_extensions.h` lists 68, which covers
every extension US1 scenario 1 names except `VK_KHR_map_memory2`. Two the
profile already asks for never reach it: `lower.py:227-276` un-claims an
extension one of whose commands the classifier could not lower.

| Wanted | In the profile | Emitted | Why not |
|---|---|---|---|
| `VK_KHR_map_memory2` | yes | no | `vkMapMemory2KHR` has `void** ppData` |
| `VK_KHR_push_descriptor` | yes | no | `vkCmdPushDescriptorSetWithTemplateKHR` has an unsized `const void* pData` |
| `VK_EXT_image_view_min_lod` | no | no | nothing: no commands, structs already serialize |
| `VK_EXT_mutable_descriptor_type` | no | no | nothing: no commands |
| `VK_EXT_map_memory_placed` | no, deliberately | no | nothing: no commands; the ICD owns it (FR-003) |
| `VK_KHR_external_memory_win32` | no | no | generates, but names `HANDLE` and win32 structs the vendored headers do not declare |
| `VK_KHR_external_semaphore_win32` | no | no | the same |

`VK_KHR_descriptor_update_template` is withdrawn by the same command as
push descriptors, so the two stand or fall together.

Adding a commandless extension is one profile line, a regeneration, a policy
row and its test. A command the classifier rejects has two routes, both in the
repository already: teach `emit/classify.py`/`emit/serialize.py` the shape - the
generator has a weak-hook mechanism for exactly this, `host_image_copy_bytes`
in `vk_host_image_default.cpp`, with strong definitions on each side - or name
the command in `[commands].exclude` and hand-write it as a `local_vk*` entry in
`src/icd/vulkan/local_entries.*` with a row in `local_rows`, which two tests
hold to answering every excluded name (`test_entry_points.cpp:74-88`,
`test_icd_root.cpp:238-260`).

The host side needs no per-extension work: `RealVulkanDispatch` resolves an
extension command once through `vkGetInstanceProcAddr` and caches it
(`emit/server_backends.py:176-213`), `VulkanBackend::knows` answers from the
generated name table, and the guest's enabled list is forwarded into the host's
`vkCreateDevice`. `VulkanBackend`'s own header says widening the profile is
meant to be "a behaviour change instead of a build break".

## 5. Where the required lists can be read from

FR-001 wants the lists extracted from the pinned releases at test time rather
than hand-copied. Both projects declare them in their own source, in a table of
names with a required/optional flag; **which file and which shape is not
verified here** - there is no network in this environment, so the parser is
written against the pinned archive when the pin is first fetched, and the
extractor's test drives it over a small archive the test builds itself rather
than over the real release. What the extractor may not do is carry a
hand-copied list (FR-001).

The pins are therefore two source archives (for the lists) and two binary
archives (for the DLLs the guest run needs). `tools/fetch_vulkan_runtime.py` is the
pattern to follow: pins as data in `tools/*_pins.txt`, a SHA-256 per file,
nothing written when a hash disagrees, and `--check` for "are the files on disk
the pinned ones". Python here is 3.14 with `compression.zstd` in the standard
library, so vkd3d-proton's `.tar.zst` release needs no new dependency.

## 6. What the guest run needs beyond the DLLs

A D3D application. The runner has no D3D program to drive, so this feature
writes them, in the shape feature 045's `rvkvkprobe` and feature 050's
`rvkglprobe` already have: a program with no C runtime that prints one `ok
<api>.<step>` line per step, a summary line, and keeps a BMP of what it drew
(`docs/testing.md`, "Render pictures"). Feature 046 established that a program
that needs a window must run in the console session, not the agent's
(`ConsoleProgram`, `docs/testing.md`), which these samples do.

## 7. Win32 external memory and semaphores on a Linux host

The host executor has the FD-based extensions in its profile
(`VK_KHR_external_memory_fd`, `VK_KHR_external_semaphore_fd`,
`VK_EXT_external_memory_dma_buf`), and no host driver on Linux has the Win32
ones. US3 is limited to one process, and feature 053 owns cross-process
identity. So the Win32 handle types are the ICD's and the device's to answer:
an export names a host object that the host side already holds, and an import
in the same process finds it again. What a handle *is* to the guest - an NT
handle from rvkgpu - is FR-005's, and only the in-process case is proven here.

## 8. Open questions this feature must answer while planning

- Whether `VK_EXT_descriptor_buffer` stays excluded (FR-002 says yes until a
  spec change) and `VK_EXT_host_image_copy` stays withheld (yes).
- Whether the 32-bit ICD (FR-004) can be built with the existing toolchain
  files, and what in the wire protocol assumes a 64-bit pointer. One is already
  known: the generated code would write a win32 `HANDLE` to the wire as a POD,
  and the width audit's `MACHINE_WIDTH_TYPES` (`emit/classify.py:45-48`) does
  not know the type, so a 32-bit guest and a 64-bit host would disagree on its
  size.
- Whether FR-006's heap corruption on "render-server's current transport" is
  reproducible here at all: this repository owns its transport (feature 043),
  so the claim has to be tested before it can be fixed.

## 9. Placement is not this ICD's to give either

FR-003 asks for `VK_EXT_map_memory_placed` "implemented over host-visible
allocations". It cannot be, and the reason is the one feature 041 already
recorded for the same layer of the stack.

The extension's whole content is that the pointer `vkMapMemory2` returns *is*
the caller's `pPlacedAddress`. This ICD's host-visible memory is a chunk of
guest RAM locked through D3DKMT: `HostVisibleChunk::lock`
(`src/guest/transport/host_visible_chunk.hpp:30-46`) calls
`D3DKMTLockAllocation`, and the process's view is the address **VidMm chose** -
`DeviceMemory::map` (`src/icd/vulkan/device_memory.cpp:104-113`) hands out
`placed->view() + offset` and has nothing else to hand out. Nothing in
`D3DKMT_LOCKFLAGS` or `D3DKMT_LOCK2FLAGS` takes an address, and
`specs/041-kmd-host-visible-memory/tasks.md:79-107` (T023) is the same finding
one level down: "the view's address is VidMm's alone".

Mapping the same pages a second time at an address of our choosing needs a
section object to map (`MapViewOfFile3` with `MEM_REPLACE_PLACEHOLDER`), and a
D3DKMT allocation is not one.

So `VK_KHR_map_memory2` is reported - it is a shape the ICD answers, and it is
what an application probes for - and `VK_EXT_map_memory_placed` is not:
reporting it would be claiming placement the ICD cannot perform, which is
exactly what constitution VI forbids and what
`tools/codegen/profiles/vulkan.toml:159-174` says was tried once already
("It was in this list for a while with no entry points behind it at all, which
is strictly worse than not advertising it").

What that costs is the WoW64 mapping path: winevulkan asks for placement
because a 32-bit process needs the mapping inside its own address space. That
is feature 061's problem together with the rest of the 32-bit ICD, and it is
recorded there rather than half-answered here.
