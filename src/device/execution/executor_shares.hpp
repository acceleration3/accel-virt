#ifndef AV_EXECUTOR_SHARES_HPP
#define AV_EXECUTOR_SHARES_HPP

#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "proto/context/device_uuid.hpp"
#include "proto/context/sub_api.hpp"

#include <stdint.h>

namespace av {

class ExecutorLibrary;
class Host;
struct ExecutorSharesProbe;

class ExecutorShares {
public:
    static constexpr uint32_t capacity = 64u;
    static constexpr uint32_t uuid_bytes = DeviceUuid::bytes;

    ExecutorShares(Host &host, const ExecutorLibrary &library);
    ~ExecutorShares();
    ExecutorShares(const ExecutorShares &) = delete;
    ExecutorShares &operator=(const ExecutorShares &) = delete;

    [[nodiscard]] Status initialize();
    [[nodiscard]] Status publish(uint32_t id, const uint8_t *uuid, AvExecutor *executor);
    void                 withdraw(uint32_t id);
    AvExecutor         *create_sharing(SubApi api, uint32_t share_id, const uint8_t *uuid,
                                        const AvContextOptions &options);
    uint32_t             published();

    bool invariants_hold() const;

private:
    static constexpr uint32_t none = capacity;

    uint32_t index_of(uint32_t id) const;
    bool     on_gpu(uint32_t index, const uint8_t *uuid) const;

    Host                  &host_;
    const ExecutorLibrary &library_;
    void                  *lock_;
    uint32_t               ids_[capacity];
    DeviceUuid             uuids_[capacity];
    AvExecutor           *executors_[capacity];

    friend struct ExecutorSharesProbe;
};

}

#endif
