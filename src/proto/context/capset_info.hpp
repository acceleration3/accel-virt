#ifndef AV_CAPSET_INFO_HPP
#define AV_CAPSET_INFO_HPP

#include "core/support/status.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireWriter;

class CapsetInfo {
public:
    static constexpr uint32_t capset_id    = 0x72u;
    static constexpr size_t   encoded_size = 128u;
    static constexpr uint64_t feature_host_visible_memory = 1u;
    static constexpr uint64_t feature_dmabuf_scanout = 8u;
    static constexpr uint64_t feature_present_engine = 16u;
    static constexpr uint64_t feature_present_from_image = 32u;
    static constexpr uint64_t feature_raises = 64u;
    static constexpr uint64_t feature_scanout_planes = 128u;
    static constexpr uint64_t feature_present_between_surfaces = 256u;
    static constexpr uint64_t feature_present_from_surface = 512u;

    CapsetInfo();
    CapsetInfo(const ProtocolVersion &version, uint32_t sub_apis, uint32_t max_contexts,
               uint8_t min_ring_order, uint8_t max_ring_order, uint64_t features,
               const GpuIdentity &gpu);

    const ProtocolVersion &version() const;
    uint32_t               max_contexts() const;
    const GpuIdentity     &gpu() const;
    bool                   offers(SubApi api) const;
    void                   announce(uint64_t feature);
    uint32_t               vram_reserve_mib() const;
    void                   reserve_vram(uint32_t mib);
    uint64_t               host_ram_floor() const;
    void                   floor_host_ram(uint64_t bytes);
    bool                   accepts_ring_order(uint8_t order) const;
    bool                   has_feature(uint64_t feature) const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    void                 encode(WireWriter &writer) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic           = 0x00315641u;
    static constexpr uint32_t known_sub_apis  = 0xFu;
    static constexpr uint8_t  lowest_order    = 12u;
    static constexpr uint8_t  highest_order   = 24u;

    bool fields_valid() const;

    ProtocolVersion version_;
    uint32_t        sub_apis_;
    uint32_t        max_contexts_;
    uint8_t         min_ring_order_;
    uint8_t         max_ring_order_;
    uint32_t        vram_reserve_mib_;
    uint64_t        host_ram_floor_;
    uint64_t        features_;
    GpuIdentity     gpu_;
};

}

#endif
