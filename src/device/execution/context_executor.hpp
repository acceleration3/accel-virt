/* One context's host executor: configured at setup, created by the context's
 * worker when its ring attaches and destroyed by it before the worker exits,
 * so a host API with thread affinity (a current GL context) sees one thread. */
#ifndef AV_CONTEXT_EXECUTOR_HPP
#define AV_CONTEXT_EXECUTOR_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "proto/context/device_uuid.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/sub_api.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ExecutorLibrary;
class ExecutorShares;
class Host;
struct ContextExecutorProbe;

class ContextSignal;

class ContextExecutor {
public:
    static constexpr size_t uuid_bytes = DeviceUuid::bytes;

    /* `shares` is the device's; an OpenGL executor is published there under
     * context `id` for as long as it lives (feature 050). */
    ContextExecutor(Host &host, const ExecutorLibrary &library, ExecutorShares &shares, uint32_t id);
    /* An executor the worker never destroyed is released here, off its thread. */
    ~ContextExecutor();
    ContextExecutor(const ContextExecutor &) = delete;
    ContextExecutor &operator=(const ContextExecutor &) = delete;

    /* Main loop, at setup: the executor create() makes, with `options`, whose
     * share context is the device's id of the context to share with. */
    void                 configure(SubApi api, const uint8_t *gpu_uuid, const GlContextOptions &options);
    /* Main loop, before create: what the library is to call when a host API
     * finishes work on a thread of its own. */
    void                 signals_through(ContextSignal &signal);
    /* Worker. invalid_state when created or not configured, or for an OpenGL
     * executor that cannot be published, which is given back; no_memory when
     * the library makes none, or there is no executor to share with. */
    [[nodiscard]] Status create();
    /* Worker. */
    void                 destroy();
    /* Worker. ok when it ran, with `reply_size` set; not_found for a command
     * the library does not know; malformed for one it cannot decode;
     * invalid_state for a lost device, an answer no library may give, or no
     * executor. */
    void                 end_batch();
    bool                 poll();
    void                 gpu_progress(uint64_t &marked, uint64_t &reached) const;
    [[nodiscard]] Status execute(uint32_t command_id, const uint8_t *payload, size_t size, uint8_t *reply,
                                 size_t reply_capacity, size_t &reply_size);
    /* Worker. The memory allocated under `memory_key` imported under
     * `blob_id`, with `device_memory` the wire handle the import became:
     * invalid_state without an executor; not_supported, leaving nothing
     * imported, for a library that refuses it or names it nothing. */
    [[nodiscard]] Status import(uint64_t blob_id, uint64_t memory_key, uint64_t &device_memory);
    /* Worker. Nothing without an executor: destroying it released every import. */
    void                 release(uint64_t blob_id);

    /* Worker. The object `wire_handle` names in this context's commands
     * shared as `object_id`, which the library keeps (feature 053). ok;
     * invalid_state without an executor; not_found, invalid_parameter or
     * conflict as the library judged the id; not_supported for an object the
     * host API will not share. */
    [[nodiscard]] Status share_object(uint64_t object_id, uint32_t kind, uint64_t size, uint64_t wire_handle);
    /* Worker. The object shared as `object_id` opened as one of this
     * context's, with `wire_handle` what the guest names it by. Same answers
     * as share_object. */
    [[nodiscard]] Status open_object(uint64_t object_id, uint32_t kind, uint64_t size, uint64_t &wire_handle);
    /* Worker. What open_object made of `object_id` let go of. Nothing
     * without an executor: destroying it closed everything it opened. */
    void                 close_object(uint64_t object_id);
    /* The API half (id >> 16) of the commands this context runs. */
    uint16_t             command_api() const;
    bool                 created() const;

    bool invariants_hold() const;

private:
    AvExecutor *make() const;
    /* What the library's AV_SHARED_* answer is to the device. */
    Status      status_of(uint32_t answered) const;

    Host                  &host_;
    const ExecutorLibrary &library_;
    /* The landing pad a host thread's completion reaches this context
     * through; null for a context nothing signals. */
    ContextSignal         *signal_;
    ExecutorShares        &shares_;
    uint32_t               id_;
    AvExecutor           *executor_;
    SubApi                 api_;
    DeviceUuid             uuid_;
    GlContextOptions       options_;
    bool                   configured_;

    friend struct ContextExecutorProbe;
};

}  // namespace av

#endif  // AV_CONTEXT_EXECUTOR_HPP
