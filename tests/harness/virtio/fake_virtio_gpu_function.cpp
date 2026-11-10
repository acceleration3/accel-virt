#include "harness/virtio/fake_virtio_gpu_function.hpp"

#include "harness/framework/fixture_file.hpp"

#include <cstring>

namespace av {

FakeVirtioGpuFunction::FakeVirtioGpuFunction()
    : config_{}, resources_(), message_interrupts_(3u), virtio_bar_length_(virtio_bar_length),
      virtio_bar_present_(true), common_(), isr_(), device_config_(0x1000u), notify_(0x1000u), doorbells_(),
      notify_window_(&notify_), doorbell_window_(&doorbells_), doorbell_region_(false), next_capability_(first_added_capability)
{
    const FixtureFile fixture("virtio_gpu_pci_config.bin");
    std::memcpy(config_, fixture.bytes(), config_bytes);
    const std::uint32_t low = static_cast<std::uint32_t>(virtio_bar_address) | 0xCu;
    const std::uint32_t high = static_cast<std::uint32_t>(virtio_bar_address >> 32u);
    std::memcpy(config_ + 0x10u + 4u * virtio_bar, &low, 4u);
    std::memcpy(config_ + 0x14u + 4u * virtio_bar, &high, 4u);
    const std::uint32_t msix = static_cast<std::uint32_t>(msix_bar_address);
    std::memcpy(config_ + 0x14u, &msix, 4u);

    common_.offer_features(feature_version_1 | feature_edid | feature_resource_blob | feature_context_init);
    common_.set_queues(2u, 256u);
    device_config_.set_u32(8u, 1u);
    device_config_.set_u32(12u, 1u);
    rebuild_resources();
}

void FakeVirtioGpuFunction::add_shared_memory_region(std::uint8_t id, std::uint8_t bar, std::uint64_t offset,
                                                     std::uint64_t length, std::uint64_t bar_address)
{
    constexpr std::uint8_t vendor_capability = 0x09u;
    constexpr std::uint8_t shared_memory_type = 8u;
    constexpr std::uint8_t capability_bytes = 24u;
    const std::uint8_t at = next_capability_;
    next_capability_ = static_cast<std::uint8_t>(at + capability_bytes);
    config_[at] = vendor_capability;
    config_[at + 1u] = config_[0x34u];
    config_[at + 2u] = capability_bytes;
    config_[at + 3u] = shared_memory_type;
    config_[at + 4u] = bar;
    config_[at + 5u] = id;
    const std::uint32_t offset_low = static_cast<std::uint32_t>(offset);
    const std::uint32_t length_low = static_cast<std::uint32_t>(length);
    const std::uint32_t offset_high = static_cast<std::uint32_t>(offset >> 32u);
    const std::uint32_t length_high = static_cast<std::uint32_t>(length >> 32u);
    std::memcpy(config_ + at + 8u, &offset_low, 4u);
    std::memcpy(config_ + at + 12u, &length_low, 4u);
    std::memcpy(config_ + at + 16u, &offset_high, 4u);
    std::memcpy(config_ + at + 20u, &length_high, 4u);
    config_[0x34u] = at;
    const std::uint32_t low = static_cast<std::uint32_t>(bar_address) | 0xCu;
    const std::uint32_t high = static_cast<std::uint32_t>(bar_address >> 32u);
    std::memcpy(config_ + 0x10u + 4u * bar, &low, 4u);
    std::memcpy(config_ + 0x14u + 4u * bar, &high, 4u);
    resources_.add_memory(bar_address, static_cast<std::uint32_t>(length));
}

void FakeVirtioGpuFunction::add_doorbell_region()
{
    constexpr std::uint64_t length = FakeDoorbellPages::pages * FakeDoorbellPages::page_bytes;
    add_shared_memory_region(doorbell_region_id, 0u, 0u, length, doorbell_bar_address);
    const std::uint32_t low = static_cast<std::uint32_t>(doorbell_bar_address);
    std::memcpy(config_ + 0x10u, &low, 4u);
    common_.set_queues(ContextCapacity::contexts + 3u, 256u);
    doorbell_region_ = true;
}

void FakeVirtioGpuFunction::set_subsystem_id(std::uint16_t id)
{
    constexpr std::size_t subsystem_offset = 0x2Eu;
    std::memcpy(config_ + subsystem_offset, &id, sizeof id);
}

void FakeVirtioGpuFunction::present_host_gpu(std::uint16_t vendor, std::uint16_t device, std::uint8_t revision)
{
    constexpr std::size_t revision_offset = 0x08u;
    std::memcpy(config_, &vendor, sizeof vendor);
    std::memcpy(config_ + sizeof vendor, &device, sizeof device);
    config_[revision_offset] = revision;
}

void FakeVirtioGpuFunction::serve_notify_with(RegisterWindow &window) { notify_window_ = &window; }
void FakeVirtioGpuFunction::serve_doorbells_with(RegisterWindow &window) { doorbell_window_ = &window; }

void FakeVirtioGpuFunction::rebuild_resources()
{
    resources_ = FakeResourceList();
    resources_.add_memory(msix_bar_address, 0x1000u);
    if (virtio_bar_present_) resources_.add_memory(virtio_bar_address, virtio_bar_length_);
    if (doorbell_region_)
        resources_.add_memory(doorbell_bar_address, FakeDoorbellPages::pages * FakeDoorbellPages::page_bytes);
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

void FakeVirtioGpuFunction::set_device_config_length(std::uint32_t length)
{
    constexpr std::uint8_t device_config_type = 4u;
    constexpr std::size_t  capability_pointer = 0x34u;
    for (std::uint8_t at = config_[capability_pointer]; at != 0u; at = config_[at + 1u])
        if (config_[at] == 0x09u && config_[at + 3u] == device_config_type)
            std::memcpy(config_ + at + 12u, &length, sizeof length);
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
    if (bar == 0u && doorbell_region_) return offset == 0u ? doorbell_window_ : nullptr;
    if (bar != virtio_bar) return nullptr;
    switch (offset) {
    case 0x0000u: return &common_;
    case 0x1000u: return &isr_;
    case 0x2000u: return &device_config_;
    case 0x3000u: return notify_window_;
    default:      return nullptr;
    }
}

int FakeVirtioGpuFunction::bar_at(std::uint64_t address) const
{
    if (address == virtio_bar_address) return virtio_bar;
    if (address == msix_bar_address) return 1;
    if (address == doorbell_bar_address && doorbell_region_) return 0;
    return -1;
}

FakeCommonConfig  &FakeVirtioGpuFunction::common() { return common_; }
FakeIsrRegister   &FakeVirtioGpuFunction::isr() { return isr_; }
FakeRegisterBytes &FakeVirtioGpuFunction::device_config() { return device_config_; }
FakeRegisterBytes &FakeVirtioGpuFunction::notify() { return notify_; }
FakeDoorbellPages &FakeVirtioGpuFunction::doorbells() { return doorbells_; }

}
