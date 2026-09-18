# Contract: vkMapMemory2 and placed mappings (047)

`vkMapMemory2KHR` and `vkUnmapMemory2KHR` are local ICD entry points, beside
`vkMapMemory` and `vkUnmapMemory` which are already local: a mapping is guest
memory at a guest address, and nothing about it crosses the wire.

## What the ICD answers

- `VK_KHR_map_memory2` is reported when the host memory this ICD maps is
  reachable at all, which is the same condition `vkMapMemory` already has.
- `VK_EXT_map_memory_placed` is appended by the ICD itself, as
  `VK_KHR_swapchain` is: the host's opinion does not filter it, because
  placement decides where in the *guest's* address space the pages appear.
  `HostExtensionList` therefore keeps it out of what the host is asked to
  enable.
- `VkPhysicalDeviceMapMemoryPlacedFeaturesEXT` answers
  `memoryMapPlaced = VK_TRUE`, `memoryMapRangePlaced = VK_FALSE` and
  `memoryUnmapReserve = VK_FALSE`;
  `VkPhysicalDeviceMapMemoryPlacedPropertiesEXT` answers the guest's page size
  as `minPlacedMemoryMapAlignment`.

## What a placed map does

`vkMapMemory2KHR` with a `VkMemoryMapPlacedInfoEXT` in its chain maps the
memory so that the returned pointer *is* `pPlacedAddress`. Without that node it
behaves exactly as `vkMapMemory`.

| Situation | Answer |
|---|---|
| `pPlacedAddress` not aligned to `minPlacedMemoryMapAlignment` | `VK_ERROR_MEMORY_MAP_FAILED` |
| the range does not fit the allocation | `VK_ERROR_MEMORY_MAP_FAILED` |
| `VK_MEMORY_MAP_PLACED_BIT_EXT` set with no placed node | `VK_ERROR_MEMORY_MAP_FAILED` |
| a placed node with the bit clear | the node is ignored, as the extension says |
| already mapped | `VK_ERROR_MEMORY_MAP_FAILED`, as `vkMapMemory` answers |
| memory that is not host visible | `VK_ERROR_MEMORY_MAP_FAILED` |

`vkUnmapMemory2KHR` takes `VkMemoryUnmapInfoKHR`; `VK_MEMORY_UNMAP_RESERVE_BIT_EXT`
is refused, because `memoryUnmapReserve` is answered false.
