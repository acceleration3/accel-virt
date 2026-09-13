# Contract: the `Host` operations the probes need

Every call below is added to `av::Host` (`src/device/host/host.hpp`),
implemented once in the QEMU glue (`src/device/qemu/virtio-gpu-rvk-host.c`,
which already holds the Vulkan instance it enumerates GPUs with) and once in
`MockHost`, which scripts each failure. The C ABI table `RvkHostOps`
(`src/device/abi/rvk_device.h`) gains one function pointer per call, in the
same order; `table_host.cpp` forwards them.

## Exported host GPU memory

```c++
/* Main loop. `size` bytes of memory allocated on the bound host GPU and mapped
 * into this process: its handle, or null with `host_error` naming why. The
 * device's own view is `pointer`. */
[[nodiscard]] virtual void *export_gpu_memory(uint64_t size, void *&pointer, int32_t &host_error) = 0;
virtual void                release_gpu_memory(void *memory) = 0;
```

- The handle is what `place_in_hostmem` accepts, so an exported block travels
  the same path a host-memory block does today.
- `host_error` is the `VkResult` when Vulkan refused, or `errno` when the
  mapping did; 0 when there is no host error to report (no GPU bound, size 0).
- Refusing is normal: a host driver that cannot export answers null, and the
  device carries on with the bit clear.
- The glue must never abort: a failed `vkAllocateMemory`, a failed
  `vkGetMemoryFdKHR` and a failed `mmap` are all "null and a number".

## Importing a window over guest pages

```c++
/* Main loop. The `size` bytes at `pointer` - a direct mapping of guest RAM -
 * imported by the bound host GPU: its handle, or null with `host_error`.
 * `pointer` must stay mapped until the import is released. */
[[nodiscard]] virtual void *import_guest_memory(void *pointer, uint64_t size, int32_t &host_error) = 0;
virtual void                release_guest_memory(void *memory) = 0;
/* Main loop. `size` bytes read back through the import into `buffer`: what the
 * host GPU sees, which is what the probe compares. */
[[nodiscard]] virtual Status read_imported_memory(void *memory, uint8_t *buffer, uint64_t size) = 0;
```

- `import_guest_memory` is `VK_EXT_external_memory_host`'s
  `vkGetMemoryHostPointerPropertiesEXT` plus an import allocation; a host
  without the extension answers null with `host_error` 0.
- `read_imported_memory` exists so the probe can compare what the *GPU* sees
  with what the device wrote, which is the whole claim being tested. It may be
  a mapped read on an integrated GPU and a copy through a staging buffer on a
  discrete one; the device does not care which.

## Threading and lifetime

| Call | Side | May block | Failure |
|---|---|---|---|
| `export_gpu_memory` | main loop | yes (allocation) | null + `host_error` |
| `release_gpu_memory` | main loop | yes | none |
| `import_guest_memory` | main loop | yes | null + `host_error` |
| `release_guest_memory` | main loop | yes | none |
| `read_imported_memory` | main loop | yes | `Status` |

Nothing here is reachable from a worker: the probes run inside realize, and
`require_main_loop` names each of them.

## What `MockHost` must script

- Export: success, refusal with a `VkResult`, refusal with an `errno`, and a
  success whose memory `place_in_hostmem` then refuses.
- Import: success, refusal (no extension), and a success whose readback differs
  from what was written.
- Balance: every handle returned must be released before the test ends, which
  `MockHost` already asserts for host-memory blocks and guest mappings.
