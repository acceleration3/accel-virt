#include "proto/resources/shared_object_kind.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

SharedObjectKind::SharedObjectKind() : value_(0u)
{
    AV_SCOPE("SharedObjectKind::SharedObjectKind");
}

SharedObjectKind::SharedObjectKind(uint32_t value) : value_(value)
{
    AV_SCOPE("SharedObjectKind::SharedObjectKind");
}

uint32_t SharedObjectKind::value() const
{
    AV_SCOPE("SharedObjectKind::value");
    return value_;
}

bool SharedObjectKind::known() const
{
    AV_SCOPE("SharedObjectKind::known");
    return value_ >= memory && value_ <= native_fence;
}

bool SharedObjectKind::is_memory() const
{
    AV_SCOPE("SharedObjectKind::is_memory");
    return value_ == memory;
}

bool SharedObjectKind::is_fence() const
{
    AV_SCOPE("SharedObjectKind::is_fence");
    return value_ == monitored_fence || value_ == native_fence;
}

bool SharedObjectKind::same_as(const SharedObjectKind &other) const
{
    AV_SCOPE("SharedObjectKind::same_as");
    return value_ == other.value_;
}

bool SharedObjectKind::sized(uint64_t size) const
{
    AV_SCOPE("SharedObjectKind::sized");
    if (is_fence()) return size == 0u;
    return size != 0u && size % page_size == 0u;
}

}  // namespace av
