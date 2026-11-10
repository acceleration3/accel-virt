#include "guest/transport/host_allocation.hpp"

#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/transport/kmt_transport.hpp"

namespace av {

HostAllocation::HostAllocation(GuestConnection &connection, KmtTransport &transport)
    : chunk_(connection, transport), import_(transport), named_(0u), placed_(false)
{
    AV_SCOPE("HostAllocation::HostAllocation");
}

HostAllocation::~HostAllocation()
{
    AV_SCOPE("HostAllocation::~HostAllocation");
    static_cast<void>(close());
}

Status HostAllocation::open(uint64_t bytes, HostMemoryRequest memory, const BlobRole &role)
{
    AV_SCOPE("HostAllocation::open");
    if (chunk_.resource_id() != 0u) return Status::invalid_state;
    const Status opened = chunk_.open(bytes, memory);
    if (opened != Status::ok) return opened;
    return import_taken(role);
}

Status HostAllocation::adopt(void *address, uint64_t bytes, HostMemoryRequest memory)
{
    AV_SCOPE("HostAllocation::adopt");
    if (chunk_.resource_id() != 0u) return Status::invalid_state;
    const Status lent = chunk_.adopt(address, bytes, memory);
    if (lent != Status::ok) return lent;
    return import_taken(BlobRole());
}

Status HostAllocation::place(uint64_t bytes, HostMemoryRequest memory, uint64_t named)
{
    AV_SCOPE("HostAllocation::place");
    if (chunk_.resource_id() != 0u) return Status::invalid_state;
    if (named == 0u || !memory.places()) return Status::invalid_parameter;
    const Status opened = chunk_.open(bytes, memory);
    if (opened != Status::ok) return opened;
    named_ = named;
    placed_ = true;
    return Status::ok;
}

Status HostAllocation::import_taken(const BlobRole &role)
{
    AV_SCOPE("HostAllocation::import_taken");
    ScopeExit    undo([this] { static_cast<void>(chunk_.release()); });
    const Status imported = import_.import(chunk_.resource_id(), role);
    if (imported != Status::ok) return imported;
    undo.dismiss();
    return Status::ok;
}

Status HostAllocation::close()
{
    AV_SCOPE("HostAllocation::close");
    if (chunk_.resource_id() == 0u) return Status::ok;
    if (placed_) {
        placed_ = false;
        named_ = 0u;
        return chunk_.release();
    }
    const Status released = import_.release();
    const Status unbound = chunk_.release();
    return released != Status::ok ? released : unbound;
}

Status HostAllocation::abandon()
{
    AV_SCOPE("HostAllocation::abandon");
    placed_ = false;
    named_ = 0u;
    import_.forget();
    return chunk_.release();
}

bool HostAllocation::held() const
{
    AV_SCOPE("HostAllocation::held");
    return chunk_.resource_id() != 0u;
}

bool HostAllocation::placed() const
{
    AV_SCOPE("HostAllocation::placed");
    return placed_;
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

uint8_t *HostAllocation::view_at(uint64_t offset, uint64_t bytes) const
{
    AV_SCOPE("HostAllocation::view_at");
    const uint64_t whole = this->bytes();
    if (bytes == 0u || offset > whole || bytes > whole - offset) return nullptr;
    return view() + offset;
}

uint64_t HostAllocation::handle() const
{
    AV_SCOPE("HostAllocation::handle");
    return placed_ ? named_ : import_.memory();
}

uint32_t HostAllocation::host_status() const
{
    AV_SCOPE("HostAllocation::host_status");
    return import_.host_status();
}

bool HostAllocation::invariants_hold() const
{
    AV_SCOPE("HostAllocation::invariants_hold");
    if (!chunk_.invariants_hold() || !import_.invariants_hold()) return false;
    if (placed_) return chunk_.resource_id() != 0u && named_ != 0u && import_.memory() == 0u;
    return chunk_.resource_id() != 0u ? import_.memory() != 0u : import_.memory() == 0u;
}

}
