#include "core/scheduling/context_table.hpp"

#include "core/ddi/dxgkarg_create_context_record.hpp"
#include "core/platform/mutex_guard.hpp"
#include "core/scheduling/context.hpp"
#include "core/scheduling/device.hpp"
#include "core/scheduling/device_table.hpp"
#include "core/platform/mutex_try_guard.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "proto/context/rvk_context_record.hpp"
#include "core/support/invariant_scope.hpp"

namespace av {

ContextTable::ContextTable(Platform &platform, DeviceTable &devices, const GpuMmuGeometry &geometry)
    : platform_(platform), devices_(devices), geometry_(geometry), mutex_(platform, LockRank::context_table), contexts_(platform)
    {
        AV_SCOPE("ContextTable::ContextTable");
    }

Status ContextTable::create(void *device, DxgkargCreateContextRecord &request)
{
    AV_SCOPE("ContextTable::create");
    platform_.require_irql_at_most(Irql::passive, "ContextTable::create");
    MutexGuard lock(mutex_);
    InvariantScope<ContextTable> guard(platform_, *this, "create");
    request.hContext = nullptr;
    Device *owner = devices_.find(device);
    /* Private data is either absent, or exactly the rvk context record - any
     * other size is data this driver does not define. */
    const bool rvk = request.PrivateDriverDataSize == RvkContextRecord::encoded_size;
    /* Node 1 is the hardware-scheduled one: dxgkrnl puts a context there only
     * when the process asked for one a queue may run on, and a context that
     * did not ask has no way to submit to it (feature 049, research.md §16). */
    if (owner == nullptr || request.NodeOrdinal >= node_count ||
        (request.NodeOrdinal == hardware_queue_node && (request.Flags & hw_queue_supported) == 0u) ||
        (request.EngineAffinity & ~known_engines) != 0u ||
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
    AV_SCOPE("ContextTable::destroy");
    platform_.require_irql_at_most(Irql::passive, "ContextTable::destroy");
    MutexGuard lock(mutex_);
    InvariantScope<ContextTable> guard(platform_, *this, "destroy");
    return contexts_.destroy(contexts_.find(context));
}

Context *ContextTable::find(void *context)
{
    AV_SCOPE("ContextTable::find");
    platform_.require_irql_at_most(Irql::apc, "ContextTable::find");
    MutexGuard lock(mutex_);
    return contexts_.find(context);
}

Context *ContextTable::followed(const Context &follower, uint32_t ring)
{
    AV_SCOPE("ContextTable::followed");
    platform_.require_irql_at_most(Irql::apc, "ContextTable::followed");
    MutexGuard  lock(mutex_);
    void *const process = follower.device().process();
    return contexts_.find_if([&](const Context &context) {
        return context.is_rvk() && context.rvk_record().ring() == ring && context.device().process() == process;
    });
}

uint32_t ContextTable::live() const
{
    AV_SCOPE("ContextTable::live");
    return contexts_.live();
}

void ContextTable::report(StateWriter &writer)
{
    AV_SCOPE("ContextTable::report");
    platform_.require_irql_at_most(Irql::apc, "ContextTable::report");
    const MutexTryGuard guard(mutex_);
    writer.open("context_table", 0u);
    writer.flag("busy", !guard.held());
    if (guard.held()) {
        writer.number("live", contexts_.live());
        uint64_t index = 0u;
        contexts_.for_each([&](const Context &context) {
            writer.open("context", index++);
            writer.address("handle", &context);
            writer.address("device", &context.device());
            writer.number("node", context.node());
            writer.hex("flags", context.flags());
            writer.flag("rvk", context.is_rvk());
            writer.close();
        });
    }
    writer.close();
}

bool ContextTable::invariants_hold() const
{
    AV_SCOPE("ContextTable::invariants_hold");
    return contexts_.invariants_hold();
}

}  // namespace av
