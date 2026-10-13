/* The C ABI of an executor library: what runs a guest API context's commands
 * on the host's own API for virtio-gpu-av. The device owns this contract, as
 * it owns AvHostOps. The library that implements it is this project's own,
 * built on the generated server dispatch over host API backends (feature 042);
 * the qemu configuration builds av_echo_executor for the tests. The library
 * exports one symbol,
 * AV_EXECUTOR_OPS_SYMBOL, a function answering its table; the QEMU glue loads
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
#ifndef AV_EXECUTOR_H
#define AV_EXECUTOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One version, and the device accepts that one only: the device, the library
 * and the guest are built together, and an older table is a build to fix
 * rather than one to serve. 16 made the library the one place a shared object
 * is known (HostShares): the device names a guest device's objects by a
 * domain it opens, and the library publishes, opens and releases them. 19
 * hands a dma-buf with the device's own host mapping of the same memory. */
#define AV_EXECUTOR_ABI_VERSION 19u
#define AV_EXECUTOR_OPS_SYMBOL "av_executor_ops"

/* The capset's sub_apis bits. */
#define AV_EXECUTOR_SUB_API_VULKAN 1u
#define AV_EXECUTOR_SUB_API_OPENGL 2u
#define AV_EXECUTOR_SUB_API_OPENCL 4u
#define AV_EXECUTOR_SUB_API_D3D 8u

/* What execute answers. */
#define AV_EXECUTED 0u         /* ran; *reply_size bytes of answer, or 0 */
#define AV_UNKNOWN 1u          /* a command id this library does not know: skipped */
#define AV_UNDECODABLE 2u      /* the payload does not decode */
#define AV_DEVICE_LOST 3u      /* the host API lost its device: nothing more will run */

/* What a blob is, to whichever host API is about to be handed it. No single
 * identifier is zero-copy everywhere: Vulkan imports a host pointer on every
 * GPU measured, while OpenGL and OpenCL import a dma-buf and only on some, so
 * carrying one kind would concede a whole API on some hosts. A handle carries
 * either, and the device picks from what import_kinds offers. */
#define AV_BLOB_HOST_POINTER 1u /* `pointer`, page-aligned, stays mapped */
#define AV_BLOB_DMA_BUF 2u      /* `fd`, owned by the callee once imported */

/* What kind of thing a shared object is, for share_object and open_object. The
 * two fence kinds are one host semaphore either way; they differ to the guest,
 * which has two WDDM primitives, and not to the host (feature 053). */
#define AV_SHARED_MEMORY 1u
#define AV_SHARED_MONITORED_FENCE 2u
#define AV_SHARED_NATIVE_FENCE 3u

/* What kind of descriptor a shared object's is, inside the library. Its own
 * vocabulary and not AV_BLOB_*, because a semaphore's descriptor is neither a
 * pointer nor a dma-buf: it is an opaque fd, which only the API that made it
 * understands. Memory may be either, and which one matters - OpenGL and
 * OpenCL import only a dma-buf, while Vulkan takes both. */
#define AV_SHARED_HANDLE_OPAQUE_FD 1u
#define AV_SHARED_HANDLE_DMA_BUF 2u

/* What share_object and open_object answer: done, or why not. */
#define AV_SHARED_DONE 0u
#define AV_SHARED_UNKNOWN 1u     /* no object is shared under the id in this domain */
#define AV_SHARED_MISMATCHED 2u  /* the kind or size disagrees with what is shared */
#define AV_SHARED_CONFLICT 3u    /* the id is shared otherwise, or already held by this executor */
#define AV_SHARED_REFUSED 4u     /* the host API will not, or the library has no room */

/* There is deliberately no offset: the device hands over a handle naming
 * exactly the range the guest asked for, because OpenCL's import has no offset
 * property and an offset here would buy a capability flag per API for nothing. */
typedef struct AvBlobHandle {
    uint32_t kind;     /* exactly one AV_BLOB_* value */
    /* The memory type the guest asked the import to be made as (IMPORT_BLOB,
     * feature 045) - an index into the memory properties of the device the
     * executor's guest made - whichever kind carries it: a dma-buf is imported
     * into a type the same way a host pointer is. The name stays so version 2
     * libraries keep compiling. */
    uint32_t reserved;
    /* AV_BLOB_HOST_POINTER: the memory. AV_BLOB_DMA_BUF: the device's own
     * mapping of the same memory, page-aligned and mapped as long as the
     * descriptor's memory is, or NULL - what a GPU that reads an imported
     * dma-buf without snooping the CPU's caches imports instead. */
    void *pointer;
    int fd;            /* AV_BLOB_DMA_BUF; -1 otherwise */
    uint64_t size;     /* bytes, a whole number of host pages */
} AvBlobHandle;

