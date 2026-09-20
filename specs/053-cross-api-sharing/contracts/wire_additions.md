# Contract: what 053 adds to the wire and to the executor ABI (053)

## Transport commands (API 0)

Three ids join NOP, IMPORT_BLOB and RELEASE_BLOB. The device answers all three
itself, whichever executor library is loaded, so they behave the same under
every one.

| Id | Command | Body | Answer |
|--:|---|---|---|
| `0x00000004` | SHARE_OBJECT | 24 bytes: u64 object_id, u32 kind, u32 reserved, u64 wire_handle | 16 bytes: u32 status, u32 reserved, u64 zero |
| `0x00000005` | OPEN_SHARED_OBJECT | the same 24 bytes, wire_handle 0 | 16 bytes: u32 status, u32 reserved, u64 wire_handle |
| `0x00000006` | RELEASE_SHARED_OBJECT | the same 24 bytes, wire_handle 0 | 16 bytes: u32 status, u32 reserved, u64 zero |

A body of any length but 24 is command framing - fault 5, the context stops -
exactly as a NOP with a body is. Every other refusal is a status in the answer
and the context runs on. The status table is in `data-model.md`.

**SHARE_OBJECT** publishes an object this context's guest already made. The
device asks the context's executor for a descriptor of `wire_handle`, records
it under `object_id`, and counts this context as its first holder. Publishing
an id this device already holds is refused unless it is the same context
publishing the same handle again, which answers ok and changes nothing.

**OPEN_SHARED_OBJECT** hands this context's executor a `dup` of that
descriptor and answers the handle the executor made of it - a `VkDeviceMemory`,
a GL memory object name, a `cl_mem` - which the guest names in that API's own
commands from then on. A context opening an id it already holds is refused
(status 6): a second handle for one object in one context would be two objects
the guest could free twice.

**RELEASE_SHARED_OBJECT** destroys what this context made and drops its
reference. An id this context never held answers ok and does nothing, as
RELEASE_BLOB does.

A context being destroyed releases everything it still holds. The device's last
holder dropping closes the descriptor.

## Allocation private driver data

Private data of exactly `AllocationDescriptor::encoded_size` (48) bytes is an
allocation that is not shared. Private data of 88 bytes is a descriptor
followed by a `SharedObjectRecord` (data-model.md); any other length is refused
as it is today.

`DxgkDdiCreateAllocation` mints the id into the record it was handed and writes
it back, so the value the runtime keeps is the one the opening process gets.

## Executor ABI version 6

```c
#define RVK_EXECUTOR_ABI_VERSION 6u
#define RVK_EXECUTOR_ABI_VERSION_SHARING 6u

#define RVK_SHARED_MEMORY 1u
#define RVK_SHARED_MONITORED_FENCE 2u
#define RVK_SHARED_NATIVE_FENCE 3u

/* A descriptor for an object this executor's guest made. RVK_EXECUTED with
 * *fd owned by the caller, or RVK_UNKNOWN when this API exports nothing of
 * that kind. Runs on the context's worker. */
uint32_t (*export_object)(void *opaque, RvkExecutor *executor, uint32_t kind,
                          uint64_t wire_handle, uint32_t *handle_kind, int *fd);
/* That descriptor made into an object of this executor's, answered as the
 * handle the guest names it by. RVK_EXECUTED with *wire_handle non-zero, or
 * RVK_UNKNOWN. `fd` stays the caller's, whatever the answer - unlike
 * import_blob's, which is made for one import and owned by the callee: a
 * shared object's descriptor is the device's for as long as the object lives,
 * and every importer duplicates it. Runs on the context's worker. */
uint32_t (*import_object)(void *opaque, RvkExecutor *executor, uint32_t kind,
                          uint32_t handle_kind, int fd, uint64_t size,
                          uint64_t *wire_handle);
/* That object destroyed. A no-op for a handle this executor never made.
 * Runs on the context's worker. */
void (*release_object)(void *opaque, RvkExecutor *executor, uint32_t kind, uint64_t wire_handle);
```

`handle_kind` is `RVK_SHARED_HANDLE_DMA_BUF` or `RVK_SHARED_HANDLE_OPAQUE_FD` and says what the
descriptor is, because a GL or CL import takes only a dma-buf while Vulkan
takes either (research.md §7). An importer that cannot take the kind the
publisher exported answers `RVK_UNKNOWN`, which is status 4.

A library of version 5 or below is never asked: the device answers status 4
without calling, as it does for `import_blob` on a version 2 library.

## What the three backends implement

| Backend | `export_object` | `import_object` |
|---|---|---|
| Vulkan | `vkGetMemoryFdKHR` / `vkGetSemaphoreFdKHR` | `VkImportMemoryFdInfoKHR` / `vkImportSemaphoreFdKHR` |
| OpenGL | `RVK_UNKNOWN` (GL exports nothing) | `glImportMemoryFdEXT` / `glImportSemaphoreFdEXT` |
| OpenCL | `RVK_UNKNOWN` (CL exports nothing) | `CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR` |

