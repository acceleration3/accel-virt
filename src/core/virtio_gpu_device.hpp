/* The virtio-gpu function of one adapter while it is powered: its register
 * windows, transport, control queue and cursor queue (virtio 1.2 5.7). Built
 * over BARs already mapped; start brings it to DRIVER_OK, and destruction
 * writes reset before the queues' memory is released. */
#ifndef AV_VIRTIO_GPU_DEVICE_HPP
#define AV_VIRTIO_GPU_DEVICE_HPP

#include "mapped_register_window.hpp"
#include "platform.hpp"
#include "status.hpp"
#include "virt_queue.hpp"
#include "virtio_pci_transport.hpp"

#include <stdint.h>

namespace av {

class BarMappings;
class VirtioPciLayout;
struct VirtioGpuDeviceProbe;

class VirtioGpuDevice {
public:
    static constexpr uint64_t feature_edid          = uint64_t{1} << 1u;
    static constexpr uint64_t feature_resource_blob = uint64_t{1} << 3u;
    static constexpr uint64_t feature_context_init  = uint64_t{1} << 4u;

    /* Every structure of `layout` must be mapped in `mappings`. */
    VirtioGpuDevice(Platform &platform, const BarMappings &mappings, const VirtioPciLayout &layout);
    ~VirtioGpuDevice();
    VirtioGpuDevice(const VirtioGpuDevice &) = delete;
    VirtioGpuDevice &operator=(const VirtioGpuDevice &) = delete;

    /* `message_interrupts` is what the resource list granted: none means a
     * line interrupt and no MSI-X vectors; fewer than one per source means
     * every source shares vector 0. Any failure leaves the device FAILED. */
    [[nodiscard]] Status start(uint32_t message_interrupts);
    [[nodiscard]] Status stop();
    /* DxgkDdiResetDevice: reset without waiting, at any IRQL. */
    void                 reset_immediately();

    uint8_t              read_isr();
    /* Takes every completed control request; a device that lies about one is
     * failed. */
    [[nodiscard]] Status harvest_control(uint32_t &completed);

    uint64_t features() const;
    bool     invariants_hold() const;

private:
    enum : uint16_t { config_source = 0u, control_source = 1u, cursor_source = 2u, source_count = 3u };
    enum : uint16_t { control_queue = 0u, cursor_queue = 1u };
    static constexpr uint16_t no_vector = 0xFFFFu;

    uint16_t vector_for(uint16_t source, uint32_t message_interrupts) const;

    Platform            &platform_;
    MappedRegisterWindow common_;
    MappedRegisterWindow notify_;
    MappedRegisterWindow isr_;
    MappedRegisterWindow device_;
    VirtioPciTransport   transport_;
    VirtQueue            control_;
    VirtQueue            cursor_;
    bool                 running_;

    friend struct VirtioGpuDeviceProbe;
};

}  // namespace av

#endif  // AV_VIRTIO_GPU_DEVICE_HPP