typedef struct AvExecutor AvExecutor;

/* What a context is made with: the guest device's domain (open_domain), and
 * for OpenGL its version (major 0 is the host's default context, and then
 * every other field is 0), a profile (0 none, 1 core, 2 compatibility) and
 * the WGL/EGL flags (bit 0 debug, 1 forward compatible, 2 robust access, 3
 * lose context on reset). */
#define AV_CONTEXT_PROFILE_CORE 1u
#define AV_CONTEXT_PROFILE_COMPATIBILITY 2u
#define AV_CONTEXT_FLAG_DEBUG 1u
#define AV_CONTEXT_FLAG_FORWARD_COMPATIBLE 2u
#define AV_CONTEXT_FLAG_ROBUST_ACCESS 4u
#define AV_CONTEXT_FLAG_LOSE_CONTEXT_ON_RESET 8u

typedef struct AvContextOptions {
    uint8_t major;
    uint8_t minor;
    uint8_t profile;
    uint8_t flags;
    uint32_t domain;
} AvContextOptions;

/* A published image copied into one of the executor's exportable images: a
 * dma-buf the executor keeps owning (a caller that keeps it duplicates it),
 * which of its images `slot` is, and how the display reads it. */
typedef struct AvExportedImage {
    int32_t  fd;
    uint32_t slot;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t offset;
    uint32_t fourcc;
    uint32_t reserved;
    uint64_t modifier;
} AvExportedImage;

/* One plane of a display head's frame (docs/protocol.md, "Scanout planes"):
 * the image the guest shared as `key`, its `source` rectangle shown in
 * `destination` on the head - both left, top, right, bottom, the clip already
 * applied - and layered by `layer`, 0 the bottom. `flags` are AV_PLANE_*. */
typedef struct AvScanoutPlane {
    uint64_t key;
    int32_t  source[4];
    int32_t  destination[4];
    uint32_t layer;
    uint32_t flags;
} AvScanoutPlane;

/* Scaled with a filter rather than to the nearest pixel. */
#define AV_PLANE_FILTERED 0x1u
/* A YCbCr image converted to RGB as it is drawn: by the BT.709 matrix rather
 * than BT.601's, and from full-range values rather than studio ones. */
#define AV_PLANE_YCBCR 0x2u
#define AV_PLANE_BT709 0x4u
#define AV_PLANE_FULL_RANGE 0x8u

/* DRM_FORMAT_MOD_INVALID: an exported surface's layout is the one its driver
 * keeps with the memory itself, which an importer on the same GPU reads from
 * the dma-buf (export_surface). */
#define AV_EXPORT_MODIFIER_IMPLICIT 0x00FFFFFFFFFFFFFFull

/* A staged frame mapped for reading (map_frame): `height` rows of `width`
 * four-byte pixels, `row_pitch` bytes apart, in the frame's own byte order.
 * Valid until unmap_frame. */
typedef struct AvFrameView {
    const uint8_t *pixels;
    uint32_t row_pitch;
    uint32_t width;
    uint32_t height;
    uint32_t reserved;
} AvFrameView;

/* One rectangle of a staged frame written into a surface (write_frame):
 * `width` by `height` pixels from (`source_x`, `source_y`) of the frame to
 * (`destination_x`, `destination_y`) of the surface. */
typedef struct AvFrameRegion {
    uint32_t source_x;
    uint32_t source_y;
    uint32_t destination_x;
    uint32_t destination_y;
    uint32_t width;
    uint32_t height;
} AvFrameRegion;

