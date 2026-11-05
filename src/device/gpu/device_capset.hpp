#ifndef AV_DEVICE_CAPSET_HPP
#define AV_DEVICE_CAPSET_HPP

#include "proto/context/capset_info.hpp"

#include <stdint.h>

namespace av {

class Host;

class DeviceCapset {
public:
    static constexpr uint32_t capset_version = 2u;

    explicit DeviceCapset(Host &host);
    DeviceCapset(const DeviceCapset &) = delete;
    DeviceCapset &operator=(const DeviceCapset &) = delete;

    void              describe(uint32_t gpu_index, uint32_t sub_apis, uint64_t features);
    void              resolve_display(bool takes_dmabuf);
    bool              display_resolved() const;
    bool              described() const;
    const CapsetInfo &info() const;

private:
    Host      &host_;
    CapsetInfo info_;
    bool       described_;
    bool       display_resolved_;
};

}

#endif
