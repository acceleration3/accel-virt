#ifndef AV_HW_QUEUE_TABLE_HPP
#define AV_HW_QUEUE_TABLE_HPP

#include "core/platform/mutex.hpp"
#include "core/platform/platform.hpp"
#include "miniport/scheduling/hw_queue.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class StateWriter;

class ContextTable;
struct DxgkargCreateHwQueueRecord;
struct HwQueueTableProbe;

class HwQueueTable {
public:
    HwQueueTable(Platform &platform, ContextTable &contexts);
    HwQueueTable(const HwQueueTable &) = delete;
    HwQueueTable &operator=(const HwQueueTable &) = delete;

    [[nodiscard]] Status create(void *context, DxgkargCreateHwQueueRecord &request);
    [[nodiscard]] Status destroy(void *queue);
    bool                 serves(void *context);
    HwQueue             *find(void *handle);
    uint32_t             live() const;

    void report(StateWriter &writer);

    bool invariants_hold() const;

private:
    static constexpr uint32_t known_flags = 0x7u;
    static constexpr uint32_t context_hw_queue_supported = 0x10u;

    Platform            &platform_;
    ContextTable        &contexts_;
    Mutex                mutex_;
    ObjectPool<HwQueue>  queues_;

    friend struct HwQueueTableProbe;
};

}

#endif
