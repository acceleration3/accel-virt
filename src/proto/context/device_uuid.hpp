/* A Vulkan device UUID (VkPhysicalDeviceIDProperties::deviceUUID), which names a
 * host GPU across reboots and driver updates where enumeration order does not.
 * Every host API reports the same sixteen bytes for a GPU - OpenCL's
 * CL_DEVICE_UUID_KHR, OpenGL's GL_DEVICE_UUID_EXT, DXVK's interop through
 * Vulkan - and each finds the GPU a context runs on by it. The guest names
 * the GPU it was given by it too (GpuIdentity), so it crosses the wire. */
#ifndef AV_DEVICE_UUID_HPP
#define AV_DEVICE_UUID_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class TextBuffer;
class WireReader;
class WireWriter;

class DeviceUuid {
public:
    static constexpr uint32_t bytes = 16u;

    DeviceUuid();
    /* The sixteen bytes at `uuid`, as a host API reported them; all zero for
     * null. */
    explicit DeviceUuid(const uint8_t *uuid);

    /* The 8-4-4-4-12 hex text, either case. invalid_parameter, changing
     * nothing, for anything else. */
    [[nodiscard]] Status parse(const char *text);
    /* False for null. */
    bool                 equals(const uint8_t *uuid) const;
    bool                 equals(const DeviceUuid &other) const;
    /* Its thirty-two hex digits, for a state dump. */
    void                 append_hex(TextBuffer &text) const;
    /* The sixteen bytes, for an API that takes them as such. */
    const uint8_t       *raw() const;
    /* The sixteen bytes on the wire as they are; a short read latches in
     * `reader`, which its caller checks once. */
    void                 encode(WireWriter &writer) const;
    void                 decode(WireReader &reader);

private:
    static constexpr uint32_t text_length = 36u;

    /* The value of hex digit `c`, or -1. */
    int32_t digit(char c) const;
    bool    separator_at(uint32_t position) const;

    uint8_t bytes_[bytes];
};

}  // namespace av

#endif  // AV_DEVICE_UUID_HPP
