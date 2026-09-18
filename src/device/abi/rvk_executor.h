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
 * on QEMU's main loop for the worker's call to finish.
 *
 * The one exception is `signal`, which the device gives the library and the
 * library calls back: a host API that completes work on a thread of its own -
 * OpenCL's event callbacks - has no other way to say so. It is the only call
 * in this contract that may come from any thread, and it may arrive after the
 * executor it names has been destroyed (feature 052). */
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
 * memory the guest names in its own commands (feature 045). Version 4 adds
 * create_with_options, so an OpenGL context is made with the version, profile
 * and flags the application asked for and shares another's objects (feature
 * 050). Version 5 adds set_signal, the first thing in this table that runs the
 * other way: the device hands the library something to call when a host API
 * finishes work on a thread of its own, so an OpenCL event's completion
 * reaches the guest without anyone polling for it (feature 052). The device
 * accepts all five: a version 1 library imports nothing, a version 2 library
 * imports nothing a guest can name, a version 3 library makes only default
 * contexts, and a version 4 library signals nothing - a guest on one waits for
 * its events at a synchronising call instead. */
#define RVK_EXECUTOR_ABI_VERSION 5u
#define RVK_EXECUTOR_ABI_VERSION_MINIMUM 1u
/* The versions that brought each part of the table, which the device reads no
 * further than a library's abi_version allows. */
#define RVK_EXECUTOR_ABI_VERSION_IMPORTS 2u
#define RVK_EXECUTOR_ABI_VERSION_HANDLES 3u
#define RVK_EXECUTOR_ABI_VERSION_OPTIONS 4u
#define RVK_EXECUTOR_ABI_VERSION_EVENTS 5u
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

/* What an OpenGL context is made with: its version (major 0 is the host's
 * default context, and then every field is 0), a profile (0 none, 1 core, 2
 * compatibility) and the WGL/EGL flags (bit 0 debug, 1 forward compatible, 2
 * robust access, 3 lose context on reset). */
#define RVK_CONTEXT_PROFILE_CORE 1u
#define RVK_CONTEXT_PROFILE_COMPATIBILITY 2u
#define RVK_CONTEXT_FLAG_DEBUG 1u
#define RVK_CONTEXT_FLAG_FORWARD_COMPATIBLE 2u
#define RVK_CONTEXT_FLAG_ROBUST_ACCESS 4u
#define RVK_CONTEXT_FLAG_LOSE_CONTEXT_ON_RESET 8u

typedef struct RvkContextOptions {
    uint8_t major;
    uint8_t minor;
    uint8_t profile;
    uint8_t flags;
    uint32_t reserved; /* 0 */
} RvkContextOptions;

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

    /* Version 4 onwards. A version 3 table ends above. */

    /* create, made with `options` (NULL: the defaults, as create) and sharing
     * the objects of `share` (NULL: none), an executor of the same sub-API on
     * the same GPU that stays alive until this returns. A sub-API that has no
     * options refuses non-default ones; a version or share the host API
     * refuses answers NULL. Runs on the new context's worker, while `share`
     * may be current on its own. */
    RvkExecutor *(*create_with_options)(void *opaque, uint32_t sub_api, const uint8_t *gpu_uuid,
                                        const RvkContextOptions *options, RvkExecutor *share);

    /* Version 5 onwards. A version 4 table ends above. */

    /* Gives `executor` something to call when the host API finishes work on a
     * thread the device did not give it: `signal(context)` with the pointer
     * handed here, from whatever thread the host API used. NULL for `signal`
     * takes it back.
     *
     * What the device does with it is wake that context's worker; nothing
     * about what completed crosses here, because the guest asks. The call must
     * do almost nothing and must not call back into this table.
     *
     * It may arrive late. A host API that cannot unregister a callback -
     * OpenCL cannot - may call one after destroy, so the device's `context`
     * must stay valid for as long as the device runs, and the device must
     * tolerate a signal for a context that is gone. Runs on the context's
     * worker. */
    void (*set_signal)(void *opaque, RvkExecutor *executor,
                       void (*signal)(void *context), void *context);
} RvkExecutorOps;

typedef const RvkExecutorOps *(*RvkExecutorOpsFunction)(void);

#ifdef __cplusplus
}
#endif

#endif /* RVK_EXECUTOR_H */
