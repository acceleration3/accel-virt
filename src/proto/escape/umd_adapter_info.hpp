#ifndef AV_UMD_ADAPTER_INFO_HPP
#define AV_UMD_ADAPTER_INFO_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class UmdAdapterInfo {
public:
    static constexpr size_t   base_size = 16u;
    static constexpr size_t   encoded_size = 24u;
    static constexpr uint64_t feature_present_snapshots = 1u;
    static constexpr uint64_t feature_integrated_gpu = 2u;

    UmdAdapterInfo();
    UmdAdapterInfo(uint64_t luid, uint64_t features);

    uint64_t luid() const;
    bool     offers(uint64_t feature) const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity, size_t &written) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint64_t tag = 0x0044494C555641ull;

    uint64_t luid_;
    uint64_t features_;
};

}

#endif
