#include "core/virtio/virtio_pci_layout.hpp"

namespace av {

VirtioPciLayout::VirtioPciLayout()
    : common_(), notify_(), isr_(), device_(), notify_multiplier_(0u), shared_memory_{},
      shared_memory_ids_{}, shared_memory_count_(0u) {}

VirtioPciLayout::VirtioPciLayout(const VirtioRegion &common, const VirtioRegion &notify,
                                 const VirtioRegion &isr, const VirtioRegion &device,
                                 uint32_t notify_multiplier, const VirtioRegion *shared_memory,
                                 const uint8_t *shared_memory_ids, uint8_t shared_memory_count)
    : common_(common), notify_(notify), isr_(isr), device_(device),
      notify_multiplier_(notify_multiplier), shared_memory_{}, shared_memory_ids_{},
      shared_memory_count_(shared_memory_count)
{
    for (uint8_t i = 0u; i < shared_memory_count; i++) {
        shared_memory_[i] = shared_memory[i];
        shared_memory_ids_[i] = shared_memory_ids[i];
    }
}

const VirtioRegion &VirtioPciLayout::common() const            { return common_; }
const VirtioRegion &VirtioPciLayout::notify() const            { return notify_; }
const VirtioRegion &VirtioPciLayout::isr() const               { return isr_; }
const VirtioRegion &VirtioPciLayout::device() const            { return device_; }
uint32_t            VirtioPciLayout::notify_multiplier() const { return notify_multiplier_; }

VirtioRegion VirtioPciLayout::shared_memory(uint8_t id) const
{
    for (uint8_t i = 0u; i < shared_memory_count_; i++)
        if (shared_memory_ids_[i] == id) return shared_memory_[i];
    return VirtioRegion();
}

}  // namespace av
