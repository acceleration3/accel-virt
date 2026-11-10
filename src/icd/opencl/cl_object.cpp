#include "icd/opencl/cl_object.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClObject::ClObject(const cl_icd_dispatch &table, BlockSource &memory, Kind kind, uint64_t wire,
                   GuestClContext *context, ClObject *parent)
    : dispatch_(&table), context_(context), parent_(parent), wire_(wire), backing_offset_(0u), backing_bytes_(0u), application_memory_(nullptr), lent_(nullptr), sliced_from_(nullptr), shape_(nullptr), flags_(0u), extent_(), command_type_(0u), traits_(0u), references_(1u), children_(0u), kind_(kind), idle_(true), ordinals_(), ordinal_(0u), fixed_(memory)
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

void ClObject::use_application_memory(void *memory)
{
    AV_SCOPE("ClObject::use_application_memory");
    application_memory_ = static_cast<uint8_t *>(memory);
}

void ClObject::lend(HostAllocation &lent)
{
    AV_SCOPE("ClObject::lend");
    lent_ = &lent;
}

HostAllocation *ClObject::lent() const
{
    AV_SCOPE("ClObject::lent");
    return lent_;
}

uint8_t *ClObject::application_memory() const
{
    AV_SCOPE("ClObject::application_memory");
    return application_memory_;
}

void ClObject::slice_of(ClObject &buffer)
{
    AV_SCOPE("ClObject::slice_of");
    sliced_from_ = &buffer;
}

ClObject *ClObject::sliced_from() const
{
    AV_SCOPE("ClObject::sliced_from");
    return sliced_from_;
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

void ClObject::busy()
{
    AV_SCOPE("ClObject::busy");
    __atomic_store_n(&idle_, false, __ATOMIC_RELEASE);
}

void ClObject::idle()
{
    AV_SCOPE("ClObject::idle");
    __atomic_store_n(&idle_, true, __ATOMIC_RELEASE);
}

bool ClObject::known_idle() const
{
    AV_SCOPE("ClObject::known_idle");
    return __atomic_load_n(&idle_, __ATOMIC_ACQUIRE);
}

uint64_t ClObject::take_ordinal()
{
    AV_SCOPE("ClObject::take_ordinal");
    return ordinals_.next();
}

void ClObject::stamp(uint64_t ordinal)
{
    AV_SCOPE("ClObject::stamp");
    ordinal_ = ordinal;
}

uint64_t ClObject::ordinal() const
{
    AV_SCOPE("ClObject::ordinal");
    return ordinal_;
}

void ClObject::keep_shape(ClKernelShape *shape)
{
    AV_SCOPE("ClObject::keep_shape");
    shape_ = shape;
}

ClKernelShape *ClObject::shape() const
{
    AV_SCOPE("ClObject::shape");
    return shape_;
}

ClMemExtent &ClObject::extent()
{
    AV_SCOPE("ClObject::extent");
    return extent_;
}

const ClMemExtent &ClObject::extent() const
{
    AV_SCOPE("ClObject::extent");
    return extent_;
}

void ClObject::keep_flags(uint64_t flags)
{
    AV_SCOPE("ClObject::keep_flags");
    flags_ = flags;
}

uint64_t ClObject::flags() const
{
    AV_SCOPE("ClObject::flags");
    return flags_;
}

void ClObject::keep_command_type(uint32_t type)
{
    AV_SCOPE("ClObject::keep_command_type");
    command_type_ = type;
}

uint32_t ClObject::command_type() const
{
    AV_SCOPE("ClObject::command_type");
    return command_type_;
}

void ClObject::keep_traits(uint32_t traits)
{
    AV_SCOPE("ClObject::keep_traits");
    traits_ = traits;
}

uint32_t ClObject::traits() const
{
    AV_SCOPE("ClObject::traits");
    return traits_;
}

bool ClObject::kept_answer(cl_mem_info name, const ClInfoRequest &asked, cl_int &result) const
{
    AV_SCOPE("ClObject::kept_answer");
    return fixed_.answer(0u, name, asked, result);
}

void ClObject::keep_answer(cl_mem_info name, const void *value, size_t bytes)
{
    AV_SCOPE("ClObject::keep_answer");
    fixed_.keep(0u, name, value, bytes);
}

bool ClObject::invariants_hold() const
{
    AV_SCOPE("ClObject::invariants_hold");
    if (dispatch_ == nullptr) return false;
    if (wire_ == 0u) return false;
    if (parent_ != nullptr && static_cast<uint8_t>(parent_->kind_) >= static_cast<uint8_t>(kind_)) {
        return false;
    }
    if (kind_ == Kind::platform && context_ != nullptr) return false;
    if (sliced_from_ != nullptr && (kind_ != Kind::mem || sliced_from_ == this)) return false;
    return true;
}

}
