#ifndef AV_D3D12_OBJECT_HOLDS_HPP
#define AV_D3D12_OBJECT_HOLDS_HPP

#include "core/support/growable_array.hpp"
#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "executor/d3d/d3d12_list_hold.hpp"

#include <stdint.h>

struct IUnknown;

namespace av {

class BlockSource;
class D3d12BlockHeap;

class D3d12ObjectHolds {
public:
    explicit D3d12ObjectHolds(BlockSource &blocks);
    D3d12ObjectHolds(const D3d12ObjectHolds &) = delete;
    D3d12ObjectHolds &operator=(const D3d12ObjectHolds &) = delete;

    [[nodiscard]] Status hold(uint32_t list, IUnknown *object, D3d12BlockHeap *block);
    void                 submitted(uint32_t list, uint64_t mark);
    [[nodiscard]] Status release_list(uint32_t list, uint64_t reached);
    [[nodiscard]] Status retire(IUnknown *object, uint64_t mark, uint64_t reached);
    [[nodiscard]] Status inherit(const IUnknown *holder, IUnknown *object);
    void                 reap(uint64_t reached);
    void                 release_all();

    bool     holding(uint32_t list, const IUnknown *object) const;
    uint32_t lists() const;
    uint32_t retired() const;

    bool invariants_hold() const;

private:
    struct Retired {
        IUnknown *object_;
        uint64_t  mark_;
    };

    D3d12ListHold *list_for(uint32_t list);
    void           let_go(const D3d12ListHold::Held &held, uint64_t mark);

    ObjectPool<D3d12ListHold>         pool_;
    KeyedSlots<uint32_t, D3d12ListHold *> lists_;
    GrowableArray<Retired>            retired_;
    BlockSource                      &blocks_;
};

}

#endif
