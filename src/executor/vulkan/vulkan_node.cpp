#include "executor/vulkan/vulkan_node.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/vulkan/vulkan_lifetimes.hpp"

namespace av {

VulkanNode::VulkanNode(VulkanLifetimes &owner, uint64_t handle, uint64_t device, uint64_t parent)
    : owner_(owner), handle_(handle), device_(device), parent_(parent), references_(1u), stamped_(0u), kind_(VulkanDestroyKind::none),
      orphaned_(false), scheduled_(false), pool_(nullptr), first_child_(nullptr), next_(nullptr), previous_(nullptr)
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

void VulkanNode::link(VulkanNode *pool)
{
    AV_SCOPE("VulkanNode::link");
    pool_ = pool;
    pool->AddRef();
    previous_ = nullptr;
    next_ = pool->first_child_;
    if (next_ != nullptr) next_->previous_ = this;
    pool->first_child_ = this;
}

void VulkanNode::unlink()
{
    AV_SCOPE("VulkanNode::unlink");
    VulkanNode *const pool = pool_;
    if (pool == nullptr) return;
    if (previous_ != nullptr) previous_->next_ = next_;
    else pool->first_child_ = next_;
    if (next_ != nullptr) next_->previous_ = previous_;
    pool_ = nullptr;
    next_ = nullptr;
    previous_ = nullptr;
    pool->Release();
}

void VulkanNode::orphan()
{
    AV_SCOPE("VulkanNode::orphan");
    const bool alive = kind_ == VulkanDestroyKind::none;
    kind_ = VulkanDestroyKind::none;
    orphaned_ = true;
    unlink();
    if (alive) Release();
}

void VulkanNode::schedule()
{
    AV_SCOPE("VulkanNode::schedule");
    scheduled_ = true;
}

void VulkanNode::unschedule()
{
    AV_SCOPE("VulkanNode::unschedule");
    scheduled_ = false;
}

void VulkanNode::stamp(uint64_t mark)
{
    AV_SCOPE("VulkanNode::stamp");
    if (mark > stamped_) stamped_ = mark;
}

uint64_t VulkanNode::stamped() const
{
    AV_SCOPE("VulkanNode::stamped");
    return stamped_;
}

bool VulkanNode::scheduled() const
{
    AV_SCOPE("VulkanNode::scheduled");
    return scheduled_;
}

VulkanNode *VulkanNode::next_child() const
{
    AV_SCOPE("VulkanNode::next_child");
    return next_;
}

VulkanNode *VulkanNode::pool() const
{
    AV_SCOPE("VulkanNode::pool");
    return pool_;
}

VulkanNode *VulkanNode::first_child() const
{
    AV_SCOPE("VulkanNode::first_child");
    return first_child_;
}

bool VulkanNode::orphaned() const
{
    AV_SCOPE("VulkanNode::orphaned");
    return orphaned_;
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
    if (handle_ == 0u || references_ == 0u) return false;
    if (pool_ != nullptr && pool_->references_ == 0u) return false;
    return next_ == nullptr || (next_->previous_ == this && next_->pool_ == pool_);
}

}
