# Contract: how the Windows Vulkan loader reaches `rvkvk.dll`

Source of truth for the loader's side: Khronos Vulkan-Loader `bde79ad2dd83`
(research.md §1). This contract is what the INF, the manifest and the DLL
promise it.

## Registration (INF)

In the software key of every rvkgpu adapter:

| Value | Type | Data |
|---|---|---|
| `VulkanDriverName` | `REG_MULTI_SZ` (`0x00010000`) | `%13%\rvkvk.json` |

No `HKLM\SOFTWARE\Khronos\Vulkan\Drivers` value is written, and no environment
variable is set. `rvkvk.dll` and `rvkvk.json` are copied to DIRID 13 with
`rvkgpu.sys`, and are covered by the package catalog.

## Manifest (`rvkvk.json`)

```json
{"file_format_version": "1.0.1",
 "ICD": {"library_path": ".\\rvkvk.dll", "api_version": "1.3.0", "library_arch": "64"}}
```

## Exports

Exactly two, undecorated, from a `.def`:

| Export | Behaviour |
|---|---|
| `vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t *pVersion)` | a request of 7 or more answers 7 and `VK_SUCCESS`; 5 or 6 answer themselves and `VK_SUCCESS`; below 5 answers `VK_ERROR_INCOMPATIBLE_DRIVER`; a null pointer answers `VK_ERROR_INCOMPATIBLE_DRIVER` |
| `vk_icdGetInstanceProcAddr(VkInstance, const char *pName)` | with a null instance: `vkCreateInstance`, `vkEnumerateInstanceExtensionProperties`, `vkEnumerateInstanceVersion`, `vk_icdNegotiateLoaderICDInterfaceVersion`, `vk_icdGetPhysicalDeviceProcAddr`, `vk_icdEnumerateAdapterPhysicalDevices`, else null; with an instance this ICD made: any entry point in the `EntryTable`, else null; with another pointer: null |

## Entry points answered through `vk_icdGetInstanceProcAddr`

| Entry point | Behaviour |
|---|---|
| `vkEnumerateInstanceExtensionProperties(NULL, ...)` | zero extensions; a layer name answers `VK_ERROR_LAYER_NOT_PRESENT` |
| `vkEnumerateInstanceVersion` | `VK_API_VERSION_1_3` |
| `vkCreateInstance` | opens a `Connection` on the rvkgpu adapter and creates the host instance; no rvkgpu adapter, or one whose capset offers no Vulkan, answers `VK_ERROR_INCOMPATIBLE_DRIVER` |
| `vk_icdEnumerateAdapterPhysicalDevices(instance, luid, count, devices)` | the instance's physical device when `luid` is its connection's adapter; otherwise a count of 0 and `VK_SUCCESS`; count/array semantics as `vkEnumeratePhysicalDevices`; the handle is the one `vkEnumeratePhysicalDevices` answers |
| `vk_icdGetPhysicalDeviceProcAddr(instance, name)` | a physical-device-level entry point of the `EntryTable`, else null |

## Dispatchable objects

Instances, physical devices, devices, queues and command buffers are
`LoaderHandle`s: a plain structure whose first pointer-sized field holds
`ICD_LOADER_MAGIC` (`0x01CDC0DE`) when handed out; the loader may overwrite it.
No virtual function is reachable through the handle's address.

## Loading and unloading

The DLL's entry does nothing at attach or detach beyond the loader lock's rules:
no thread creation, no wait, no allocation. The ICD root is constructed on the
first `vk_icdGetInstanceProcAddr` call and tears down nothing at process detach
(an instance the application leaked is the process's to lose, as on any ICD).
