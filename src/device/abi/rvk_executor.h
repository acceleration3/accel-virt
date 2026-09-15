/* The C ABI of an executor library: what runs a guest API context's commands
 * on the host's own API for virtio-gpu-rvk. The device owns this contract, as
 * it owns RvkHostOps. The library that implements it is this project's own,
 * built on the generated server dispatch over host API backends (feature 042);
 * the qemu configuration builds rvk_echo_executor for the tests. The library
 * exports one symbol,
 * RVK_EXECUTOR_OPS_SYMBOL, a function answering its table; the QEMU glue loads
 * the library the device's `executor` property names and hands the table over.
 *
 * Threading: sub_apis runs on QEMU's main loop. create, execute and destroy for
 * one executor run on that context's worker thread only, one call at a time.
 * Every call must return: destroying a context, or resetting the device, waits
 * on QEMU's main loop for the worker's call to finish. */
#ifndef RVK_EXECUTOR_H
#define RVK_EXECUTOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version 1 has no memory channel at all: the only bytes an executor can see
 * are one command's payload, so a command naming a blob has nothing to name it
 * with. Version 2 adds the three operations that fix that and changes nothing
 * else (feature 042). Version 3 adds imported_handle, so an import can become
 * memory the guest names in its own commands (feature 045). The device accepts
 * all three: a version 1 library imports nothing, and a version 2 library
 * imports nothing a guest can name. */
#define RVK_EXECUTOR_ABI_VERSION 3u
#define RVK_EXECUTOR_ABI_VERSION_MINIMUM 1u
/* The versions that brought each part of the table, which the device reads no
 * further than a library's abi_version allows. */
#define RVK_EXECUTOR_ABI_VERSION_IMPORTS 2u
#define RVK_EXECUTOR_ABI_VERSION_HANDLES 3u
#define RVK_EXECUTOR_OPS_SYMBOL "rvk_executor_ops"

/* The capset's sub_apis bits. */
#define RVK_EXECUTOR_SUB_API_VULKAN 1u
#define RVK_EXECUTOR_SUB_API_OPENGL 2u
#define RVK_EXECUTOR_SUB_API_OPENCL 4u

/* What execute answers. */
#define RVK_EXECUTED 0u         /* ran; *reply_size bytes of answer, or 0 */
#define RVK_UNKNOWN 1u          /* a command id this library does not know: skipped */
#define RVK_UNDECODABLE 2u      /* the payload does not decode */
#define RVK_DEVICE_LOST 3u      /* the host API lost its device: nothing more will run */

/* What a blob is, to whichever host API is about to be handed it. No single
 * identifier is zero-copy everywhere: Vulkan imports a host pointer on every
 * GPU measured, while OpenGL and OpenCL import a dma-buf and only on some, so
 * carrying one kind would concede a whole API on some hosts. Version 2 carries
 * both and the device picks from what import_kinds offers. */
#define RVK_BLOB_HOST_POINTER 1u /* `pointer`, page-aligned, stays mapped */
#define RVK_BLOB_DMA_BUF 2u      /* `fd`, owned by the callee once imported */

/* There is deliberately no offset: the device hands over a handle naming
 * exactly the range the guest asked for, because OpenCL's import has no offset
 * property and an offset here would buy a capability flag per API for nothing. */
typedef struct RvkBlobHandle {
    uint32_t kind;     /* exactly one RVK_BLOB_* value */
    /* RVK_BLOB_HOST_POINTER: the memory type the guest asked the import to be
     * made as (IMPORT_BLOB, feature 045) - an index into the memory properties
     * of the device the executor's guest made. 0 for RVK_BLOB_DMA_BUF. The name
     * stays so version 2 libraries keep compiling. */
    uint32_t reserved;
    void *pointer;     /* RVK_BLOB_HOST_POINTER; NULL otherwise */
    int fd;            /* RVK_BLOB_DMA_BUF; -1 otherwise */
    uint64_t size;     /* bytes, a whole number of host pages */
} RvkBlobHandle;

typedef struct RvkExecutor RvkExecutor;

typedef struct RvkExecutorOps {
    uint32_t abi_version;
    void *opaque;
    /* Which sub-APIs create accepts. */
    uint32_t (*sub_apis)(void *opaque);
    /* An executor for one context of `sub_api` on the host GPU whose Vulkan
     * device UUID is `gpu_uuid` (16 bytes): NULL on failure. */
    RvkExecutor *(*create)(void *opaque, uint32_t sub_api, const uint8_t *gpu_uuid);
    void (*destroy)(void *opaque, RvkExecutor *executor);
    /* Runs one command: `payload` is the `size` bytes after its 8-byte header.
     * An answer is written to `reply`, at most `reply_capacity` bytes, and its
     * length to *reply_size; an answer longer than the capacity sets
     * *reply_size to its length and writes nothing. */
    uint32_t (*execute)(void *opaque, RvkExecutor *executor, uint32_t command_id, const uint8_t *payload,
                        size_t size, uint8_t *reply, size_t reply_capacity, size_t *reply_size);

    /* Version 2 onwards. A version 1 table ends above, and the device must not
     * read past `execute` unless abi_version is 2 or more. */

    /* Which RVK_BLOB_* kinds `sub_api` imports on the GPU `gpu_uuid` names, or
     * 0 for "copy at the boundary". On the table rather than on an executor
     * because the capset is described during realize, before any context
     * exists; answered on QEMU's main loop. */
    uint32_t (*import_kinds)(void *opaque, uint32_t sub_api, const uint8_t *gpu_uuid);
    /* Makes `blob_id` usable by commands on this executor. RVK_EXECUTED or
     * RVK_UNDECODABLE. On success the callee owns an RVK_BLOB_DMA_BUF handle's
     * fd and closes it; on failure the caller still owns it. A host pointer is
     * never owned: the device keeps it mapped until release or destroy
     * returns. Runs on the context's worker. */
    uint32_t (*import_blob)(void *opaque, RvkExecutor *executor, uint64_t blob_id,
                            const RvkBlobHandle *handle);
    /* Drops an import. A no-op for an id that was never imported; destroy
     * releases whatever is left. Runs on the context's worker. */
    void (*release_blob)(void *opaque, RvkExecutor *executor, uint64_t blob_id);

    /* Version 3 onwards. A version 2 table ends above. */

    /* The wire handle the import made under `blob_id` answers to in this
     * executor's commands - for Vulkan the VkDeviceMemory the guest binds and
     * frees - or 0 when `blob_id` made none. import_blob answers only whether
     * it imported, and the guest needs the handle the import became, so this
     * asks it once the import is made. The device chooses every blob_id and
     * never reuses one while its import lives. Runs on the context's worker. */
    uint64_t (*imported_handle)(void *opaque, RvkExecutor *executor, uint64_t blob_id);
} RvkExecutorOps;

typedef const RvkExecutorOps *(*RvkExecutorOpsFunction)(void);

#ifdef __cplusplus
}
#endif

#endif /* RVK_EXECUTOR_H */
