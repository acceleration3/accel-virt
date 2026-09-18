/* The lock order, declared once. A thread may only acquire a lock whose rank is
 * strictly greater than every lock it already holds, so no two locks can ever
 * be taken in opposite orders. Each named rank is added here by the feature
 * that introduces its lock, in acquisition order. */
#ifndef AV_LOCK_RANK_HPP
#define AV_LOCK_RANK_HPP

#include <stdint.h>

namespace av {

enum class LockRank : uint16_t {
    doorbell_table = 70u,     /* DoorbellTable, taken before the queues it names (feature 049) */
    hw_queue_table = 80u,     /* HwQueueTable, which looks contexts up in ContextTable */
    context_table = 90u,      /* ContextTable, which looks devices up in DeviceTable */
    device_table = 100u,       /* DeviceTable, which calls into AllocationTable */
    rvk_control_requests = 105u, /* wait mutex: an rvk context's setup, teardown or chunk request sequence,
                                    which resolves its allocations in AllocationTable */
    allocation_table = 110u,
    rvk_context_slots = 190u,  /* spin lock: which rvk contexts are live, around a submission and the DPC's serve */
    execution_node = 200u,     /* spin lock: SubmitCommand, PreemptCommand and the DPC */
    hw_queue_contexts = 205u,  /* spin lock: what each hardware queue runs on, read at DISPATCH (feature 049) */
    hw_queue_submissions = 210u, /* spin lock: SubmitCommandToHwQueue, DestroyHwQueue, ResetHwEngine and the DPC */
    control_queue = 220u,      /* spin lock: virtio-gpu control requests and the DPC's harvest */
    cursor_queue = 230u,       /* spin lock: virtio-gpu cursor requests and the DPC's harvest */
    rvk_context = 240u,        /* spin lock: an rvk context's submission, answers and the DPC's serve */
    log_budget = 250u,         /* spin lock: a leaf, taken while the DPC already holds the locks above */
};

}  // namespace av

#endif  // AV_LOCK_RANK_HPP
