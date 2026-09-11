#include "context_table.hpp"

#include "ddi/dxgkarg_create_context_record.hpp"
#include "device_table.hpp"
#include "invariant_scope.hpp"
#include "mutex_guard.hpp"

namespace av {

ContextTable::ContextTable(Platform &platform, DeviceTable &devices)
    : platform_(platform), devices_(devices), mutex_(platform, LockRank::context_table), contexts_(platform) {}

Status ContextTable::create(void *device, DxgkargCreateContextRecord &request)
{
    platform_.require_irql_at_most(Irql::passive, "ContextTable::create");
    MutexGuard lock(mutex_);
    InvariantScope<ContextTable> guard(platform_, *this, "create");
    request.hContext = nullptr;
    Device *owner = devices_.find(device);
    if (owner == nullptr || request.NodeOrdinal != 0u || (request.EngineAffinity & ~known_engines) != 0u ||
        (request.Flags & ~known_flags) != 0u || request.PrivateDriverDataSize != 0u)
        return Status::invalid_parameter;
    Context *context = nullptr;
    const Status status = contexts_.create(context, platform_, *owner, request.NodeOrdinal, request.Flags);
    if (status == Status::ok) context->report_to(request);
    return status;
}

Status ContextTable::destroy(void *context)
{
    platform_.require_irql_at_most(Irql::passive, "ContextTable::destroy");
    MutexGuard lock(mutex_);
    InvariantScope<ContextTable> guard(platform_, *this, "destroy");
    return contexts_.destroy(contexts_.find(context));
}

Context *ContextTable::find(void *context)
{
    platform_.require_irql_at_most(Irql::apc, "ContextTable::find");
    MutexGuard lock(mutex_);
    return contexts_.find(context);
}

uint32_t ContextTable::live() const { return contexts_.live(); }

bool ContextTable::invariants_hold() const { return contexts_.invariants_hold(); }

}  // namespace av
