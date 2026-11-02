#ifndef AV_VIRTIO_CAPABILITY_WALK_HPP
#define AV_VIRTIO_CAPABILITY_WALK_HPP

#include "core/support/status.hpp"
#include "miniport/virtio/virtio_pci_layout.hpp"
#include "miniport/virtio/virtio_region.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class VirtioCapabilityWalk {
public:
    static constexpr size_t config_space_bytes = 256u;

    VirtioCapabilityWalk(const uint8_t *config_space, size_t length);

    [[nodiscard]] Status walk(VirtioPciLayout &layout) const;

private:
    static constexpr uint8_t  capability_pointer = 0x34u;
    static constexpr uint8_t  status_register    = 0x06u;
    static constexpr uint8_t  has_capabilities   = 0x10u;
    static constexpr uint8_t  first_capability   = 0x40u;
    static constexpr uint8_t  vendor_capability  = 0x09u;
    static constexpr uint8_t  highest_bar        = 5u;
    static constexpr uint64_t common_bytes       = 0x38u;
    static constexpr uint64_t notify_bytes       = 2u;
    static constexpr uint64_t isr_bytes          = 1u;

    class Collector;

    Status   visit(size_t at, Collector &collector) const;
    uint32_t read_u32(size_t offset) const;

    uint8_t bytes_[config_space_bytes];
    bool    complete_;
};

}

#endif
