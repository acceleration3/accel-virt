#ifndef AV_D3D12_DESCRIPTOR_HOLDS_HPP
#define AV_D3D12_DESCRIPTOR_HOLDS_HPP

#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d12_block_heap.hpp"
#include "executor/d3d/d3d12_object_holds.hpp"
#include "executor/descriptor_tables.hpp"

#include <stdint.h>

struct IUnknown;

namespace av {

class BlockSource;

class D3d12DescriptorHolds {
public:
    using Tables = DescriptorTables<IUnknown, D3d12BlockHeap>;
    using View = Tables::Slot;

    D3d12DescriptorHolds(BlockSource &blocks, D3d12ObjectHolds &holds);
    D3d12DescriptorHolds(const D3d12DescriptorHolds &) = delete;
    D3d12DescriptorHolds &operator=(const D3d12DescriptorHolds &) = delete;

    template <typename Reached>
    [[nodiscard]] Status note(uint32_t heap, uint32_t index, const View &view, uint64_t marked, Reached reached);
    template <typename Reached>
    [[nodiscard]] Status copy(uint32_t to_heap, uint32_t to_index, uint32_t from_heap, uint32_t from_index,
                              uint32_t count, uint64_t marked, Reached reached);
    template <typename Reached>
    [[nodiscard]] Status destroy_heap(uint32_t heap, const IUnknown *object, uint64_t marked, Reached reached);
    void                 release_all();

    bool            holds(uint32_t heap, uint32_t index, const IUnknown *resource) const;
    IUnknown       *resource_at(uint32_t heap, uint32_t index) const;
    D3d12BlockHeap *pin_at(uint32_t heap, uint32_t index) const;
    uint32_t        count() const;

    bool invariants_hold() const;

private:
    uint64_t key_of(uint32_t index) const;

    template <typename Reached>
    [[nodiscard]] Status let_go(const View &view, uint64_t marked, Reached reached);

    D3d12ObjectHolds &holds_;
    Tables            tables_;
};

template <typename Reached>
Status D3d12DescriptorHolds::let_go(const View &view, uint64_t marked, Reached reached)
{
    AV_SCOPE("D3d12DescriptorHolds::let_go");
    Status let = Status::ok;
    IUnknown *const       named[2] = {view.first_, view.second_};
    D3d12BlockHeap *const pins[2] = {view.first_pin_, view.second_pin_};
    for (uint32_t which = 0u; which < 2u; which++) {
        holds_.let_go_pin(pins[which], marked);
        if (named[which] == nullptr) continue;
        const Status retired = holds_.retire(named[which], marked, marked == 0u ? 0u : reached());
        if (retired != Status::ok) let = retired;
    }
    return let;
}

template <typename Reached>
Status D3d12DescriptorHolds::note(uint32_t heap, uint32_t index, const View &view, uint64_t marked, Reached reached)
{
    AV_SCOPE("D3d12DescriptorHolds::note");
    View         displaced = {};
    const Status put_status = tables_.put(heap, key_of(index), view, displaced);
    if (put_status != Status::ok) {
        holds_.let_go_pin(view.first_pin_, 0u);
        holds_.let_go_pin(view.second_pin_, 0u);
        return put_status;
    }
    return let_go(displaced, marked, reached);
}

template <typename Reached>
Status D3d12DescriptorHolds::copy(uint32_t to_heap, uint32_t to_index, uint32_t from_heap, uint32_t from_index,
                                  uint32_t count, uint64_t marked, Reached reached)
{
    AV_SCOPE("D3d12DescriptorHolds::copy");
    for (uint32_t step = 0u; step < count; step++) {
        const View *const from = tables_.find(from_heap, key_of(from_index + step));
        View              source = from != nullptr ? *from : View{};
        source.first_pin_ = holds_.repinned(source.first_pin_);
        source.second_pin_ = holds_.repinned(source.second_pin_);
        const Status noted = note(to_heap, to_index + step, source, marked, reached);
        if (noted != Status::ok) return noted;
    }
    return Status::ok;
}

template <typename Reached>
Status D3d12DescriptorHolds::destroy_heap(uint32_t heap, const IUnknown *object, uint64_t marked, Reached reached)
{
    AV_SCOPE("D3d12DescriptorHolds::destroy_heap");
    Status gone = holds_.inherit_from(object, [&](auto sink) {
        tables_.for_each_slot(heap, [&](const View &view) {
            sink(view.first_, view.first_pin_);
            sink(view.second_, view.second_pin_);
        });
    });
    const Status released = tables_.release(heap, [&](const View &view, uint64_t) { return let_go(view, marked, reached); });
    return gone != Status::ok ? gone : released;
}

}

#endif
