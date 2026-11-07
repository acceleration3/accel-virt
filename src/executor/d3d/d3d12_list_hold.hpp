#ifndef AV_D3D12_LIST_HOLD_HPP
#define AV_D3D12_LIST_HOLD_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

struct IUnknown;

namespace av {

class BlockSource;
class D3d12BlockHeap;

class D3d12ListHold {
public:
    struct Held {
        IUnknown       *object_;
        D3d12BlockHeap *block_;
    };

    explicit D3d12ListHold(BlockSource &blocks);
    D3d12ListHold(const D3d12ListHold &) = delete;
    D3d12ListHold &operator=(const D3d12ListHold &) = delete;

    [[nodiscard]] Status add(const Held &held, bool &fresh);
    void                 submitted(uint64_t mark);
    void                 pop();
    void                 forget_mark();

    bool       holds(const IUnknown *object) const;
    uint64_t   mark() const;
    uint32_t   size() const;
    const Held &at(uint32_t index) const;

    bool invariants_hold() const;

private:
    GrowableArray<Held> held_;
    uint64_t            mark_;
};

}

#endif
