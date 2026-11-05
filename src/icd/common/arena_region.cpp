#include "icd/common/arena_region.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/client/client_transport.hpp"

namespace av::icd {

ArenaRegion::ArenaRegion() : wire_(nullptr), lease_()
{
    AV_SCOPE("ArenaRegion::ArenaRegion");
}

ArenaRegion::~ArenaRegion()
{
    AV_SCOPE("ArenaRegion::~ArenaRegion");
    close();
}

bool ArenaRegion::open(ClientTransport &wire, uint64_t bytes)
{
    AV_SCOPE("ArenaRegion::open");
    if (lease_.taken() || bytes == 0u) return false;
    const ClientBlobLease made = wire.open_region(bytes);
    if (!made.taken()) return false;
    wire_ = &wire;
    lease_ = made;
    return true;
}

void ArenaRegion::close()
{
    AV_SCOPE("ArenaRegion::close");
    if (wire_ != nullptr) wire_->close_region(lease_);
    wire_ = nullptr;
    lease_ = ClientBlobLease();
}

bool ArenaRegion::held() const
{
    AV_SCOPE("ArenaRegion::held");
    return lease_.taken();
}

uint8_t *ArenaRegion::view() const
{
    AV_SCOPE("ArenaRegion::view");
    return static_cast<uint8_t *>(lease_.data());
}

uint64_t ArenaRegion::offset() const
{
    AV_SCOPE("ArenaRegion::offset");
    return lease_.offset();
}

uint64_t ArenaRegion::bytes() const
{
    AV_SCOPE("ArenaRegion::bytes");
    return lease_.bytes();
}

bool ArenaRegion::invariants_hold() const
{
    AV_SCOPE("ArenaRegion::invariants_hold");
    return lease_.taken() == (wire_ != nullptr);
}

}
