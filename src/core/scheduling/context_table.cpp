#include "core/scheduling/context_table.hpp"

#include "core/ddi/dxgkarg_create_context_record.hpp"
#include "core/platform/mutex_guard.hpp"
#include "core/scheduling/device_table.hpp"
#include "proto/context/rvk_context_record.hpp"
#include "core/support/invariant_scope.hpp"

namespace av {

ContextTable::ContextTable(Platform &platform, DeviceTable &devices, const GpuMmuGeometry &geometry)
    : platform_(platform), devices_(devices), geometry_(geometry), mutex_(platform, LockRank::context_table), contexts_(platform) {}

Status ContextTable::create(void *device, DxgkargCreateContextRecord &request)
{
    platform_.require_irql_at_most(Irql::passive, "ContextTable::create");
    MutexGuard lock(mutex_);
    InvariantScope<ContextTable> guard(platform_, *this, "create");
    request.hContext = nullptr;
    Device *owner = devices_.find(device);
    /* Private data is either absent, or exactly the rvk context record - any
     * other size is data this driver does not define. */
    const bool rvk = request.PrivateDriverDataSize == RvkContextRecord::encoded_size;
    if (owner == nullptr || request.NodeOrdinal != 0u || (request.EngineAffinity & ~known_engines) != 0u ||
        (request.Flags & ~known_flags) != 0u ||
        (request.PrivateDriverDataSize != 0u && !rvk) ||
        (request.Flags & (hw_queue_supported | virtual_addressing)) == hw_queue_supported)
        return Status::invalid_parameter;
    RvkContextRecord record;
    if (rvk) {
        const Status decoded = record.decode(
            static_cast<const uint8_t *>(request.pPrivateDriverData), request.PrivateDriverDataSize);
        if (decoded != Status::ok) return Status::invalid_parameter;
    }
    Context *context = nullptr;
    const Status status = contexts_.create(context, platform_, *owner, geometry_, request.NodeOrdinal, request.Flags);
    if (status != Status::ok) return status;
    if (rvk) context->become_rvk(record);
    context->report_to(request);
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
