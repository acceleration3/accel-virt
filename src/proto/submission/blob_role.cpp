#include "proto/submission/blob_role.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/guest_bytes.hpp"

namespace av {

BlobRole::BlobRole() : kind_(Kind::plain), base_(0u)
{
    AV_SCOPE("BlobRole::BlobRole");
}

BlobRole::BlobRole(Kind kind, uint64_t base) : kind_(kind), base_(base)
{
    AV_SCOPE("BlobRole::BlobRole");
}

BlobRole::Kind BlobRole::kind() const
{
    AV_SCOPE("BlobRole::kind");
    return kind_;
}

uint64_t BlobRole::base() const
{
    AV_SCOPE("BlobRole::base");
    return base_;
}

bool BlobRole::is_arena() const
{
    AV_SCOPE("BlobRole::is_arena");
    return kind_ == Kind::arena_segment;
}

bool BlobRole::invariants_hold() const
{
    AV_SCOPE("BlobRole::invariants_hold");
    if (kind_ == Kind::plain) return base_ == 0u;
    return kind_ == Kind::arena_segment && base_ % GuestBytes::slot_bytes == 0u &&
           base_ >> GuestBytes::slot_shift < GuestBytes::slot_count;
}

Status BlobRole::decode(uint32_t kind, uint64_t base)
{
    AV_SCOPE("BlobRole::decode");
    const BlobRole read(static_cast<Kind>(kind), base);
    if (kind > static_cast<uint32_t>(Kind::arena_segment) || !read.invariants_hold()) return Status::invalid_parameter;
    *this = read;
    return Status::ok;
}

bool BlobRole::operator==(const BlobRole &other) const
{
    AV_SCOPE("BlobRole::operator==");
    return kind_ == other.kind_ && base_ == other.base_;
}

}
