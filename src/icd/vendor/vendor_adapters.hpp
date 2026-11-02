#ifndef AV_VENDOR_VENDOR_ADAPTERS_HPP
#define AV_VENDOR_VENDOR_ADAPTERS_HPP

#include "guest/kmt/avgpu_adapters.hpp"
#include "core/support/status.hpp"
#include "icd/vendor/vendor_adapter.hpp"

#include <stdint.h>

namespace av {
class AvgpuAdapters;
class Kmt;
}

namespace av::vendor {

class VendorAdapters {
public:
    static constexpr uint32_t max_adapters = AvgpuAdapters::max_adapters;

    explicit VendorAdapters(Kmt &kmt);
    VendorAdapters(const VendorAdapters &) = delete;
    VendorAdapters &operator=(const VendorAdapters &) = delete;

    [[nodiscard]] Status learn();

    uint32_t             count_of(uint32_t vendor) const;
    const VendorAdapter *nth_of(uint32_t vendor, uint32_t index) const;
    [[nodiscard]] Status local_memory_left(const VendorAdapter &adapter, uint64_t &bytes);

private:
    bool describe(const AvgpuAdapters &avgpu, uint32_t index, VendorAdapter &out);

    Kmt          &kmt_;
    VendorAdapter adapters_[max_adapters];
    uint32_t      count_;
};

}

#endif
