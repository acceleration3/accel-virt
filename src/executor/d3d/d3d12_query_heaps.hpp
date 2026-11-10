#ifndef AV_D3D12_QUERY_HEAPS_HPP
#define AV_D3D12_QUERY_HEAPS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlockSource;

class D3d12QueryHeaps {
public:
    explicit D3d12QueryHeaps(BlockSource &blocks);
    D3d12QueryHeaps(const D3d12QueryHeaps &) = delete;
    D3d12QueryHeaps &operator=(const D3d12QueryHeaps &) = delete;

    [[nodiscard]] Status remember(uint32_t heap, uint32_t heap_type, uint32_t count);
    void                 forget(uint32_t heap);
    void                 clear();

    bool serves(uint32_t heap_type) const;
    bool admits(uint32_t heap, uint32_t query_type, uint32_t first, uint32_t count) const;
    uint32_t known() const;

    bool invariants_hold() const;

private:
    struct Info {
        uint32_t type_;
        uint32_t count_;
    };

    KeyedSlots<uint32_t, Info> heaps_;
};

}

#endif
