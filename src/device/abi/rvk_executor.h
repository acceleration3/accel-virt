/* The C ABI of an executor library: what runs a guest API context's commands
 * on the host's own API for virtio-gpu-rvk. The device owns this contract, as
 * it owns RvkHostOps; render-server's librvk_codec is meant to implement it
 * over its API backends. The library exports one symbol,
 * RVK_EXECUTOR_OPS_SYMBOL, a function answering its table; the QEMU glue loads
 * the library the device's `executor` property names and hands the table over.
 *
 * Threading: sub_apis runs on QEMU's main loop. create, execute and destroy for
 * one executor run on that context's worker thread only, one call at a time. */
#ifndef RVK_EXECUTOR_H
#define RVK_EXECUTOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RVK_EXECUTOR_ABI_VERSION 1u
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
} RvkExecutorOps;

typedef const RvkExecutorOps *(*RvkExecutorOpsFunction)(void);

#ifdef __cplusplus
}
#endif

#endif /* RVK_EXECUTOR_H */
