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
    /* Whether the library can make a context with `options`: the defaults
     * always, anything else only from version 4. */
    bool                 accepts(const GlContextOptions &options) const;
    /* Main loop, before create: what the library is to call when a host API
     * finishes work on a thread of its own. A library older than version 5 is
     * given nothing, and a guest on one waits at a synchronising call
     * instead. */
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
    /* Worker. `handle` imported under `blob_id`, with `device_memory` the wire
     * handle the import became: invalid_state without an executor;
     * not_supported, leaving nothing imported, for a library that cannot name
     * an import, refuses this one or names it nothing. */
    [[nodiscard]] Status import(uint64_t blob_id, const AvBlobHandle &handle, uint64_t &device_memory);
    /* Worker. Nothing without an executor: destroying it released every import. */
    void                 release(uint64_t blob_id);

    /* Worker. A descriptor for the object `wire_handle` names in this
     * context's commands, for the device to publish (feature 053).
     * invalid_state without an executor; not_supported for a library older
     * than the ABI version that shares objects, a handle it did not make, or a
     * kind it cannot export - only Vulkan exports anything. The descriptor is
     * the caller's on ok and nothing is held here. */
    [[nodiscard]] Status export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind, int &fd);
    /* Worker. `fd` made into an object of this context's, with `wire_handle`
     * what the guest names it by. The descriptor stays the caller's whatever
     * the answer: it is the device's, and an importer that keeps one
     * duplicates it. Same refusals as export_object. */
    [[nodiscard]] Status import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                       uint64_t &wire_handle);
    /* Worker. Nothing without an executor: destroying it released every
     * object it made. */
    void                 release_object(uint32_t kind, uint64_t wire_handle);
    /* The API half (id >> 16) of the commands this context runs. */
    uint16_t             command_api() const;
    bool                 created() const;

    bool invariants_hold() const;

private:
    /* render-server's API ids (av/wire/api_id.hpp), not the capset's bits. */
    static constexpr uint16_t vulkan_commands = 1u;
    static constexpr uint16_t opencl_commands = 2u;
    static constexpr uint16_t opengl_commands = 3u;
    static constexpr uint16_t d3d_commands = 4u;

    AvExecutor *make() const;

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
