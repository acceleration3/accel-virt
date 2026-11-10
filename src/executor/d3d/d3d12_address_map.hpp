#ifndef AV_D3D12_ADDRESS_MAP_HPP
#define AV_D3D12_ADDRESS_MAP_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"
#include "executor/gpu_range_table.hpp"

#include <stdint.h>

namespace av {

class BlockSource;

class D3d12AddressMap {
public:
    explicit D3d12AddressMap(BlockSource &blocks);

    [[nodiscard]] Status insert(uint64_t guest, uint64_t size, uint64_t host, uint32_t id);
    void               remove(uint32_t id);
    uint64_t           translate(uint64_t guest, uint64_t bytes) const;
    uint32_t           owner(uint64_t guest, uint64_t &offset) const;
    void               clear();
    uint32_t           count() const;
    uint32_t           version() const;
    uint32_t           rows(GpuRangeTable::Row *out, uint32_t capacity) const;

    bool invariants_hold() const;

private:
    struct Range {
        uint64_t guest_;
        uint64_t size_;
        uint64_t host_;
        uint32_t id_;
    };

    uint32_t below(uint64_t guest) const;

    GrowableArray<Range> ranges_;
    uint32_t             version_;
};

}

#endif
