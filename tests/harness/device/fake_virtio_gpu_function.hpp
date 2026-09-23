/* QEMU's virtio-gpu-pci as the PCI function dxgkrnl enumerates: the captured
 * configuration space with BAR 4 assigned, the translated resources firmware
 * would hand out, and the fake register regions behind BAR 4 (common config at
 * 0x0, ISR at 0x1000, device config at 0x2000, notify at 0x3000, multiplier 4). */
#ifndef AV_FAKE_VIRTIO_GPU_FUNCTION_HPP
#define AV_FAKE_VIRTIO_GPU_FUNCTION_HPP

#include "harness/device/fake_common_config.hpp"
#include "harness/device/fake_doorbell_pages.hpp"
#include "harness/device/fake_isr_register.hpp"
#include "harness/device/fake_register_bytes.hpp"
#include "harness/device/fake_resource_list.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class RegisterWindow;

class FakeVirtioGpuFunction {
public:
    static constexpr std::size_t   config_bytes      = 256u;
    static constexpr std::uint8_t  virtio_bar        = 4u;
    static constexpr std::uint64_t virtio_bar_address = 0x8000000000ull;
    static constexpr std::uint32_t virtio_bar_length = 0x4000u;
    static constexpr std::uint64_t msix_bar_address  = 0xFEA00000u;
    static constexpr std::uint64_t doorbell_bar_address = 0xFE000000u;
    static constexpr std::uint8_t  doorbell_region_id = 0x72u;
    static constexpr std::uint64_t feature_edid         = 1ull << 1u;
    static constexpr std::uint64_t feature_resource_blob = 1ull << 3u;
    static constexpr std::uint64_t feature_context_init = 1ull << 4u;
    static constexpr std::uint64_t feature_version_1    = 1ull << 32u;

    FakeVirtioGpuFunction();
    FakeVirtioGpuFunction(const FakeVirtioGpuFunction &) = delete;
    FakeVirtioGpuFunction &operator=(const FakeVirtioGpuFunction &) = delete;

    /* Knobs; each rebuilds the resource list. */
    void set_message_interrupts(std::uint32_t count);
    void set_virtio_bar_length(std::uint32_t length);
    void remove_virtio_bar_resource();
    /* A virtio shared memory capability (cfg_type 8) at the head of the
     * capability list, as a device with a hostmem region presents one. The
     * region needs a resource of its own, which firmware assigns like any BAR. */
    void add_shared_memory_region(std::uint8_t id, std::uint8_t bar, std::uint64_t offset,
                                  std::uint64_t length, std::uint64_t bar_address);

    /* virtio-gpu-rvk-pci's doorbell pages: BAR 0, located by shared memory
     * capability 0x72, with the 66 virtqueues the device has. */
    void add_doorbell_region();
    /* The subsystem ID in configuration space: virtio-gpu-rvk-pci's is 0x5256,
     * the captured virtio-gpu-pci's 0x1100. */
    void set_subsystem_id(std::uint16_t id);
    /* The vendor, device and revision virtio-gpu-rvk-pci presents when its
     * host GPU has a PCI function: that GPU's, in place of 1af4:1050. */
    void present_host_gpu(std::uint16_t vendor, std::uint16_t device, std::uint8_t revision);
    /* Stores to the notify region, or to BAR 0's doorbell pages, reach `window`
     * instead of the recorders: a device that serves what the driver rings. */
    void serve_notify_with(RegisterWindow &window);
    void serve_doorbells_with(RegisterWindow &window);

    std::uint8_t          *config_space();
    const FakeResourceList &resources() const;

    /* The fake region at `offset` of `bar`, or null. */
    RegisterWindow *region(std::uint8_t bar, std::uint64_t offset);
    /* The BAR whose resource starts at `address`, or -1. */
    int             bar_at(std::uint64_t address) const;

    FakeCommonConfig  &common();
    FakeIsrRegister   &isr();
    FakeRegisterBytes &device_config();
    FakeRegisterBytes &notify();
    FakeDoorbellPages &doorbells();

private:
    void rebuild_resources();

    std::uint8_t      config_[config_bytes];
    FakeResourceList  resources_;
    /* Where a capability this harness adds goes: past everything the captured
     * configuration space already holds. */
    static constexpr std::uint8_t first_added_capability = 0xC0u;

    std::uint32_t     message_interrupts_;
    std::uint32_t     virtio_bar_length_;
    bool              virtio_bar_present_;
    FakeCommonConfig  common_;
    FakeIsrRegister   isr_;
    FakeRegisterBytes device_config_;
    FakeRegisterBytes notify_;
    FakeDoorbellPages doorbells_;
    RegisterWindow   *notify_window_;
    RegisterWindow   *doorbell_window_;
    bool              doorbell_region_;
    std::uint8_t      next_capability_;
};

}  // namespace av

#endif  // AV_FAKE_VIRTIO_GPU_FUNCTION_HPP
