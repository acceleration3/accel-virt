/* A Vulkan device UUID (VkPhysicalDeviceIDProperties::deviceUUID), which names a
 * host GPU across reboots and driver updates where enumeration order does not. */
#ifndef AV_DEVICE_UUID_HPP
#define AV_DEVICE_UUID_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class DeviceUuid {
public:
    static constexpr uint32_t bytes = 16u;

    DeviceUuid();

    /* The 8-4-4-4-12 hex text, either case. invalid_parameter, changing
     * nothing, for anything else. */
    [[nodiscard]] Status parse(const char *text);
    bool                 equals(const uint8_t *uuid) const;

private:
    static constexpr uint32_t text_length = 36u;

    /* The value of hex digit `c`, or -1. */
    int32_t digit(char c) const;
    bool    separator_at(uint32_t position) const;

    uint8_t bytes_[bytes];
};

}  // namespace av

#endif  // AV_DEVICE_UUID_HPP
