#include "executor/vulkan/vulkan_lifetimes.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/vulkan/vulkan_destroyer.hpp"

namespace av {

namespace {

constexpr uint64_t everything = ~uint64_t{0};

}

VulkanLifetimes::VulkanLifetimes(BlockSource &blocks, VulkanDestroyer &destroyer)
    : destroyer_(destroyer), pool_(blocks), nodes_(blocks, Pool::paged), holds_(blocks), tables_(blocks), reached_(0u),
      leaking_(false)
{
    AV_SCOPE("VulkanLifetimes::VulkanLifetimes");
}

uint64_t VulkanLifetimes::key_of(const Descriptor &at) const
{
    AV_SCOPE("VulkanLifetimes::key_of");
    return (uint64_t{at.binding_} << 32u) | at.element_;
}

VulkanNode *VulkanLifetimes::node_for(uint64_t handle, uint64_t device, uint64_t parent)
{
    AV_SCOPE("VulkanLifetimes::node_for");
    VulkanNode **const found = nodes_.find(handle);
    if (found != nullptr) return *found;
    VulkanNode *const pooled = parent != 0u ? node_for(parent, device, 0u) : nullptr;
    if (parent != 0u && pooled == nullptr) return nullptr;
    VulkanNode *made = nullptr;
    if (pool_.create(made, *this, handle, device, parent) != Status::ok) return nullptr;
    uint32_t slot = 0u;
    if (nodes_.insert(handle, made, slot) != Status::ok) {
        static_cast<void>(pool_.destroy(made));
        return nullptr;
    }
    if (pooled != nullptr) made->link(pooled);
    return made;
}

Status VulkanLifetimes::hold(uint64_t owner, uint64_t handle, uint64_t device, uint64_t pool)
{
    AV_SCOPE("VulkanLifetimes::hold");
    if (owner == 0u || handle == 0u) return Status::invalid_parameter;
    VulkanNode *const node = node_for(handle, device, pool);
    if (node == nullptr) return Status::no_memory;
    return holds_.hold(owner, node, nullptr);
}

Status VulkanLifetimes::track(uint64_t handle, uint64_t device)
{
    AV_SCOPE("VulkanLifetimes::track");
    if (handle == 0u) return Status::invalid_parameter;
    return node_for(handle, device, 0u) != nullptr ? Status::ok : Status::no_memory;
}

Status VulkanLifetimes::let_go(Tables::Slot &slot, uint64_t mark, uint64_t reached)
{
    AV_SCOPE("VulkanLifetimes::let_go");
    if (slot.first_ != nullptr) {
        const Status retired = holds_.retire(slot.first_, mark, reached);
        if (retired != Status::ok) return retired;
        slot.first_ = nullptr;
    }
    if (slot.second_ == nullptr) return Status::ok;
    const Status retired = holds_.retire(slot.second_, mark, reached);
    if (retired == Status::ok) slot.second_ = nullptr;
    return retired;
}

Status VulkanLifetimes::write(uint64_t set, const Descriptor &at, uint64_t first, uint64_t second, uint64_t device,
                              uint64_t reached)
{
    AV_SCOPE("VulkanLifetimes::write");
    if (set == 0u) return Status::invalid_parameter;
    if (reached > reached_) reached_ = reached;
    VulkanNode *const named = first != 0u ? node_for(first, device, 0u) : nullptr;
    VulkanNode *const paired = second != 0u ? node_for(second, device, 0u) : nullptr;
    if ((first != 0u && named == nullptr) || (second != 0u && paired == nullptr)) return Status::no_memory;
    Tables::Slot displaced = {};
    const Status put = tables_.put(set, key_of(at), Tables::Slot{named, paired, nullptr, nullptr}, displaced);
    if (put != Status::ok) return put;
    return let_go(displaced, tables_.mark_of(set), reached);
}

Status VulkanLifetimes::copy(uint64_t to, const Descriptor &to_at, uint64_t from, const Descriptor &from_at,
                             uint32_t count, uint64_t reached)
{
    AV_SCOPE("VulkanLifetimes::copy");
    if (reached > reached_) reached_ = reached;
    for (uint32_t step = 0u; step < count; step++) {
        const Tables::Slot *const source = tables_.find(from, key_of(Descriptor{from_at.binding_, from_at.element_ + step}));
        Tables::Slot displaced = {};
        const Status put = tables_.put(to, key_of(Descriptor{to_at.binding_, to_at.element_ + step}),
                                       source != nullptr ? *source : Tables::Slot{}, displaced);
        if (put != Status::ok) return put;
        const Status let = let_go(displaced, tables_.mark_of(to), reached);
        if (let != Status::ok) return let;
    }
    return Status::ok;
}

void VulkanLifetimes::submitted(uint64_t owner, uint64_t mark)
{
    AV_SCOPE("VulkanLifetimes::submitted");
    if (mark_of(owner) >= mark) return;
    holds_.submitted(owner, mark);
    tables_.submitted(owner, mark);
    VulkanNode **const named = nodes_.find(owner);
    if (named != nullptr) (*named)->stamp(mark);
    holds_.for_each_held(owner, [this, mark](VulkanNode *const held) {
        if (held->pool() != nullptr) submitted(held->handle(), mark);
        else tables_.submitted(held->handle(), mark);
    });
}

Status VulkanLifetimes::release(uint64_t owner, uint64_t reached)
{
    AV_SCOPE("VulkanLifetimes::release");
    if (reached > reached_) reached_ = reached;
    const Status listed = holds_.release_list(owner, reached);
    const Status tabled = tables_.release(owner, [this, reached](Tables::Slot &slot, uint64_t mark) {
        return let_go(slot, mark, reached);
    });
    return listed != Status::ok ? listed : tabled;
}

void VulkanLifetimes::orphan_children(uint64_t pool)
{
    AV_SCOPE("VulkanLifetimes::orphan_children");
    VulkanNode **const found = nodes_.find(pool);
    if (found == nullptr) return;
    VulkanNode *child = (*found)->first_child();
    while (child != nullptr) {
        VulkanNode *const next = child->next_child();
        VulkanNode **const mapped = nodes_.find(child->handle());
        if (mapped != nullptr && *mapped == child) static_cast<void>(nodes_.erase(child->handle()));
        child->orphan();
        child = next;
    }
}

Status VulkanLifetimes::destroy(VulkanDestroyKind kind, uint64_t device, uint64_t parent, uint64_t handle,
                                uint64_t marked, uint64_t reached)
{
    AV_SCOPE("VulkanLifetimes::destroy");
    if (reached > reached_) reached_ = reached;
    VulkanNode **const found = nodes_.find(handle);
    if (found != nullptr) {
        (*found)->AddRef();
        holds_.let_go_completed(*found, reached_);
        (*found)->Release();
    }
    if (found == nullptr && marked <= reached) {
        destroyer_.destroy(kind, device, parent, handle);
        return Status::ok;
    }
    VulkanNode *const node = node_for(handle, device, parent);
    if (node == nullptr) return Status::no_memory;
    const bool queued = node->scheduled();
    if (queued) node->AddRef();
    node->request(kind, device, parent);
    if (marked > reached) node->schedule();
    const Status retired = holds_.retire(node, marked, reached);
    if (retired == Status::ok) return retired;
    if (queued) node->Release();
    else node->unschedule();
    return retired;
}

uint64_t VulkanLifetimes::mark_of(uint64_t owner) const
{
    AV_SCOPE("VulkanLifetimes::mark_of");
    const uint64_t listed = holds_.mark_of(owner);
    const uint64_t tabled = tables_.mark_of(owner);
    VulkanNode *const *const named = nodes_.find(owner);
    const uint64_t stamped = named != nullptr ? (*named)->stamped() : 0u;
    const uint64_t newest = listed > tabled ? listed : tabled;
    return newest > stamped ? newest : stamped;
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
    const bool              orphaned = node.orphaned();
    VulkanNode **const      mapped = nodes_.find(handle);
    if (mapped != nullptr && *mapped == &node) static_cast<void>(nodes_.erase(handle));
    if (kind != VulkanDestroyKind::none && !leaking_) destroyer_.destroy(kind, device, parent, handle);
    if (!orphaned) static_cast<void>(release(handle, reached_));
    node.unlink();
    static_cast<void>(pool_.destroy(&node));
}

void VulkanLifetimes::release_all(bool idle)
{
    AV_SCOPE("VulkanLifetimes::release_all");
    leaking_ = !idle;
    reached_ = everything;
    do {
        holds_.release_all();
        tables_.drain_all([](Tables::Slot &slot) {
            if (slot.first_ != nullptr) slot.first_->Release();
            if (slot.second_ != nullptr) slot.second_->Release();
        });
    } while (holds_.lists() != 0u || holds_.retired() != 0u || tables_.count() != 0u);
    destroy_pending();
    nodes_.clear();
    pool_.destroy_all();
}

void VulkanLifetimes::destroy_pending()
{
    AV_SCOPE("VulkanLifetimes::destroy_pending");
    nodes_.for_each([&](uint64_t, VulkanNode *&node) {
        if (node->kind() == VulkanDestroyKind::none) return;
        if (!leaking_) destroyer_.destroy(node->kind(), node->device(), node->parent(), node->handle());
        node->request(VulkanDestroyKind::none, node->device(), node->parent());
    });
}

bool VulkanLifetimes::holding(uint64_t owner, uint64_t handle) const
{
    AV_SCOPE("VulkanLifetimes::holding");
    VulkanNode *const *const found = nodes_.find(handle);
    if (found == nullptr) return false;
    if (holds_.holding(owner, *found)) return true;
    bool named = false;
    tables_.for_each_slot(owner, [&](const Tables::Slot &slot) {
        named = named || slot.first_ == *found || slot.second_ == *found;
    });
    return named;
}

bool VulkanLifetimes::described(uint64_t set, const Descriptor &at, uint64_t handle) const
{
    AV_SCOPE("VulkanLifetimes::described");
    const Tables::Slot *const slot = tables_.find(set, key_of(at));
    return slot != nullptr && ((slot->first_ != nullptr && slot->first_->handle() == handle) ||
                               (slot->second_ != nullptr && slot->second_->handle() == handle));
}

bool VulkanLifetimes::tracked(uint64_t handle) const
{
    AV_SCOPE("VulkanLifetimes::tracked");
    return nodes_.find(handle) != nullptr;
}

bool VulkanLifetimes::linked(uint64_t handle, uint64_t pool) const
{
    AV_SCOPE("VulkanLifetimes::linked");
    VulkanNode *const *const found = nodes_.find(handle);
    return found != nullptr && (*found)->pool() != nullptr && (*found)->pool()->handle() == pool;
}

bool VulkanLifetimes::reset_pending(uint64_t pool) const
{
    AV_SCOPE("VulkanLifetimes::reset_pending");
    VulkanNode *const *const found = nodes_.find(pool);
    return found != nullptr && (*found)->kind() == VulkanDestroyKind::descriptor_pool_reset;
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

uint32_t VulkanLifetimes::descriptors() const
{
    AV_SCOPE("VulkanLifetimes::descriptors");
    return tables_.count();
}

bool VulkanLifetimes::invariants_hold() const
{
    AV_SCOPE("VulkanLifetimes::invariants_hold");
    if (!pool_.invariants_hold() || !nodes_.invariants_hold() || !holds_.invariants_hold() ||
        !tables_.invariants_hold())
        return false;
    if (pool_.live() < nodes_.count()) return false;
    bool sound = true;
    nodes_.for_each([&sound](uint64_t handle, VulkanNode *const &node) {
        sound = sound && node != nullptr && node->invariants_hold() && node->handle() == handle;
    });
    return sound;
}

}
