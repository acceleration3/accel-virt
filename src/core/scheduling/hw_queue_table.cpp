#include "core/scheduling/hw_queue_table.hpp"

#include "core/ddi/dxgkarg_create_hw_queue_record.hpp"
#include "core/platform/mutex_guard.hpp"
#include "core/scheduling/context.hpp"
#include "core/scheduling/context_table.hpp"
#include "core/support/invariant_scope.hpp"
#include "core/platform/mutex_try_guard.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av {

HwQueueTable::HwQueueTable(Platform &platform, ContextTable &contexts)
    : platform_(platform), contexts_(contexts), mutex_(platform, LockRank::hw_queue_table), queues_(platform)
    {
        AV_SCOPE("HwQueueTable::HwQueueTable");
    }

Status HwQueueTable::create(void *context, DxgkargCreateHwQueueRecord &request)
{
    AV_SCOPE("HwQueueTable::create");
    platform_.require_irql_at_most(Irql::passive, "HwQueueTable::create");
    MutexGuard lock(mutex_);
    InvariantScope<HwQueueTable> guard(platform_, *this, "create");
    Context *owner = contexts_.find(context);
    if (owner == nullptr || (owner->flags() & context_hw_queue_supported) == 0u ||
        (request.Flags & ~known_flags) != 0u || request.PrivateDriverDataSize != 0u ||
        request.HwQueueProgressFenceCPUVirtualAddress == nullptr)
        return Status::invalid_parameter;
    HwQueue *queue = nullptr;
    const Status status = queues_.create(queue, platform_, *owner,
                                         static_cast<uint64_t *>(request.HwQueueProgressFenceCPUVirtualAddress));
    if (status == Status::ok) request.hHwQueue = queue;
    return status;
}

Status HwQueueTable::destroy(void *queue)
{
    AV_SCOPE("HwQueueTable::destroy");
    platform_.require_irql_at_most(Irql::passive, "HwQueueTable::destroy");
    MutexGuard lock(mutex_);
    InvariantScope<HwQueueTable> guard(platform_, *this, "destroy");
    return queues_.destroy(queues_.find(queue));
}

bool HwQueueTable::serves(void *context)
{
    AV_SCOPE("HwQueueTable::serves");
    platform_.require_irql_at_most(Irql::passive, "HwQueueTable::serves");
    MutexGuard lock(mutex_);
    return !queues_.all_of([context](const HwQueue &queue) { return &queue.context() != context; });
}

HwQueue *HwQueueTable::find(void *handle)
{
    AV_SCOPE("HwQueueTable::find");
    platform_.require_irql_at_most(Irql::passive, "HwQueueTable::find");
    MutexGuard lock(mutex_);
    return handle != nullptr ? queues_.find(handle) : nullptr;
}

uint32_t HwQueueTable::live() const
{
    AV_SCOPE("HwQueueTable::live");
    return queues_.live();
}

void HwQueueTable::report(StateWriter &writer)
{
    AV_SCOPE("HwQueueTable::report");
    platform_.require_irql_at_most(Irql::apc, "HwQueueTable::report");
    const MutexTryGuard guard(mutex_);
    writer.open("hw_queue_table", 0u);
    writer.flag("busy", !guard.held());
    if (guard.held()) {
        writer.number("live", queues_.live());
        uint64_t index = 0u;
        queues_.for_each([&](const HwQueue &queue) {
            writer.open("hw_queue", index++);
            writer.address("handle", &queue);
            writer.address("context", &queue.context());
            writer.close();
        });
    }
    writer.close();
}

bool HwQueueTable::invariants_hold() const
{
    AV_SCOPE("HwQueueTable::invariants_hold");
    return queues_.invariants_hold();
}

}  // namespace av
