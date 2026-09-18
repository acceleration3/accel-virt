# Contract: vkMapMemory2 (047)

**Corrected while implementing.** Placement is not this ICD's to give - a
chunk's process view is the address `D3DKMTLockAllocation` chose (research.md
§9) - so `VK_EXT_map_memory_placed` is not reported and a caller that asks for
placement is refused rather than given a mapping somewhere else. What follows
is what `vkMapMemory2KHR` does; the sections about answering placement features
stand as the reason they are *not* answered.

`vkMapMemory2KHR` and `vkUnmapMemory2KHR` are local ICD entry points, beside
`vkMapMemory` and `vkUnmapMemory` which are already local: a mapping is guest
memory at a guest address, and nothing about it crosses the wire.

## What the ICD answers

- `VK_KHR_map_memory2` is reported when the host memory this ICD maps is
  reachable at all, which is the same condition `vkMapMemory` already has.
- `VK_EXT_map_memory_placed` is **not** reported: the ICD cannot place a
  mapping, so claiming it would be a capability that is not implemented
  (constitution VI). Nothing answers
  `VkPhysicalDeviceMapMemoryPlacedFeaturesEXT`, and an application that reads
  it gets the host's answer for a feature it cannot use through this driver
  either, because the extension is absent from the reported list.

## What vkMapMemory2 does

Exactly what `vkMapMemory` does, read out of `VkMemoryMapInfoKHR` instead of
five parameters: the pointer it answers is the chunk's own view at the
allocation's offset.

| Situation | Answer |
|---|---|
| a `VkMemoryMapPlacedInfoEXT` in the chain | `VK_ERROR_MEMORY_MAP_FAILED`: placement is refused, never ignored |
| `VK_MEMORY_MAP_PLACED_BIT_EXT` in the flags | `VK_ERROR_MEMORY_MAP_FAILED` |
| any other flag | `VK_ERROR_MEMORY_MAP_FAILED`, as `vkMapMemory` answers |
| the range does not fit the allocation | `VK_ERROR_MEMORY_MAP_FAILED` |
| memory this device did not allocate | `VK_ERROR_MEMORY_MAP_FAILED` |

`vkUnmapMemory2KHR` takes `VkMemoryUnmapInfoKHR` and does what `vkUnmapMemory`
does; any flag in it is refused.
