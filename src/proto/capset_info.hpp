/* What one device instance - one host GPU - offers the guest: the rvk capset
 * (id 0x72, docs/protocol.md). */
#ifndef AV_CAPSET_INFO_HPP
#define AV_CAPSET_INFO_HPP

#include "../core/status.hpp"
#include "gpu_identity.hpp"
#include "protocol_version.hpp"
#include "sub_api.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class CapsetInfo {
public:
    static constexpr uint32_t capset_id    = 0x72u;
    static constexpr size_t   encoded_size = 128u;
    static constexpr uint64_t feature_host_visible_memory = 1u;

    CapsetInfo();
    CapsetInfo(const ProtocolVersion &version, uint32_t sub_apis, uint32_t max_contexts,
               uint8_t min_ring_order, uint8_t max_ring_order, uint64_t features,
               const GpuIdentity &gpu);

    const ProtocolVersion &version() const;
    uint32_t               max_contexts() const;
    const GpuIdentity     &gpu() const;
    bool                   offers(SubApi api) const;
    bool                   accepts_ring_order(uint8_t order) const;
    /* Unknown bits are kept, so a newer device's capabilities round-trip. */
    bool                   has_feature(uint64_t feature) const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* Leaves this capset unchanged unless it returns ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic           = 0x314B5652u;  /* "RVK1" */
    static constexpr uint32_t known_sub_apis  = 0x7u;
    static constexpr uint8_t  lowest_order    = 12u;
    static constexpr uint8_t  highest_order   = 24u;

    bool fields_valid() const;

    ProtocolVersion version_;
    uint32_t        sub_apis_;
    uint32_t        max_contexts_;
    uint8_t         min_ring_order_;
    uint8_t         max_ring_order_;
    uint64_t        features_;
    GpuIdentity     gpu_;
};

}  // namespace av

#endif  // AV_CAPSET_INFO_HPP
