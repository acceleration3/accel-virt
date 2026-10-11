#include "miniport/memory/shared_object.hpp"

#include "miniport/memory/allocation.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

SharedObject::SharedObject(Platform &platform, uint64_t id, const SharedObjectKind &kind, uint64_t size,
                           Allocation &allocation)
    : platform_(platform), id_(id), kind_(kind), size_(size), allocation_(&allocation), opens_(0u)
    {
        AV_SCOPE("SharedObject::SharedObject");
    }

uint64_t SharedObject::id() const
{
    AV_SCOPE("SharedObject::id");
    return id_;
}

const SharedObjectKind &SharedObject::kind() const
{
    AV_SCOPE("SharedObject::kind");
    return kind_;
}

uint64_t SharedObject::size() const
{
    AV_SCOPE("SharedObject::size");
    return size_;
}

bool SharedObject::names(const Allocation &allocation) const
{
    AV_SCOPE("SharedObject::names");
    return allocation_ == &allocation;
}

void SharedObject::forget()
{
    AV_SCOPE("SharedObject::forget");
    allocation_ = nullptr;
}

bool SharedObject::forgotten() const
{
    AV_SCOPE("SharedObject::forgotten");
    return allocation_ == nullptr;
}

uint32_t SharedObject::opens() const
{
    AV_SCOPE("SharedObject::opens");
    return opens_;
}

Status SharedObject::open()
{
    AV_SCOPE("SharedObject::open");
    platform_.require_irql_at_most(Irql::apc, "SharedObject::open");
    if (opens_ == max_opens) return Status::no_space;
    opens_++;
    return Status::ok;
}

Status SharedObject::close()
{
    AV_SCOPE("SharedObject::close");
    platform_.require_irql_at_most(Irql::apc, "SharedObject::close");
    if (opens_ == 0u) return Status::invalid_parameter;
    opens_--;
    return Status::ok;
}

bool SharedObject::invariants_hold() const
{
    AV_SCOPE("SharedObject::invariants_hold");
    if (id_ == 0u || !kind_.known()) return false;
    /* A forgotten object is held by nobody: dxgkrnl destroys a shared
     * allocation only once every device let go of it. */
    if (allocation_ == nullptr && opens_ != 0u) return false;
    /* Each kind's own size, which is the record's rule and the wire body's: a
     * fence has none at all (feature 053, T031). */
    if (kind_.is_fence()) return size_ == 0u;
    return size_ != 0u && size_ % Allocation::page_bytes == 0u;
}

}  // namespace av
