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
 * reaches the guest without anyone polling for it (feature 052). Version 6
 * adds the three calls that move one host object between two executors, which
 * is what gives an object an identity the whole device agrees on rather than
 * one context (feature 053). The device
 * accepts all six: a version 1 library imports nothing, a version 2 library
 * imports nothing a guest can name, a version 3 library makes only default
 * contexts, a version 4 library signals nothing - a guest on one waits for
 * its events at a synchronising call instead - and a version 5 library shares
 * nothing, which the device answers as a refusal rather than a fault. */
#define RVK_EXECUTOR_ABI_VERSION 11u
#define RVK_EXECUTOR_ABI_VERSION_MINIMUM 1u
/* The versions that brought each part of the table, which the device reads no
 * further than a library's abi_version allows. */
#define RVK_EXECUTOR_ABI_VERSION_IMPORTS 2u
#define RVK_EXECUTOR_ABI_VERSION_HANDLES 3u
#define RVK_EXECUTOR_ABI_VERSION_OPTIONS 4u
#define RVK_EXECUTOR_ABI_VERSION_EVENTS 5u
#define RVK_EXECUTOR_ABI_VERSION_SHARING 6u
#define RVK_EXECUTOR_ABI_VERSION_IMAGES 7u
#define RVK_EXECUTOR_ABI_VERSION_BATCHES 8u
#define RVK_EXECUTOR_ABI_VERSION_EXPORTS 9u
#define RVK_EXECUTOR_ABI_VERSION_POLL 10u
#define RVK_EXECUTOR_ABI_VERSION_PROGRESS 11u
#define RVK_EXECUTOR_OPS_SYMBOL "rvk_executor_ops"

/* The capset's sub_apis bits. */
#define RVK_EXECUTOR_SUB_API_VULKAN 1u
#define RVK_EXECUTOR_SUB_API_OPENGL 2u
#define RVK_EXECUTOR_SUB_API_OPENCL 4u
#define RVK_EXECUTOR_SUB_API_D3D 8u

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

/* What kind of thing a shared object is, for the version 6 calls below. The
 * two fence kinds are one host semaphore either way; they differ to the guest,
 * which has two WDDM primitives, and not to the host (feature 053). */
#define RVK_SHARED_MEMORY 1u
#define RVK_SHARED_MONITORED_FENCE 2u
#define RVK_SHARED_NATIVE_FENCE 3u

/* What kind of descriptor a shared object's is. Its own vocabulary and not
 * RVK_BLOB_*, because a semaphore's descriptor is neither a pointer nor a
 * dma-buf: it is an opaque fd, which only the API that made it understands.
 * Memory may be either, and which one matters - OpenGL and OpenCL import only
 * a dma-buf, while Vulkan takes both. */
#define RVK_SHARED_HANDLE_OPAQUE_FD 1u
#define RVK_SHARED_HANDLE_DMA_BUF 2u

/* There is deliberately no offset: the device hands over a handle naming
 * exactly the range the guest asked for, because OpenCL's import has no offset
 * property and an offset here would buy a capability flag per API for nothing. */
