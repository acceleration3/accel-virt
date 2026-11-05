#ifndef AV_SLICE_BINDINGS_HPP
#define AV_SLICE_BINDINGS_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

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
    bool                 take_bound_to(const SourceMemory &store, Binding &was);
    void                 clear();
    uint32_t             count() const;

    bool invariants_hold() const;

private:
    uint32_t index_of(uint32_t resource) const;

    GrowableArray<Binding> bound_;
};

}

#endif
