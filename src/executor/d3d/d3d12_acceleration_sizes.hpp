#ifndef AV_D3D12_ACCELERATION_SIZES_HPP
#define AV_D3D12_ACCELERATION_SIZES_HPP

#include "core/support/growable_array.hpp"

#include <stdint.h>

namespace av {

class BlockSource;

class D3d12AccelerationSizes {
public:
    static constexpr uint32_t most_structures = 1u << 20u;

    explicit D3d12AccelerationSizes(BlockSource &blocks);
    D3d12AccelerationSizes(const D3d12AccelerationSizes &) = delete;
    D3d12AccelerationSizes &operator=(const D3d12AccelerationSizes &) = delete;

    [[nodiscard]] bool note(uint64_t host, uint64_t bytes);
    bool               find(uint64_t host, uint64_t &bytes) const;
    uint32_t           count() const;
    void               clear();

    bool invariants_hold() const;

private:
    struct Entry {
        uint64_t host_;
        uint64_t bytes_;
    };

    GrowableArray<Entry> entries_;
};

}

#endif
