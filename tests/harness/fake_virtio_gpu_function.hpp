/* QEMU's virtio-gpu-pci as the PCI function dxgkrnl enumerates: the captured
 * configuration space with BAR 4 assigned, the translated resources firmware
 * would hand out, and the fake register regions behind BAR 4 (common config at
 * 0x0, ISR at 0x1000, device config at 0x2000, notify at 0x3000, multiplier 4). */
#ifndef AV_FAKE_VIRTIO_GPU_FUNCTION_HPP
#define AV_FAKE_VIRTIO_GPU_FUNCTION_HPP

#include "fake_common_config.hpp"
#include "fake_isr_register.hpp"
#include "fake_register_bytes.hpp"
#include "fake_resource_list.hpp"

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

private:
    void rebuild_resources();

    std::uint8_t      config_[config_bytes];
    FakeResourceList  resources_;
    std::uint32_t     message_interrupts_;
    std::uint32_t     virtio_bar_length_;
    bool              virtio_bar_present_;
    FakeCommonConfig  common_;
    FakeIsrRegister   isr_;
    FakeRegisterBytes device_config_;
    FakeRegisterBytes notify_;
};

}  // namespace av

#endif  // AV_FAKE_VIRTIO_GPU_FUNCTION_HPP
