#ifndef AV_ADAPTER_DESCRIPTION_HPP
#define AV_ADAPTER_DESCRIPTION_HPP

#include "core/platform/platform.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class GpuIdentity;

class AdapterDescription {
public:
    explicit AdapterDescription(Platform &platform);
    AdapterDescription(const AdapterDescription &) = delete;
    AdapterDescription &operator=(const AdapterDescription &) = delete;

    [[nodiscard]] Status publish(void *physical_device_object, const GpuIdentity &gpu);
    uint32_t             published() const;

private:
    static constexpr uint64_t largest_u32 = 0xFFFFFFFFu;

    Platform &platform_;
    uint32_t  published_;
};

}

#endif