typedef struct RvkBlobHandle {
    uint32_t kind;     /* exactly one RVK_BLOB_* value */
    /* The memory type the guest asked the import to be made as (IMPORT_BLOB,
     * feature 045) - an index into the memory properties of the device the
     * executor's guest made - whichever kind carries it: a dma-buf is imported
     * into a type the same way a host pointer is. The name stays so version 2
     * libraries keep compiling. */
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

/* A published image copied into one of the executor's exportable images: a
 * dma-buf the executor keeps owning (a caller that keeps it duplicates it),
 * which of its images `slot` is, and how the display reads it. */
typedef struct RvkExportedImage {
    int32_t  fd;
    uint32_t slot;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t offset;
    uint32_t fourcc;
    uint32_t reserved;
    uint64_t modifier;
} RvkExportedImage;

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

    /* Version 6 onwards. A version 5 table ends above. */

    /* A descriptor for the object `wire_handle` names in this executor's
     * commands, so the device can hand it to another executor: RVK_EXECUTED
     * with *fd owned by the caller and *handle_kind saying which RVK_BLOB_* kind
     * it is, or RVK_UNKNOWN for a handle this executor did not make, a kind it
     * cannot export, or a host API that exports nothing (only Vulkan does).
     * Runs on the context's worker. */
    uint32_t (*export_object)(void *opaque, RvkExecutor *executor, uint32_t kind, uint64_t wire_handle,
                              uint32_t *handle_kind, int *fd);
    /* That descriptor made into an object of this executor's, answered as the
     * handle the guest names it by from then on: RVK_EXECUTED with
     * *wire_handle non-zero, or RVK_UNKNOWN.
     *
     * `fd` stays the caller's whatever the answer - unlike import_blob's,
     * which is made for one import and owned by the callee. A shared object's
     * descriptor is the device's for as long as the object lives, so an
     * importer that keeps one duplicates it. Runs on the context's worker. */
    uint32_t (*import_object)(void *opaque, RvkExecutor *executor, uint32_t kind, uint32_t handle_kind,
                              int fd, uint64_t size, uint64_t *wire_handle);
    /* What import_object made under `kind`, destroyed. The kind travels with
     * it because a host API frees memory and a semaphore by different calls
     * and the handle alone does not say which it is. A no-op for a handle this
     * executor never made that way; destroy releases whatever is left. Runs on
     * the context's worker. */
    void (*release_object)(void *opaque, RvkExecutor *executor, uint32_t kind, uint64_t wire_handle);

    /* Version 7 onwards. A version 6 table ends above. */

    /* The host image a guest allocation shows, published under `key` by a
     * Direct3D session on the GPU `gpu_uuid` names, read into `pixels`:
     * `height` rows of `width` B8G8R8A8 pixels, `stride` bytes apart - what a
     * display head's surface holds. RVK_EXECUTED, or RVK_UNKNOWN for a key no
     * session published. Runs on QEMU's main loop and blocks until the host GPU
     * has the image. */
    uint32_t (*read_image)(void *opaque, const uint8_t *gpu_uuid, uint64_t key, void *pixels, uint32_t stride,
                           uint32_t width, uint32_t height);

    /* ABI 8: the last command of a batch has run. An executor may hold what
     * every command of the batch needs - a host device's lock - across the
     * batch rather than taking it per command. */
    void (*end_batch)(void *opaque, RvkExecutor *executor);

    /* ABI 9: the image published under `key`, copied on the GPU into an
     * exportable image and handed over as a dma-buf once the copy is done.
     * RVK_EXECUTED, or RVK_UNKNOWN where no session published the key or the
     * host cannot export. Main loop. */
    uint32_t (*export_image)(void *opaque, const uint8_t *gpu_uuid, uint64_t key, RvkExportedImage *image);

    /* ABI 10: between batches, on the context's worker: the executor writes
     * what the host GPU now has for the guest to read in its own memory. 1
     * while it still waits on the GPU for some of it, 0 when nothing is left. */
    uint32_t (*poll)(void *opaque, RvkExecutor *executor);

    /* ABI 11: on the context's worker, after a batch or between batches: the
     * mark the latest GPU work was given, and the highest mark everything up
     * to which the GPU has finished. The device completes a batch only once
     * `reached` passes the `marked` it read after running it, so a guest
     * waiting on the batch's fence waits for its GPU work too. Both 0 for an
     * executor whose commands have finished when they return. */
    void (*gpu_progress)(void *opaque, RvkExecutor *executor, uint64_t *marked, uint64_t *reached);
} RvkExecutorOps;

typedef const RvkExecutorOps *(*RvkExecutorOpsFunction)(void);

#ifdef __cplusplus
}
#endif

#endif /* RVK_EXECUTOR_H */
