#include "icd/vulkan/instance_span.hpp"

#include "core/trace/function_scope.hpp"

namespace av::icd {

InstanceSpan::InstanceSpan(VkInstance own) : instances_(), count_(1u)
{
    AV_SCOPE("InstanceSpan::InstanceSpan");
    instances_[0] = own;
}

Status InstanceSpan::add(VkInstance sibling)
{
    AV_SCOPE("InstanceSpan::add");
    if (count_ == max_instances) return Status::no_space;
    instances_[count_++] = sibling;
    return Status::ok;
}

bool InstanceSpan::spans_from(VkInstance instance) const
{
    AV_SCOPE("InstanceSpan::spans_from");
    return instance != VK_NULL_HANDLE && instances_[0] == instance;
}

uint32_t InstanceSpan::count() const
{
    AV_SCOPE("InstanceSpan::count");
    return count_;
}

VkInstance InstanceSpan::at(uint32_t index) const
{
    AV_SCOPE("InstanceSpan::at");
    return index < count_ ? instances_[index] : VK_NULL_HANDLE;
}

/* Every instance spanned is one, and none twice. */
bool InstanceSpan::invariants_hold() const
{
    AV_SCOPE("InstanceSpan::invariants_hold");
    if (count_ == 0u || count_ > max_instances) return false;
    for (uint32_t index = 0u; index < count_; ++index) {
        if (instances_[index] == VK_NULL_HANDLE) return false;
        for (uint32_t other = 0u; other < index; ++other)
            if (instances_[other] == instances_[index]) return false;
    }
    return true;
}

}  // namespace av::icd
