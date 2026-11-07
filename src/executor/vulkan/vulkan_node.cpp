#include "executor/vulkan/vulkan_node.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/vulkan/vulkan_lifetimes.hpp"

namespace av {

VulkanNode::VulkanNode(VulkanLifetimes &owner, uint64_t handle, uint64_t device, uint64_t parent)
    : owner_(owner), handle_(handle), device_(device), parent_(parent), references_(1u), kind_(VulkanDestroyKind::none)
{
    AV_SCOPE("VulkanNode::VulkanNode");
}

void VulkanNode::AddRef()
{
    AV_SCOPE("VulkanNode::AddRef");
    references_++;
}

void VulkanNode::Release()
{
    AV_SCOPE("VulkanNode::Release");
    if (--references_ == 0u) owner_.finish(*this);
}

void VulkanNode::request(VulkanDestroyKind kind, uint64_t device, uint64_t parent)
{
    AV_SCOPE("VulkanNode::request");
    kind_ = kind;
    device_ = device;
    parent_ = parent;
}

uint64_t VulkanNode::handle() const
{
    AV_SCOPE("VulkanNode::handle");
    return handle_;
}

uint64_t VulkanNode::device() const
{
    AV_SCOPE("VulkanNode::device");
    return device_;
}

uint64_t VulkanNode::parent() const
{
    AV_SCOPE("VulkanNode::parent");
    return parent_;
}

VulkanDestroyKind VulkanNode::kind() const
{
    AV_SCOPE("VulkanNode::kind");
    return kind_;
}

uint32_t VulkanNode::references() const
{
    AV_SCOPE("VulkanNode::references");
    return references_;
}

bool VulkanNode::invariants_hold() const
{
    AV_SCOPE("VulkanNode::invariants_hold");
    return handle_ != 0u && references_ != 0u;
}

}
