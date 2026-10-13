/* What one device instance - one host GPU - offers the guest: the av capset
 * (id 0x72, docs/protocol.md). */
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
    /* Feature 039's probes, each set only where its probe passed on this host:
     * memory the host GPU exports that reaches the guest through the hostmem
     * region, and guest pages the host GPU addresses directly. */
    static constexpr uint64_t feature_gpu_exported_memory = 2u;
    static constexpr uint64_t feature_guest_memory_import = 4u;
    /* The host's display can show a dma-buf, so the guest may scan a blob out
     * instead of transferring it (feature 058). Decided when the guest first
     * reads the capset, not at realize: QEMU registers its display backends
     * after every device is realized. */
    static constexpr uint64_t feature_dmabuf_scanout = 8u;
    /* The device moves queued presents on a thread of its own and says which
     * it has moved through a page (docs/protocol.md, "Present engine"). */
    static constexpr uint64_t feature_present_engine = 16u;
    /* A queued present's source may be a host image, read by its key. */
    static constexpr uint64_t feature_present_from_image = 32u;
    /* The fence page counts out-of-band raises in `raised`, apart from
     * progress (docs/protocol.md, "Fence page"), so the driver can wake a
     * process waiting for one rather than have it ask. */
    static constexpr uint64_t feature_raises = 64u;
    /* The device composes a head's overlay planes (SCANOUT_PLANE, docs/
     * protocol.md "Scanout planes"), so the guest may offer Windows more than
     * the one plane it shows. */
    static constexpr uint64_t feature_scanout_planes = 128u;

    CapsetInfo();
    CapsetInfo(const ProtocolVersion &version, uint32_t sub_apis, uint32_t max_contexts,
               uint8_t min_ring_order, uint8_t max_ring_order, uint64_t features,
               const GpuIdentity &gpu);

    const ProtocolVersion &version() const;
    uint32_t               max_contexts() const;
    const GpuIdentity     &gpu() const;
    bool                   offers(SubApi api) const;
    /* Drops a feature this device turned out not to have after all: the
     * executor is asked what it can import only once a library is loaded, and
     * a bit already set from the host's own probe would otherwise promise what
     * the library cannot do (feature 042). */
    void                   withdraw(uint64_t feature);
    /* Adds a feature that could not be decided when the capset was described,
     * which is what feature_dmabuf_scanout is. */
    void                   announce(uint64_t feature);
    bool                   accepts_ring_order(uint8_t order) const;
    /* Unknown bits are kept, so a newer device's capabilities round-trip. */
    bool                   has_feature(uint64_t feature) const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* encoded_size bytes; a writer short of them latches its failure. */
    void                 encode(WireWriter &writer) const;
    /* Leaves this capset unchanged unless it returns ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic           = 0x00315641u;  /* "AV1" */
    static constexpr uint32_t known_sub_apis  = 0xFu;
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
