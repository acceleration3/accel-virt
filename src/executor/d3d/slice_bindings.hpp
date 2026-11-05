#ifndef AV_SLICE_BINDINGS_HPP
#define AV_SLICE_BINDINGS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"
#include "core/trace/function_scope.hpp"

#include <stdint.h>

namespace av {

class BlockSource;
class SourceMemory;

class SliceBindings {
public:
    struct Binding {
        uint32_t            resource_;
        const SourceMemory *store_;
        const uint8_t      *slice_;
        uint64_t            bytes_;
    };

    explicit SliceBindings(BlockSource &blocks);
    SliceBindings(const SliceBindings &) = delete;
    SliceBindings &operator=(const SliceBindings &) = delete;

    [[nodiscard]] Status bind(const Binding &binding);
    bool                 take(uint32_t resource, Binding &was);
    bool                 find(uint32_t resource, Binding &was) const;
    template <typename Visitor>
    void                 take_all_bound_to(const SourceMemory &store, Visitor visit);
    void                 clear();
    uint32_t             count() const;

    bool invariants_hold() const;

private:
    KeyedSlots<uint32_t, Binding> bound_;
};

template <typename Visitor>
void SliceBindings::take_all_bound_to(const SourceMemory &store, Visitor visit)
{
    AV_SCOPE("SliceBindings::take_all_bound_to");
    bound_.erase_if([&store, &visit](uint32_t, const Binding &binding) {
        if (binding.store_ != &store) return false;
        visit(binding);
        return true;
    });
}

}

#endif
