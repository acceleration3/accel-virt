#include "icd/opencl/cl_object.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClObject::ClObject(const cl_icd_dispatch &table, Kind kind, uint64_t wire, GuestClContext *context,
                   ClObject *parent)
    : dispatch_(&table), context_(context), parent_(parent), wire_(wire), backing_offset_(0u), backing_bytes_(0u), references_(1u), children_(0u), kind_(kind)
{
    AV_SCOPE("ClObject::ClObject");
}

ClObject::Kind ClObject::kind() const
{
    AV_SCOPE("ClObject::kind");
    return kind_;
}

uint64_t ClObject::wire() const
{
    AV_SCOPE("ClObject::wire");
    return wire_;
}

GuestClContext *ClObject::context() const
{
    AV_SCOPE("ClObject::context");
    return context_;
}

ClObject *ClObject::parent() const
{
    AV_SCOPE("ClObject::parent");
    return parent_;
}

void ClObject::back_with(uint64_t offset, uint64_t bytes)
{
    AV_SCOPE("ClObject::back_with");
    backing_offset_ = offset;
    backing_bytes_ = bytes;
}

bool ClObject::backed() const
{
    AV_SCOPE("ClObject::backed");
    return backing_bytes_ != 0u;
}

uint64_t ClObject::backing_offset() const
{
    AV_SCOPE("ClObject::backing_offset");
    return backing_offset_;
}

uint64_t ClObject::backing_bytes() const
{
    AV_SCOPE("ClObject::backing_bytes");
    return backing_bytes_;
}

bool ClObject::descends_from(const ClObject &other) const
{
    AV_SCOPE("ClObject::descends_from");
    for (const ClObject *walk = this; walk != nullptr; walk = walk->parent_) {
        if (walk == &other) return true;
    }
    return false;
}

ClObject *ClObject::nearest(Kind kind)
{
    AV_SCOPE("ClObject::nearest");
    for (ClObject *walk = this; walk != nullptr; walk = walk->parent_) {
        if (walk->kind_ == kind) return walk;
    }
    return nullptr;
}

void ClObject::retain()
{
    AV_SCOPE("ClObject::retain");
    /* A count that saturates no longer says how many, so it never drops to
     * zero either: the object lives until its context does. An application
     * that retains four billion times has other problems. */
    if (references_ != ~uint32_t{0}) ++references_;
}

bool ClObject::release()
{
    AV_SCOPE("ClObject::release");
    if (references_ == ~uint32_t{0}) return false;
    if (references_ == 0u) return false;
    --references_;
    return references_ == 0u;
}

uint32_t ClObject::references() const
{
    AV_SCOPE("ClObject::references");
    return references_;
}

void ClObject::adopt()
{
    AV_SCOPE("ClObject::adopt");
    ++children_;
}

void ClObject::orphan()
{
    AV_SCOPE("ClObject::orphan");
    if (children_ != 0u) --children_;
}

bool ClObject::unheld() const
{
    AV_SCOPE("ClObject::unheld");
    return references_ == 0u && children_ == 0u;
}

bool ClObject::invariants_hold() const
{
    AV_SCOPE("ClObject::invariants_hold");
    if (dispatch_ == nullptr) return false;
    if (wire_ == 0u) return false;
    /* A parent is always of a lower kind, which is what makes the chain
     * finite. */
    if (parent_ != nullptr && static_cast<uint8_t>(parent_->kind_) >= static_cast<uint8_t>(kind_)) {
        return false;
    }
    /* A platform belongs to no context; everything else does once it is made
     * on one. */
    if (kind_ == Kind::platform && context_ != nullptr) return false;
    return true;
}

}  // namespace av::clicd
