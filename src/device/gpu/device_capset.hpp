#ifndef AV_DEVICE_CAPSET_HPP
#define AV_DEVICE_CAPSET_HPP

#include "proto/context/capset_info.hpp"

#include <stdint.h>

namespace av {

class Host;

class DeviceCapset {
public:
    static constexpr uint32_t capset_version = 2u;

    static constexpr uint32_t hybrid_off = 0u;
    static constexpr uint32_t hybrid_integrated = 1u;
    static constexpr uint32_t hybrid_discrete = 2u;

    explicit DeviceCapset(Host &host);
    DeviceCapset(const DeviceCapset &) = delete;
    DeviceCapset &operator=(const DeviceCapset &) = delete;

    void              reserve_vram(uint32_t mib);
    void              reserve_host_ram(uint32_t mib);
    void              describe(uint32_t gpu_index, uint32_t sub_apis, uint64_t features);
    void              resolve_display(bool takes_dmabuf);
    bool              display_resolved() const;
    void              announce_hybrid(uint32_t role);
    bool              described() const;
    const CapsetInfo &info() const;

private:
    uint64_t   host_total();

    Host      &host_;
    CapsetInfo info_;
    uint32_t   vram_reserve_mib_;
    uint32_t   host_ram_reserve_mib_;
    bool       described_;
    bool       display_resolved_;
};

}

#endif
