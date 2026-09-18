#include "icd/opencl/cl_object.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClObject::ClObject(const cl_icd_dispatch &table, Kind kind, uint64_t wire, GuestClContext *context,
                   ClObject *parent)
    : dispatch_(&table), context_(context), parent_(parent), wire_(wire), references_(1u), kind_(kind)
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

bool ClObject::descends_from(const ClObject &other) const
{
    AV_SCOPE("ClObject::descends_from");
    for (const ClObject *walk = this; walk != nullptr; walk = walk->parent_) {
        if (walk == &other) return true;
    }
    return false;
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
