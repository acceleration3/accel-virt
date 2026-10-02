/* The adapter's hardware queues. A queue runs on a context created for
 * hardware queues, keeps its progress fence reachable from the kernel, carries
 * no private data, and outlives no context. */
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

    /* Answers the queue's handle in hHwQueue. */
    [[nodiscard]] Status create(void *context, DxgkargCreateHwQueueRecord &request);
    [[nodiscard]] Status destroy(void *queue);
    /* Whether any queue still runs on `context`. */
    bool                 serves(void *context);
    /* The queue at `handle`, or null for one this table did not make: what a
     * doorbell is created on (feature 049). */
    HwQueue             *find(void *handle);
    uint32_t             live() const;

    /* A line for the table and one per queue; `busy=true` and nothing more
     * while another thread holds the table. */
    void report(StateWriter &writer);

    bool invariants_hold() const;

private:
    /* D3DDDI_CREATEHWQUEUEFLAGS: DisableGpuTimeout | NoBroadcastSignal | NoBroadcastWait.
     * Not NoKmdAccess: the driver writes the progress fence. */
    static constexpr uint32_t known_flags = 0x7u;
    static constexpr uint32_t context_hw_queue_supported = 0x10u;   /* DXGK_CREATECONTEXTFLAGS */

    Platform            &platform_;
    ContextTable        &contexts_;
    Mutex                mutex_;
    ObjectPool<HwQueue>  queues_;

    friend struct HwQueueTableProbe;
};

}  // namespace av

#endif  // AV_HW_QUEUE_TABLE_HPP