typedef struct AvExecutorOps {
    uint32_t abi_version;
    void *opaque;
    /* Which sub-APIs create accepts. Main loop. */
    uint32_t (*sub_apis)(void *opaque);
    /* Which AV_BLOB_* kinds `sub_api` imports on the GPU `gpu_uuid` names, or
     * 0 for "copy at the boundary". On the table rather than on an executor
     * because the capset is described during realize, before any context
     * exists. Main loop. */
    uint32_t (*import_kinds)(void *opaque, uint32_t sub_api, const uint8_t *gpu_uuid);

    /* A domain for one guest device on the GPU whose Vulkan device UUID is
     * `gpu_uuid` (16 bytes): what its contexts are made in and every call
     * below that is not an executor's names. The guest mints its shared ids
     * per device, so a domain is what keeps two devices' ids apart. 0 for a
     * GPU this library has not got. Main loop, as the device realizes. */
    uint32_t (*open_domain)(void *opaque, const uint8_t *gpu_uuid);
    /* The device is going: every object shared in `domain` is released, as a
     * reset releases them, and the domain names nothing after. Main loop. */
    void (*close_domain)(void *opaque, uint32_t domain);

    /* An executor for one context of `sub_api` in the domain `options` names,
     * made with `options` and sharing the objects of `share` (NULL: none), an
     * executor of the same sub-API in the same domain that stays alive until
     * this returns. A sub-API that has no options refuses non-default ones; a
     * version or share the host API refuses answers NULL. Runs on the new
     * context's worker, while `share` may be current on its own. */
    AvExecutor *(*create)(void *opaque, uint32_t sub_api, const AvContextOptions *options, AvExecutor *share);
    void (*destroy)(void *opaque, AvExecutor *executor);
    /* Runs one command: `payload` is the `size` bytes after its 8-byte header.
     * An answer is written to `reply`, at most `reply_capacity` bytes, and its
     * length to *reply_size; an answer longer than the capacity sets
     * *reply_size to its length and writes nothing. */
    uint32_t (*execute)(void *opaque, AvExecutor *executor, uint32_t command_id, const uint8_t *payload,
                        size_t size, uint8_t *reply, size_t reply_capacity, size_t *reply_size);
    /* The last command of a batch has run. An executor may hold what every
     * command of the batch needs - a host device's lock - across the batch
     * rather than taking it per command. */
    void (*end_batch)(void *opaque, AvExecutor *executor);
    /* Between batches, on the context's worker: the executor writes what the
     * host GPU now has for the guest to read in its own memory. 1 while it
     * still waits on the GPU for some of it, 0 when nothing is left. */
    uint32_t (*poll)(void *opaque, AvExecutor *executor);
    /* On the context's worker, after a batch or between batches: the mark the
     * latest GPU work was given, and the highest mark everything up to which
     * the GPU has finished. The device completes a batch only once `reached`
     * passes the `marked` it read after running it, so a guest waiting on the
     * batch's fence waits for its GPU work too. Both 0 for an executor whose
     * commands have finished when they return. */
    void (*gpu_progress)(void *opaque, AvExecutor *executor, uint64_t *marked, uint64_t *reached);
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
    void (*set_signal)(void *opaque, AvExecutor *executor, void (*signal)(void *context), void *context);

    /* Makes `blob_id` usable by commands on this executor. AV_EXECUTED or
     * AV_UNDECODABLE. On success the callee owns an AV_BLOB_DMA_BUF handle's
     * fd and closes it; on failure the caller still owns it. A host pointer is
     * never owned: the device keeps it mapped until release or destroy
     * returns. Runs on the context's worker. */
    uint32_t (*import_blob)(void *opaque, AvExecutor *executor, uint64_t blob_id, const AvBlobHandle *handle);
    /* Drops an import. A no-op for an id that was never imported; destroy
     * releases whatever is left. Runs on the context's worker. */
    void (*release_blob)(void *opaque, AvExecutor *executor, uint64_t blob_id);
    /* The wire handle the import made under `blob_id` answers to in this
     * executor's commands - for Vulkan the VkDeviceMemory the guest binds and
     * frees - or 0 when `blob_id` made none. The device chooses every blob_id
     * and never reuses one while its import lives. Runs on the context's
     * worker. */
    uint64_t (*imported_handle)(void *opaque, AvExecutor *executor, uint64_t blob_id);

    /* The object `wire_handle` names in this executor's commands, shared as
     * the id the guest's driver minted over its allocation, in the executor's
     * domain, as an object of `kind` and `size` bytes: any executor of the
     * domain, of any API, may open it from then on, until the guest releases
     * it (release_shared). An AV_SHARED_* answer. Runs on the context's
     * worker. */
    uint32_t (*share_object)(void *opaque, AvExecutor *executor, uint64_t object_id, uint32_t kind, uint64_t size,
                             uint64_t wire_handle);
    /* The object shared as `object_id` made one of this executor's, answered
     * as the handle the guest names it by from then on: AV_SHARED_DONE with
     * *wire_handle non-zero, or why not. The executor holds it until
     * close_object or destroy, whatever the guest releases meanwhile. Runs on
     * the context's worker. */
    uint32_t (*open_object)(void *opaque, AvExecutor *executor, uint64_t object_id, uint32_t kind, uint64_t size,
                            uint64_t *wire_handle);
    /* What open_object made of `object_id`, destroyed. A no-op for an id this
     * executor did not open; destroy closes whatever is left. Runs on the
     * context's worker. */
    void (*close_object)(void *opaque, AvExecutor *executor, uint64_t object_id);
    /* The guest let go of `object_id` everywhere - every process that made or
     * opened the allocation it was minted over has - or, for 0, of every id
     * in `domain`, as a reset does. What an executor opened stays its own.
     * Main loop. */
    void (*release_shared)(void *opaque, uint32_t domain, uint64_t object_id);

    /* The image shared as `key` in `domain` by a Direct3D session, read into
     * `pixels`: `height` rows of `width` B8G8R8A8 pixels, `stride` bytes
     * apart - what a display head's surface holds. AV_EXECUTED, or AV_UNKNOWN
     * for a key nothing shares. Main loop; blocks until the host GPU has the
     * image. */
    uint32_t (*read_image)(void *opaque, uint32_t domain, uint64_t key, void *pixels, uint32_t stride,
                           uint32_t width, uint32_t height);
    /* The image shared as `key` in `domain`, copied on the GPU into an
     * exportable image and handed over as a dma-buf once the copy is done.
     * AV_EXECUTED, or AV_UNKNOWN where nothing shares the key or the host
     * cannot export. Main loop. */
    uint32_t (*export_image)(void *opaque, uint32_t domain, uint64_t key, AvExportedImage *image);
    /* The frame of the surface the guest shared as `surface` in `domain`, as
     * the present that published `point` on it left it - staged by the
     * session that drew it, in its own order (docs/shared-surfaces.md) -
     * mapped once the GPU has copied it: AV_EXECUTED, or AV_UNKNOWN when none
     * is there within `wait_ms`. Any thread; one map per frame. */
    uint32_t (*map_frame)(void *opaque, uint32_t domain, uint64_t surface, uint32_t point, uint32_t wait_ms,
                          AvFrameView *view);
    /* That frame let go, mapped or not. Any thread. */
    void (*unmap_frame)(void *opaque, uint32_t domain, uint64_t surface, uint32_t point);
    /* The surface the guest shared as `surface` in `domain`, as its own
     * memory, a dma-buf a process outside QEMU imports with no copy
     * (docs/external-export.md): AV_EXECUTED with `image` filled, its fd the
     * caller's to close and `slot` 0, or AV_UNKNOWN for a surface nothing
     * shares, or one whose memory no other API can open. `modifier` is 0,
     * DRM_FORMAT_MOD_LINEAR (docs/external-export.md §6). Any thread. */
    uint32_t (*export_surface)(void *opaque, uint32_t domain, uint64_t surface, AvExportedImage *image);
    /* How many copies into `surface` the host has run since it was shared, in
     * *writes, and in *fd a sync_file, the caller's, that signals once the GPU
     * has finished every one of them: AV_EXECUTED, or AV_UNKNOWN for a
     * surface nothing shares or a GPU that makes no sync_file. Any thread. */
    uint32_t (*sync_surface)(void *opaque, uint32_t domain, uint64_t surface, uint64_t *writes, int *fd);
    /* `count` planes, by layer, composed on the GPU into one `width` by
     * `height` image - black where no plane is - and handed over as
     * export_image hands one over: AV_EXECUTED, or AV_UNKNOWN where a key is
     * shared by nothing or the host cannot compose. Null in an executor that
     * cannot compose, which a device then never offers. Main loop. */
    uint32_t (*compose_planes)(void *opaque, uint32_t domain, const AvScanoutPlane *planes, uint32_t count,
                               uint32_t width, uint32_t height, AvExportedImage *image);
    /* The frame `point` of `surface` written by the GPU into the surface
     * `fd` is, `bytes` long and `pitch` bytes a row - a dma-buf over guest
     * pages, the caller's to close - region by region, and finished: no CPU
     * reads or writes a pixel. AV_EXECUTED, the frame then let go as
     * unmap_frame lets it go; AV_UNKNOWN where the frame is not there within
     * `wait_ms`, a region lies outside the frame or the surface, or its API
     * cannot write a buffer - the frame is still there for map_frame. Null in
     * an executor that writes none. Any thread. */
    uint32_t (*write_frame)(void *opaque, uint32_t domain, uint64_t surface, uint32_t point, uint32_t wait_ms,
                            int fd, uint64_t bytes, uint32_t pitch, const AvFrameRegion *regions, uint32_t count);
} AvExecutorOps;

typedef const AvExecutorOps *(*AvExecutorOpsFunction)(void);

#ifdef __cplusplus
}
#endif

#endif /* AV_EXECUTOR_H */
