#include "guest/transport/host_allocation.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_transport.hpp"

namespace av {

HostAllocation::HostAllocation(GuestConnection &connection, KmtTransport &transport)
    : chunk_(connection, transport), import_(transport)
{
    AV_SCOPE("HostAllocation::HostAllocation");
}

HostAllocation::~HostAllocation()
{
    AV_SCOPE("HostAllocation::~HostAllocation");
    static_cast<void>(close());
}

Status HostAllocation::open(uint64_t bytes, HostMemoryRequest memory)
{
    AV_SCOPE("HostAllocation::open");
    if (chunk_.resource_id() != 0u) return Status::invalid_state;
    const Status opened = chunk_.open(bytes, memory);
    if (opened != Status::ok) return opened;
    ScopeExit    undo([this] { static_cast<void>(chunk_.release()); });
    const Status imported = import_.import(chunk_.resource_id());
    if (imported != Status::ok) return imported;
    undo.dismiss();
    return Status::ok;
}

Status HostAllocation::close()
{
    AV_SCOPE("HostAllocation::close");
    if (chunk_.resource_id() == 0u) return Status::ok;
    const Status released = import_.release();
    const Status unbound = chunk_.release();
    return released != Status::ok ? released : unbound;
}

Status HostAllocation::abandon()
{
    AV_SCOPE("HostAllocation::abandon");
    import_.forget();
    return chunk_.release();
}

bool HostAllocation::held() const
{
    AV_SCOPE("HostAllocation::held");
    return chunk_.resource_id() != 0u;
}

uint64_t HostAllocation::bytes() const
{
    AV_SCOPE("HostAllocation::bytes");
    return chunk_.bytes();
}

uint8_t *HostAllocation::view() const
{
    AV_SCOPE("HostAllocation::view");
    return const_cast<uint8_t *>(chunk_.view());
}

uint64_t HostAllocation::handle() const
{
    AV_SCOPE("HostAllocation::handle");
    return import_.memory();
}

uint32_t HostAllocation::host_status() const
{
    AV_SCOPE("HostAllocation::host_status");
    return import_.host_status();
}

/* An allocation held is a chunk imported, and none held imports nothing. */
bool HostAllocation::invariants_hold() const
{
    AV_SCOPE("HostAllocation::invariants_hold");
    if (!chunk_.invariants_hold() || !import_.invariants_hold()) return false;
    return chunk_.resource_id() != 0u ? import_.memory() != 0u : import_.memory() == 0u;
}

}  // namespace av
