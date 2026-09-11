#include "core/scheduling/hw_queue_table.hpp"

#include "core/ddi/dxgkarg_create_hw_queue_record.hpp"
#include "core/platform/mutex_guard.hpp"
#include "core/scheduling/context.hpp"
#include "core/scheduling/context_table.hpp"
#include "core/support/invariant_scope.hpp"

namespace av {

HwQueueTable::HwQueueTable(Platform &platform, ContextTable &contexts)
    : platform_(platform), contexts_(contexts), mutex_(platform, LockRank::hw_queue_table), queues_(platform) {}

Status HwQueueTable::create(void *context, DxgkargCreateHwQueueRecord &request)
{
    platform_.require_irql_at_most(Irql::passive, "HwQueueTable::create");
    MutexGuard lock(mutex_);
    InvariantScope<HwQueueTable> guard(platform_, *this, "create");
    Context *owner = contexts_.find(context);
    if (owner == nullptr || (owner->flags() & context_hw_queue_supported) == 0u ||
        (request.Flags & ~known_flags) != 0u || request.PrivateDriverDataSize != 0u ||
        request.HwQueueProgressFenceCPUVirtualAddress == nullptr)
        return Status::invalid_parameter;
    HwQueue *queue = nullptr;
    const Status status = queues_.create(queue, platform_, *owner);
    if (status == Status::ok) request.hHwQueue = queue;
    return status;
}

Status HwQueueTable::destroy(void *queue)
{
    platform_.require_irql_at_most(Irql::passive, "HwQueueTable::destroy");
    MutexGuard lock(mutex_);
    InvariantScope<HwQueueTable> guard(platform_, *this, "destroy");
    return queues_.destroy(queues_.find(queue));
}

bool HwQueueTable::serves(void *context)
{
    platform_.require_irql_at_most(Irql::passive, "HwQueueTable::serves");
    MutexGuard lock(mutex_);
    return !queues_.all_of([context](const HwQueue &queue) { return &queue.context() != context; });
}

uint32_t HwQueueTable::live() const { return queues_.live(); }

bool HwQueueTable::invariants_hold() const { return queues_.invariants_hold(); }

}  // namespace av
