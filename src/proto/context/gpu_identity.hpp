/* The host GPU one device instance is bound to, as Vulkan reports it. Part of
 * the capset (docs/protocol.md), so the guest can name the adapter truthfully. */
#ifndef AV_GPU_IDENTITY_HPP
#define AV_GPU_IDENTITY_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;
class WireWriter;

class GpuIdentity {
public:
    static constexpr size_t   encoded_size = 96u;
    static constexpr size_t   uuid_bytes   = 16u;
    static constexpr size_t   name_bytes   = 48u;
    /* VkPhysicalDeviceType. */
    static constexpr uint32_t other_gpu      = 0u;
    static constexpr uint32_t integrated_gpu = 1u;
    static constexpr uint32_t discrete_gpu   = 2u;
    static constexpr uint32_t virtual_gpu    = 3u;
    static constexpr uint32_t cpu            = 4u;

    GpuIdentity();
    /* `name` is truncated to name_bytes - 1 bytes. */
    GpuIdentity(uint32_t vendor_id, uint32_t device_id, uint32_t device_type,
                const uint8_t *uuid, uint64_t local_memory, uint64_t shared_memory,
                const char *name);

    uint32_t       vendor_id() const;
    uint32_t       device_id() const;
    uint32_t       device_type() const;
    const uint8_t *uuid() const;
    uint64_t       local_memory() const;
    uint64_t       shared_memory() const;
    const char    *name() const;

    void                 encode(WireWriter &writer) const;
    /* Leaves this identity unchanged unless it returns ok. */
    [[nodiscard]] Status decode(WireReader &reader);

private:
    bool name_is_canonical() const;

    uint32_t vendor_id_;
    uint32_t device_id_;
    uint32_t device_type_;
    uint8_t  uuid_[uuid_bytes];
    uint64_t local_memory_;
    uint64_t shared_memory_;
    char     name_[name_bytes];
};

}  // namespace av

#endif  // AV_GPU_IDENTITY_HPP
