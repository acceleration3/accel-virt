#ifndef AV_GPU_IDENTITY_HPP
#define AV_GPU_IDENTITY_HPP

#include "core/support/status.hpp"
#include "proto/context/device_uuid.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;
class WireWriter;

class GpuIdentity {
public:
    static constexpr size_t   encoded_size = 96u;
    static constexpr size_t   uuid_bytes   = DeviceUuid::bytes;
    static constexpr size_t   name_bytes   = 48u;
    static constexpr uint32_t other_gpu      = 0u;
    static constexpr uint32_t integrated_gpu = 1u;
    static constexpr uint32_t discrete_gpu   = 2u;
    static constexpr uint32_t virtual_gpu    = 3u;
    static constexpr uint32_t cpu            = 4u;

    GpuIdentity();
    GpuIdentity(uint32_t vendor_id, uint32_t device_id, uint32_t device_type,
                const uint8_t *uuid, uint64_t local_memory, uint64_t shared_memory,
                const char *name);

    uint32_t       vendor_id() const;
    uint32_t       device_id() const;
    uint32_t       device_type() const;
    const uint8_t *uuid() const;
    bool           same_gpu(const GpuIdentity &other) const;
    uint64_t       local_memory() const;
    uint64_t       shared_memory() const;
    const char    *name() const;

    void                 encode(WireWriter &writer) const;
    [[nodiscard]] Status decode(WireReader &reader);

private:
    bool name_is_canonical() const;

    uint32_t vendor_id_;
    uint32_t device_id_;
    uint32_t device_type_;
    DeviceUuid uuid_;
    uint64_t local_memory_;
    uint64_t shared_memory_;
    char     name_[name_bytes];
};

}

#endif
