#ifndef AV_GPU_ADDRESS_REGISTRY_HPP
#define AV_GPU_ADDRESS_REGISTRY_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlockSource;

class GpuAddressRegistry {
public:
    explicit GpuAddressRegistry(BlockSource &blocks);
    GpuAddressRegistry(const GpuAddressRegistry &) = delete;
    GpuAddressRegistry &operator=(const GpuAddressRegistry &) = delete;

    [[nodiscard]] Status learn(uint64_t device, uint64_t owner, uint64_t address, uint64_t bytes);
    void                 forget_owner(uint64_t owner);
    void                 forget_device(uint64_t device);
    bool                 covers(uint64_t device, uint64_t address, uint64_t bytes) const;
    uint64_t             room_after(uint64_t device, uint64_t address) const;
    uint32_t             count() const;
    bool                 invariants_hold() const;

private:
    struct Row {
        uint64_t base_;
        uint64_t bytes_;
        uint64_t owner_;
        uint64_t device_;
    };

    uint32_t row_holding(uint64_t address) const;

    GrowableArray<Row> rows_;
};

}

#endif
