#include "executor/d3d/d3d12_object_holds.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d12_block_heap.hpp"
#include "executor/shim/d3d_headers.hpp"

namespace av {

D3d12ObjectHolds::D3d12ObjectHolds(BlockSource &blocks)
    : pool_(blocks), lists_(blocks, Pool::paged), retired_(blocks, Pool::paged), blocks_(blocks)
{
    AV_SCOPE("D3d12ObjectHolds::D3d12ObjectHolds");
}

D3d12ListHold *D3d12ObjectHolds::list_for(uint32_t list)
{
    AV_SCOPE("D3d12ObjectHolds::list_for");
    D3d12ListHold **const found = lists_.find(list);
    if (found != nullptr) return *found;
    D3d12ListHold *made = nullptr;
    if (pool_.create(made, blocks_) != Status::ok) return nullptr;
    uint32_t slot = 0u;
    if (lists_.insert(list, made, slot) == Status::ok) return made;
    static_cast<void>(pool_.destroy(made));
    return nullptr;
}

Status D3d12ObjectHolds::hold(uint32_t list, IUnknown *object, D3d12BlockHeap *block)
{
    AV_SCOPE("D3d12ObjectHolds::hold");
    D3d12ListHold *const kept = list_for(list);
    if (kept == nullptr) {
        if (block != nullptr) block->let_go_resource(0u);
        return Status::no_memory;
    }
    bool         fresh = false;
    const Status added = kept->add(D3d12ListHold::Held{object, block}, fresh);
    if (fresh) {
        object->AddRef();
        return Status::ok;
    }
    if (block != nullptr) block->let_go_resource(0u);
    return added;
}

void D3d12ObjectHolds::submitted(uint32_t list, uint64_t mark)
{
    AV_SCOPE("D3d12ObjectHolds::submitted");
    D3d12ListHold **const found = lists_.find(list);
    if (found != nullptr) (*found)->submitted(mark);
}

void D3d12ObjectHolds::let_go(const D3d12ListHold::Held &held, uint64_t mark)
{
    AV_SCOPE("D3d12ObjectHolds::let_go");
    if (held.block_ != nullptr) held.block_->let_go_resource(mark);
}

Status D3d12ObjectHolds::release_list(uint32_t list, uint64_t reached)
{
    AV_SCOPE("D3d12ObjectHolds::release_list");
    D3d12ListHold **const found = lists_.find(list);
    if (found == nullptr) return Status::ok;
    D3d12ListHold *const kept = *found;
    const uint64_t       mark = kept->mark();
    while (kept->size() != 0u) {
        const D3d12ListHold::Held held = kept->at(kept->size() - 1u);
        if (mark > reached) {
            const Status deferred = retired_.append(Retired{held.object_, mark});
            if (deferred != Status::ok) return deferred;
        } else {
            held.object_->Release();
        }
        let_go(held, mark);
        kept->pop();
    }
    static_cast<void>(lists_.erase(list));
    static_cast<void>(pool_.destroy(kept));
    return Status::ok;
}

Status D3d12ObjectHolds::retire(IUnknown *object, uint64_t mark, uint64_t reached)
{
    AV_SCOPE("D3d12ObjectHolds::retire");
    if (object == nullptr) return Status::invalid_parameter;
    if (mark <= reached) {
        object->Release();
        return Status::ok;
    }
    return retired_.append(Retired{object, mark});
}

void D3d12ObjectHolds::reap(uint64_t reached)
{
    AV_SCOPE("D3d12ObjectHolds::reap");
    uint32_t index = 0u;
    while (index < retired_.size()) {
        const Retired due = retired_.at(index);
        if (due.mark_ > reached) {
            index++;
            continue;
        }
        due.object_->Release();
        retired_.remove_swap(index);
    }
}

void D3d12ObjectHolds::release_all()
{
    AV_SCOPE("D3d12ObjectHolds::release_all");
    for (Retired &due : retired_) due.object_->Release();
    retired_.clear();
    lists_.for_each([this](uint32_t, D3d12ListHold *&kept) {
        for (uint32_t index = 0u; index < kept->size(); index++) {
            kept->at(index).object_->Release();
            let_go(kept->at(index), 0u);
        }
    });
    lists_.clear();
    pool_.destroy_all();
}

bool D3d12ObjectHolds::holding(uint32_t list, const IUnknown *object) const
{
    AV_SCOPE("D3d12ObjectHolds::holding");
    D3d12ListHold *const *const found = lists_.find(list);
    return found != nullptr && (*found)->holds(object);
}

uint32_t D3d12ObjectHolds::lists() const
{
    AV_SCOPE("D3d12ObjectHolds::lists");
    return lists_.count();
}

uint32_t D3d12ObjectHolds::retired() const
{
    AV_SCOPE("D3d12ObjectHolds::retired");
    return retired_.size();
}

bool D3d12ObjectHolds::invariants_hold() const
{
    AV_SCOPE("D3d12ObjectHolds::invariants_hold");
    if (!pool_.invariants_hold() || !lists_.invariants_hold() || !retired_.invariants_hold()) return false;
    if (pool_.live() != lists_.count()) return false;
    bool sound = true;
    lists_.for_each([&sound](uint32_t, D3d12ListHold *const &kept) {
        sound = sound && kept != nullptr && kept->invariants_hold();
    });
    for (const Retired &due : retired_)
        if (due.object_ == nullptr) return false;
    return sound;
}

}
