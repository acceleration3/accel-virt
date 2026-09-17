# Contract: WSI through rvkvk.dll (046)

Instance extensions: `VK_KHR_get_physical_device_properties2` (2),
`VK_KHR_surface` (25), `VK_KHR_win32_surface` (6). Device extension:
`VK_KHR_swapchain` (70), with its policy row naming its test.

| Entry | Answer |
|---|---|
| `vkCreateWin32SurfaceKHR` | a surface for `hwnd`; `VK_ERROR_INITIALIZATION_FAILED` for a null window |
| `vkGetPhysicalDeviceSurfaceSupportKHR` | `VK_TRUE` for every queue family that has graphics |
| `vkGetPhysicalDeviceWin32PresentationSupportKHR` | as above |
| `vkGetPhysicalDeviceSurfaceCapabilities(2)KHR` | research.md §2; `VK_ERROR_SURFACE_LOST_KHR` for a gone window |
| `vkGetPhysicalDeviceSurfaceFormats(2)KHR` | the four formats, count/array protocol |
| `vkGetPhysicalDeviceSurfacePresentModesKHR` | FIFO, IMMEDIATE |
| `vkCreateSwapchainKHR` | refusals: extent other than current, count outside 2..8, unknown format/mode/usage, 0x0 window |
| `vkGetSwapchainImagesKHR` | count/array protocol |
| `vkAcquireNextImageKHR` | the next image not held; `VK_NOT_READY` when all are held and timeout is 0; `OUT_OF_DATE` when resized |
| `vkQueuePresentKHR` | per swapchain result in `pResults`; the worst as the return |
| `vkDestroySwapchainKHR`, `vkDestroySurfaceKHR` | release everything |
