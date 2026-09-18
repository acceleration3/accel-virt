# Contract: Win32 external memory and semaphores, in one process (047)

## What is reported

`VK_KHR_external_memory_win32` and `VK_KHR_external_semaphore_win32` are
reported only when the host GPU has the FD-based equivalents this path builds
on (`VK_KHR_external_memory_fd`, `VK_KHR_external_semaphore_fd`), which is what
makes the report truthful (constitution VI).

`vkGetPhysicalDeviceExternalBufferProperties` and
`vkGetPhysicalDeviceExternalSemaphoreProperties` answer
`VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT` and
`VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT` with export and import
both supported, and every other Win32 handle type absent - no
`..._WIN32_KMT_BIT`, no `..._D3D11_TEXTURE_BIT`, no `..._D3D12_...`.

## What a handle is

A handle is this ICD's, not the operating system's: an opaque value that
`ExternalObjectTable` can turn back into the host object it was exported for.
It is not a kernel object until FR-005 is carried by feature 053's shared
allocations, and `vkGetMemoryWin32HandleKHR` therefore does not hand out a
value another process could open - a handle whose `name` is not null is
refused, and so is one this process did not export.

## The sequence US3 proves

1. A device exports memory: `vkGetMemoryWin32HandleKHR` with
   `VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT`.
2. A second device in the same process imports it through
   `VkImportMemoryWin32HandleInfoKHR` in its `vkAllocateMemory` chain.
3. Both read the same pixels: what the first device wrote, the second sees.
4. The same for a timeline semaphore: exported by the first, imported by the
   second, and a wait on one woken by a signal on the other.

## Refusals

| Situation | Answer |
|---|---|
| a handle this process did not export | `VK_ERROR_INVALID_EXTERNAL_HANDLE` |
| a named handle (`name != nullptr`) | `VK_ERROR_INVALID_EXTERNAL_HANDLE` |
| a handle type not in the properties above | `VK_ERROR_INVALID_EXTERNAL_HANDLE` |
| a host without the FD extensions | the Win32 extensions are not reported at all |
| an object destroyed while a handle names it | the handle is forgotten with it |
