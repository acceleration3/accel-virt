#include "guest/transport/arena_segment.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/transport/arena_slots.hpp"
#include "proto/wire/guest_bytes.hpp"
#include "proto/resources/host_memory_request.hpp"
#include "proto/submission/blob_role.hpp"

namespace av {

ArenaSegment::ArenaSegment(GuestConnection &connection, KmtTransport &transport, ArenaSlots &slots)
    : slots_(slots), memory_(connection, transport), base_(0u), bytes_(0u)
{
    AV_SCOPE("ArenaSegment::ArenaSegment");
}

ArenaSegment::~ArenaSegment()
{
    AV_SCOPE("ArenaSegment::~ArenaSegment");
    static_cast<void>(close());
}

Status ArenaSegment::open(uint64_t bytes)
{
    AV_SCOPE("ArenaSegment::open");
    if (bytes_ != 0u) return Status::invalid_state;
    const uint64_t whole = (bytes + page_bytes - 1u) / page_bytes * page_bytes;
    uint64_t       base = 0u;
    if (bytes == 0u || whole < bytes || !slots_.take(whole, base)) return Status::no_space;
    return make(base, whole);
}

Status ArenaSegment::open_at(uint64_t base, uint64_t bytes)
{
    AV_SCOPE("ArenaSegment::open_at");
    if (bytes_ != 0u) return Status::invalid_state;
    const uint64_t whole = (bytes + page_bytes - 1u) / page_bytes * page_bytes;
    if (bytes == 0u || whole < bytes || !slots_.take_at(base, whole)) return Status::no_space;
    return make(base, whole);
}

Status ArenaSegment::make(uint64_t base, uint64_t bytes)
{
    AV_SCOPE("ArenaSegment::make");
    const Status opened = memory_.open(bytes, HostMemoryRequest(HostMemoryRequest::Kind::readback, 0u),
                                       BlobRole(BlobRole::Kind::arena_segment, base));
    if (opened != Status::ok) {
        slots_.give(base, bytes);
        return opened;
    }
    base_ = base;
    bytes_ = bytes;
    return Status::ok;
}

Status ArenaSegment::close()
{
    AV_SCOPE("ArenaSegment::close");
    if (bytes_ == 0u) return Status::ok;
    const Status closed = memory_.close();
    slots_.give(base_, bytes_);
    base_ = 0u;
    bytes_ = 0u;
    return closed;
}

Status ArenaSegment::abandon()
{
    AV_SCOPE("ArenaSegment::abandon");
    if (bytes_ == 0u) return Status::ok;
    const Status abandoned = memory_.abandon();
    slots_.give(base_, bytes_);
    base_ = 0u;
    bytes_ = 0u;
    return abandoned;
}

bool ArenaSegment::held() const
{
    AV_SCOPE("ArenaSegment::held");
    return bytes_ != 0u;
}

uint64_t ArenaSegment::base() const
{
    AV_SCOPE("ArenaSegment::base");
    return base_;
}

uint64_t ArenaSegment::bytes() const
{
    AV_SCOPE("ArenaSegment::bytes");
    return bytes_;
}

uint8_t *ArenaSegment::view() const
{
    AV_SCOPE("ArenaSegment::view");
    return bytes_ == 0u ? nullptr : memory_.view();
}

bool ArenaSegment::invariants_hold() const
{
    AV_SCOPE("ArenaSegment::invariants_hold");
    if (bytes_ == 0u) return base_ == 0u && !memory_.held();
    return memory_.held() && bytes_ % page_bytes == 0u && base_ % GuestBytes::slot_bytes == 0u &&
           memory_.bytes() == bytes_;
}

}
