#include "executor/vulkan/vulkan_lifetimes.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/vulkan/vulkan_destroyer.hpp"

namespace av {

VulkanLifetimes::VulkanLifetimes(BlockSource &blocks, VulkanDestroyer &destroyer)
    : destroyer_(destroyer), pool_(blocks), nodes_(blocks, Pool::paged), holds_(blocks), reached_(0u)
{
    AV_SCOPE("VulkanLifetimes::VulkanLifetimes");
}

VulkanNode *VulkanLifetimes::node_for(uint64_t handle, uint64_t device, uint64_t parent)
{
    AV_SCOPE("VulkanLifetimes::node_for");
    VulkanNode **const found = nodes_.find(handle);
    if (found != nullptr) return *found;
    VulkanNode *made = nullptr;
    if (pool_.create(made, *this, handle, device, parent) != Status::ok) return nullptr;
    uint32_t slot = 0u;
    if (nodes_.insert(handle, made, slot) == Status::ok) return made;
    static_cast<void>(pool_.destroy(made));
    return nullptr;
}

Status VulkanLifetimes::hold(uint64_t owner, uint64_t handle, uint64_t device, uint64_t pool)
{
    AV_SCOPE("VulkanLifetimes::hold");
    if (owner == 0u || handle == 0u) return Status::invalid_parameter;
    const bool  fresh = nodes_.find(handle) == nullptr;
    VulkanNode *const node = node_for(handle, device, pool);
    if (node == nullptr) return Status::no_memory;
    if (fresh && pool != 0u) {
        VulkanNode *const pooled = node_for(pool, device, 0u);
        if (pooled == nullptr) return Status::no_memory;
        const Status linked = holds_.hold(handle, pooled, nullptr);
        if (linked != Status::ok) return linked;
    }
    return holds_.hold(owner, node, nullptr);
}

Status VulkanLifetimes::track(uint64_t handle, uint64_t device)
{
    AV_SCOPE("VulkanLifetimes::track");
    if (handle == 0u) return Status::invalid_parameter;
    return node_for(handle, device, 0u) != nullptr ? Status::ok : Status::no_memory;
}

void VulkanLifetimes::submitted(uint64_t owner, uint64_t mark)
{
    AV_SCOPE("VulkanLifetimes::submitted");
    if (holds_.mark_of(owner) >= mark) return;
    holds_.submitted(owner, mark);
    holds_.for_each_held(owner, [this, mark](VulkanNode *const held) { submitted(held->handle(), mark); });
}

Status VulkanLifetimes::release(uint64_t owner, uint64_t reached)
{
    AV_SCOPE("VulkanLifetimes::release");
    if (reached > reached_) reached_ = reached;
    return holds_.release_list(owner, reached);
}

Status VulkanLifetimes::destroy(VulkanDestroyKind kind, uint64_t device, uint64_t parent, uint64_t handle,
                                uint64_t marked, uint64_t reached)
{
    AV_SCOPE("VulkanLifetimes::destroy");
    if (reached > reached_) reached_ = reached;
    VulkanNode **const found = nodes_.find(handle);
    if (found == nullptr && marked <= reached) {
        destroyer_.destroy(kind, device, parent, handle);
        return Status::ok;
    }
    VulkanNode *const node = node_for(handle, device, parent);
    if (node == nullptr) return Status::no_memory;
    node->request(kind, device, parent);
    return holds_.retire(node, marked, reached);
}

Status VulkanLifetimes::copy(uint64_t to, uint64_t from)
{
    AV_SCOPE("VulkanLifetimes::copy");
    Status copied = Status::ok;
    holds_.for_each_held(from, [&](VulkanNode *const held) {
        if (copied == Status::ok) copied = holds_.hold(to, held, nullptr);
    });
    return copied;
}

uint64_t VulkanLifetimes::mark_of(uint64_t owner) const
{
    AV_SCOPE("VulkanLifetimes::mark_of");
    return holds_.mark_of(owner);
}

void VulkanLifetimes::reap(uint64_t reached)
{
    AV_SCOPE("VulkanLifetimes::reap");
    if (reached > reached_) reached_ = reached;
    holds_.reap(reached);
}

void VulkanLifetimes::finish(VulkanNode &node)
{
    AV_SCOPE("VulkanLifetimes::finish");
    const uint64_t          handle = node.handle();
    const uint64_t          device = node.device();
    const uint64_t          parent = node.parent();
    const VulkanDestroyKind kind = node.kind();
    static_cast<void>(nodes_.erase(handle));
    static_cast<void>(pool_.destroy(&node));
    if (kind != VulkanDestroyKind::none) destroyer_.destroy(kind, device, parent, handle);
    static_cast<void>(holds_.release_list(handle, reached_));
}

void VulkanLifetimes::release_all()
{
    AV_SCOPE("VulkanLifetimes::release_all");
    holds_.release_all();
    nodes_.clear();
    pool_.destroy_all();
}

bool VulkanLifetimes::holding(uint64_t owner, uint64_t handle) const
{
    AV_SCOPE("VulkanLifetimes::holding");
    VulkanNode *const *const found = nodes_.find(handle);
    return found != nullptr && holds_.holding(owner, *found);
}

bool VulkanLifetimes::tracked(uint64_t handle) const
{
    AV_SCOPE("VulkanLifetimes::tracked");
    return nodes_.find(handle) != nullptr;
}

uint32_t VulkanLifetimes::nodes() const
{
    AV_SCOPE("VulkanLifetimes::nodes");
    return nodes_.count();
}

uint32_t VulkanLifetimes::retired() const
{
    AV_SCOPE("VulkanLifetimes::retired");
    return holds_.retired();
}

bool VulkanLifetimes::invariants_hold() const
{
    AV_SCOPE("VulkanLifetimes::invariants_hold");
    if (!pool_.invariants_hold() || !nodes_.invariants_hold() || !holds_.invariants_hold()) return false;
    if (pool_.live() != nodes_.count()) return false;
    bool sound = true;
    nodes_.for_each([&sound](uint64_t handle, VulkanNode *const &node) {
        sound = sound && node != nullptr && node->invariants_hold() && node->handle() == handle;
    });
    return sound;
}

}
