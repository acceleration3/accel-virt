/* The lock order, declared once. A thread may only acquire a lock whose rank is
 * strictly greater than every lock it already holds, so no two locks can ever
 * be taken in opposite orders. Each named rank is added here by the feature
 * that introduces its lock, in acquisition order. */
#ifndef AV_LOCK_RANK_HPP
#define AV_LOCK_RANK_HPP

#include <stdint.h>

namespace av {

enum class LockRank : uint16_t {
    context_table = 90u,      /* ContextTable, which looks devices up in DeviceTable */
    device_table = 100u,       /* DeviceTable, which calls into AllocationTable */
    allocation_table = 110u,
    execution_node = 200u,     /* spin lock: SubmitCommand, PreemptCommand and the DPC */
};

}  // namespace av

#endif  // AV_LOCK_RANK_HPP