## OpenGL commands added to the generator

Six, in `tools/codegen/profiles/opengl.toml`'s `extra`, with append-only ids:

```
glBufferStorageMemEXT        glTextureStorageMem2DEXT
glDeleteMemoryObjectsEXT     glDeleteSemaphoresEXT
glSignalSemaphoreEXT         glWaitSemaphoreEXT
```

`glCreateMemoryObjectsEXT`, `glGenSemaphoresEXT` and the two
`glImport*Win32HandleEXT` are deliberately absent: the device makes the object
inside `import_object` and answers its name, and a Win32 `HANDLE` is a value
only the guest kernel resolves.

## Extension reporting

| Extension | Reported when |
|---|---|
| `VK_KHR_external_memory_win32` | the host has `VK_KHR_external_memory_fd` (047's rule, unchanged) |
| `VK_KHR_external_semaphore_win32` | the host has `VK_KHR_external_semaphore_fd` |
| `GL_EXT_memory_object` / `GL_EXT_memory_object_win32` | the context's host lists `GL_EXT_memory_object_fd` |
| `GL_EXT_semaphore` / `GL_EXT_semaphore_win32` | the context's host lists `GL_EXT_semaphore_fd` |
| `cl_khr_external_memory_win32` | the host device lists `cl_khr_external_memory_dma_buf` |
| `cl_khr_gl_sharing` | the context's device has a GL executor to share with |

**A host extension in the right-hand column is never a row of its own.** The
`_fd` names are the *host's*, and they are the condition a guest row is
reported under - a guest has no file descriptors, which is the whole reason the
Win32 spelling exists. Six rows are reported to a guest and no more:
`GL_EXT_memory_object`, `GL_EXT_memory_object_win32`, `GL_EXT_semaphore`,
`GL_EXT_semaphore_win32`, `cl_khr_external_memory_win32` and
`cl_khr_gl_sharing`, beside the two Vulkan ones 047 already reported. Anything
reading this table to build a conformance subset wants those names and not the
conditions beside them.

### What each row promises, and what 053 answers

Reporting an extension promises every command in it. These do not all keep that
promise yet, and a suite that enforces the base rows before T037 settles it
will call entry points that resolve to null - the trap `tools/codegen/profiles/opengl.toml`
already records for sparse textures ("an extension may be claimed only when
every command it needs is generated"), met again here for real.

| Row | Commands in the registry | Answered after T036 |
|---|--:|--:|
| `GL_EXT_memory_object` | 19 | 4 |
| `GL_EXT_memory_object_win32` | 2 | 1 |
| `GL_EXT_semaphore` | 9 | 4 |
| `GL_EXT_semaphore_win32` | 2 | 1 |

The four answered of `GL_EXT_memory_object` are `glCreateMemoryObjectsEXT`,
`glDeleteMemoryObjectsEXT`, `glBufferStorageMemEXT` and
`glTextureStorageMem2DEXT`; of `GL_EXT_semaphore`, `glGenSemaphoresEXT`,
`glDeleteSemaphoresEXT`, `glSignalSemaphoreEXT` and `glWaitSemaphoreEXT`. The
one missing from each `_win32` row is its `glImport*Win32NameEXT`, which is
refused on purpose: a named section is what another process opens *by name*,
and this path shares by handle.

**T037 decided: none of the four is reported yet** (research.md §12).
`GL_DEDICATED_MEMORY_OBJECT_EXT` and `GL_PROTECTED_MEMORY_OBJECT_EXT` are set
with `glMemoryObjectParameterivEXT` *before* the import, and 053's device makes
the memory object inside `OPEN_SHARED_OBJECT` - so at the moment an application
sets them there is no host object to set them on. Reporting
`GL_EXT_memory_object` would promise an ordering this design cannot keep, and
the registry has `GL_EXT_memory_object_win32` require the base extension, so it
cannot be reported alone either.

**T040 decided: neither OpenCL row is reported either**, which strikes the last
sentence this contract used to end on. `cl_khr_external_memory_win32` is built
in the guest - `ClExternalMemory` and `clCreateBufferWithProperties` take a
Win32 handle all the way to `OPEN_SHARED_OBJECT` - and refused on the host,
because the device's OpenCL backend cannot import a shared object until T019
finds a host whose OpenCL lists `cl_khr_external_memory_dma_buf`.
`cl_khr_gl_sharing` needs the *host's* CL context created against the *host's*
GL context, which belongs to another rvk context that nothing on the wire names
(research.md §18). So a conformance subset carries none of these six rows
today, and each of the three reasons is a different thing to fix.
