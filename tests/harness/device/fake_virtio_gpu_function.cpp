#include "harness/device/fake_virtio_gpu_function.hpp"

#include "harness/framework/fixture_file.hpp"

#include <cstring>

namespace av {

FakeVirtioGpuFunction::FakeVirtioGpuFunction()
    : config_{}, resources_(), message_interrupts_(3u), virtio_bar_length_(virtio_bar_length),
      virtio_bar_present_(true), common_(), isr_(), device_config_(0x1000u), notify_(0x1000u)
{
    const FixtureFile fixture("virtio_gpu_pci_config.bin");
    std::memcpy(config_, fixture.bytes(), config_bytes);
    const std::uint32_t low = static_cast<std::uint32_t>(virtio_bar_address) | 0xCu;  /* 64-bit, prefetchable */
    const std::uint32_t high = static_cast<std::uint32_t>(virtio_bar_address >> 32u);
    std::memcpy(config_ + 0x10u + 4u * virtio_bar, &low, 4u);
    std::memcpy(config_ + 0x14u + 4u * virtio_bar, &high, 4u);
    const std::uint32_t msix = static_cast<std::uint32_t>(msix_bar_address);
    std::memcpy(config_ + 0x14u, &msix, 4u);

    common_.offer_features(feature_version_1 | feature_edid | feature_resource_blob | feature_context_init);
    common_.set_queues(2u, 256u);
    device_config_.set_u32(8u, 1u);   /* num_scanouts */
    device_config_.set_u32(12u, 1u);  /* num_capsets */
    rebuild_resources();
}

void FakeVirtioGpuFunction::rebuild_resources()
{
    resources_ = FakeResourceList();
    resources_.add_memory(msix_bar_address, 0x1000u);
    if (virtio_bar_present_) resources_.add_memory(virtio_bar_address, virtio_bar_length_);
    for (std::uint32_t i = 0u; i < message_interrupts_; i++) resources_.add_message_interrupt();
}

void FakeVirtioGpuFunction::set_message_interrupts(std::uint32_t count)
{
    message_interrupts_ = count;
    rebuild_resources();
}

void FakeVirtioGpuFunction::set_virtio_bar_length(std::uint32_t length)
{
    virtio_bar_length_ = length;
    rebuild_resources();
}

void FakeVirtioGpuFunction::remove_virtio_bar_resource()
{
    virtio_bar_present_ = false;
    rebuild_resources();
}

std::uint8_t *FakeVirtioGpuFunction::config_space() { return config_; }

const FakeResourceList &FakeVirtioGpuFunction::resources() const { return resources_; }

RegisterWindow *FakeVirtioGpuFunction::region(std::uint8_t bar, std::uint64_t offset)
{
    if (bar != virtio_bar) return nullptr;
    switch (offset) {
    case 0x0000u: return &common_;
    case 0x1000u: return &isr_;
    case 0x2000u: return &device_config_;
    case 0x3000u: return &notify_;
    default:      return nullptr;
    }
}

int FakeVirtioGpuFunction::bar_at(std::uint64_t address) const
{
    if (address == virtio_bar_address) return virtio_bar;
    if (address == msix_bar_address) return 1;
    return -1;
}

FakeCommonConfig  &FakeVirtioGpuFunction::common() { return common_; }
FakeIsrRegister   &FakeVirtioGpuFunction::isr() { return isr_; }
FakeRegisterBytes &FakeVirtioGpuFunction::device_config() { return device_config_; }
FakeRegisterBytes &FakeVirtioGpuFunction::notify() { return notify_; }

}  // namespace av
