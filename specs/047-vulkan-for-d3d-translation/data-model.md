# Data model: the feature set DXVK and vkd3d-proton need (047)

## A descriptor update template's layout

`VkDescriptorUpdateTemplateCreateInfo` carries
`descriptorUpdateEntryCount` entries, each with `dstBinding`,
`dstArrayElement`, `descriptorCount`, `descriptorType`, `offset` and `stride`.
Nothing in a later `vkUpdateDescriptorSetWithTemplate` or
`vkCmdPushDescriptorSetWithTemplate` call says how long `pData` is: the length
is `max over entries of (offset + (descriptorCount - 1) * stride + size of one
descriptor of that type)`, where one descriptor is a `VkDescriptorImageInfo`,
`VkDescriptorBufferInfo`, a `VkBufferView`, or - for inline uniform block -
one byte per `descriptorCount`.

`DescriptorTemplateLayout` holds those entries and answers that length. Every
term is 64-bit and overflow-checked: the entries come from an application, so
an `offset` of `SIZE_MAX` or a `stride` that multiplies past the end is a
returned status, never an assert, and never a length the caller then reads.

- Invariant: the length it answers is not smaller than any entry's own end.
- Invariant: an empty template answers zero, and a zero-length blob is what
  crosses the wire for it.

`DescriptorTemplateTable` (guest) keeps one layout per
`VkDescriptorUpdateTemplate` a device created, and forgets it when the template
is destroyed. `HostTemplateTable` (device side) keeps the same thing from the
create command it decoded, and a blob whose length disagrees with its own
layout is a refused frame - the guest's claim is not trusted (constitution V).

## A placed mapping

`VkMemoryMapPlacedInfoEXT` names `pPlacedAddress`, the address the caller wants
the mapping at. `PlacedMapping` holds that address, the chunk the memory lives
in and the length, and is what `local_vkUnmapMemory2` looks up.

- The address must be page-aligned and the length within the chunk; anything
  else is `VK_ERROR_MEMORY_MAP_FAILED`, which is what the extension defines.
- A mapping already placed at that address is not replaced silently.
- Invariant: the table holds no two mappings whose ranges overlap.

## An exported external object

An export is the guest asking for a handle that names a host object. A row of
`ExternalObjectTable` holds the handle, what kind of object it names (memory or
semaphore), and the host object's wire handle.

- A handle is refused unless this process exported it: the table is per ICD
  root, and cross-process identity is feature 053's.
- A handle type this path does not implement is absent from
  `VkExternalMemoryProperties`, so an application never asks for one (FR-002).
- Invariant: a handle appears once, and only while the object it names is
  alive.

## The two Win32 structures the wire must carry

The vendored `vulkan_core.h` has no `vulkan_win32.h`, so the three structures
these extensions use are read through portable records, as feature 046 already
does for `VkWin32SurfaceCreateInfoKHR`
(`src/icd/vulkan/win32_surface_create_record.hpp`):

| Record | Structure | Fields read |
|---|---|---|
| `MemoryWin32HandleRecord` | `VkMemoryGetWin32HandleInfoKHR` | `sType`, `pNext`, `memory`, `handleType` |
| `SemaphoreWin32HandleRecord` | `VkSemaphoreGetWin32HandleInfoKHR` | `sType`, `pNext`, `semaphore`, `handleType` |
| `SemaphoreWin32ImportRecord` | `VkImportSemaphoreWin32HandleInfoKHR` | `sType`, `pNext`, `semaphore`, `flags`, `handleType`, `handle`, `name` |

Each record validates its own `sType` before it reads anything else, and a
`name` that is not null is refused: a named handle is a cross-process object.

## What a layer release says it requires

`LayerRequirements` reads one pinned archive and answers two sets: the
extensions the release treats as required, and the ones it treats as optional.
A name it cannot classify is neither - it is reported, so the extractor's test
fails rather than the list quietly shrinking.

- Invariant: the two sets do not intersect.
- Invariant: every name matches `VK_[A-Z]+_[a-z0-9_]+`, so a parse that picked
  up prose cannot pass.
