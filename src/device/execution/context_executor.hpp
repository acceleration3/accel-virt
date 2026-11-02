#ifndef AV_CONTEXT_EXECUTOR_HPP
#define AV_CONTEXT_EXECUTOR_HPP

#include "core/support/status.hpp"
#include "device/execution/host_poll.hpp"
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

    ContextExecutor(Host &host, const ExecutorLibrary &library, ExecutorShares &shares, uint32_t id);
    ~ContextExecutor();
    ContextExecutor(const ContextExecutor &) = delete;
    ContextExecutor &operator=(const ContextExecutor &) = delete;

    void                 configure(SubApi api, const uint8_t *gpu_uuid, const GlContextOptions &options);
    void                 signals_through(ContextSignal &signal);
    [[nodiscard]] Status create();
    void                 destroy();
    void                 end_batch();
    HostPoll             poll();
    void                 gpu_progress(uint64_t &marked, uint64_t &reached) const;
    [[nodiscard]] Status execute(uint32_t command_id, const uint8_t *payload, size_t size, uint8_t *reply,
                                 size_t reply_capacity, size_t &reply_size);
    [[nodiscard]] Status import(uint64_t blob_id, uint64_t memory_key, uint64_t &device_memory);
    void                 release(uint64_t blob_id);
    size_t               answer_part(uint64_t offset, uint8_t *out, size_t capacity, uint64_t &length);

    [[nodiscard]] Status share_object(uint64_t object_id, uint32_t kind, uint64_t size, uint64_t wire_handle);
    [[nodiscard]] Status open_object(uint64_t object_id, uint32_t kind, uint64_t size, uint64_t into,
                                     uint64_t &wire_handle);
    [[nodiscard]] Status lose_device();
    void                 close_object(uint64_t object_id, uint64_t wire_handle);
    uint16_t             command_api() const;
    bool                 created() const;

    bool invariants_hold() const;

private:
    AvExecutor *make() const;
    Status      status_of(uint32_t answered) const;

    Host                  &host_;
    const ExecutorLibrary &library_;
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

}

#endif
