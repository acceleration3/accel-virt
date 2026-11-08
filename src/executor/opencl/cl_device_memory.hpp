#ifndef AV_CL_DEVICE_MEMORY_HPP
#define AV_CL_DEVICE_MEMORY_HPP

#include "core/support/keyed_slots.hpp"
#include "executor/vram_account.hpp"

#include <CL/cl.h>

#include <stdint.h>

namespace av {

class BlockSource;
class ByteLedger;

class ClDeviceMemory {
public:
    explicit ClDeviceMemory(BlockSource &blocks);
    ClDeviceMemory(const ClDeviceMemory &) = delete;
    ClDeviceMemory &operator=(const ClDeviceMemory &) = delete;

    static constexpr uint64_t host_bit = 1ull << 63;

    void bind(ByteLedger *vram, ByteLedger *host);

    [[nodiscard]] cl_int claim(cl_mem_flags flags, uint64_t bytes, uint64_t &claimed);
    [[nodiscard]] bool   keep(cl_mem memory, uint64_t claimed);
    void                 unclaim(uint64_t claimed);
    uint64_t             forget(cl_mem memory);
    uint64_t             clear();

    uint64_t held() const;
    uint64_t host_held() const;
    uint64_t cap() const;
    uint64_t free_bytes() const;
    uint32_t objects() const;

    bool invariants_hold() const;

private:
    VramAccount                   account_;
    VramAccount                   host_;
    KeyedSlots<uint64_t, uint64_t> sizes_;
};

}

#endif
