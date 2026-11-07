#ifndef AV_D3D12_DESCRIPTOR_HOLDS_HPP
#define AV_D3D12_DESCRIPTOR_HOLDS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/d3d/d3d12_object_holds.hpp"

#include <stdint.h>

struct IUnknown;

namespace av {

class BlockSource;

class D3d12DescriptorHolds {
public:
    D3d12DescriptorHolds(BlockSource &blocks, D3d12ObjectHolds &holds);
    D3d12DescriptorHolds(const D3d12DescriptorHolds &) = delete;
    D3d12DescriptorHolds &operator=(const D3d12DescriptorHolds &) = delete;

    template <typename Reached>
    [[nodiscard]] Status note(uint32_t heap, uint32_t index, IUnknown *resource, IUnknown *counter, uint64_t marked,
                              Reached reached);
    template <typename Reached>
    [[nodiscard]] Status copy(uint32_t to_heap, uint32_t to_index, uint32_t from_heap, uint32_t from_index,
                              uint32_t count, uint64_t marked, Reached reached);
    template <typename Reached>
    [[nodiscard]] Status destroy_heap(uint32_t heap, const IUnknown *object, uint64_t marked, Reached reached);
    void                 release_all();

    bool     holds(uint32_t heap, uint32_t index, const IUnknown *resource) const;
    uint32_t count() const;

    bool invariants_hold() const;

private:
    struct Slot {
        IUnknown *resource_;
        IUnknown *counter_;
    };

    uint64_t key_of(uint32_t heap, uint32_t index) const;

    [[nodiscard]] Status put(uint64_t key, IUnknown *resource, IUnknown *counter, Slot &displaced);
    template <typename Reached>
    [[nodiscard]] Status let_go(const Slot &slot, uint64_t marked, Reached reached);

    D3d12ObjectHolds             &holds_;
    KeyedSlots<uint64_t, Slot>    slots_;
};

template <typename Reached>
Status D3d12DescriptorHolds::let_go(const Slot &slot, uint64_t marked, Reached reached)
{
    AV_SCOPE("D3d12DescriptorHolds::let_go");
    Status let = Status::ok;
    IUnknown *const named[2] = {slot.resource_, slot.counter_};
    for (IUnknown *const object : named) {
        if (object == nullptr) continue;
        const Status retired = holds_.retire(object, marked, marked == 0u ? 0u : reached());
        if (retired != Status::ok) let = retired;
    }
    return let;
}

template <typename Reached>
Status D3d12DescriptorHolds::note(uint32_t heap, uint32_t index, IUnknown *resource, IUnknown *counter,
                                  uint64_t marked, Reached reached)
{
    AV_SCOPE("D3d12DescriptorHolds::note");
    Slot         displaced = {};
    const Status put_status = put(key_of(heap, index), resource, counter, displaced);
    if (put_status != Status::ok) return put_status;
    return let_go(displaced, marked, reached);
}

template <typename Reached>
Status D3d12DescriptorHolds::copy(uint32_t to_heap, uint32_t to_index, uint32_t from_heap, uint32_t from_index,
                                  uint32_t count, uint64_t marked, Reached reached)
{
    AV_SCOPE("D3d12DescriptorHolds::copy");
    for (uint32_t step = 0u; step < count; step++) {
        const Slot *const from = slots_.find(key_of(from_heap, from_index + step));
        const Slot        source = from != nullptr ? *from : Slot{};
        const Status      noted = note(to_heap, to_index + step, source.resource_, source.counter_, marked, reached);
        if (noted != Status::ok) return noted;
    }
    return Status::ok;
}

template <typename Reached>
Status D3d12DescriptorHolds::destroy_heap(uint32_t heap, const IUnknown *object, uint64_t marked, Reached reached)
{
    AV_SCOPE("D3d12DescriptorHolds::destroy_heap");
    Status gone = Status::ok;
    slots_.erase_if([&](uint64_t key, Slot &slot) {
        if (static_cast<uint32_t>(key >> 32u) != heap) return false;
        IUnknown *const named[2] = {slot.resource_, slot.counter_};
        for (IUnknown *const kept : named) {
            if (kept == nullptr) continue;
            const Status inherited = holds_.inherit(object, kept);
            if (inherited != Status::ok) gone = inherited;
        }
        const Status let = let_go(slot, marked, reached);
        if (let != Status::ok) gone = let;
        return true;
    });
    return gone;
}

}

#endif
