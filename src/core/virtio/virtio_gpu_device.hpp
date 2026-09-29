/* The virtio-gpu function of one adapter while it is powered: its register
 * windows, transport, and the control and cursor queues requests are sent on
 * (virtio 1.2 5.7). Built
 * over BARs already mapped; start brings it to DRIVER_OK, and destruction
 * writes reset before the queues' memory is released. */
#ifndef AV_VIRTIO_GPU_DEVICE_HPP
#define AV_VIRTIO_GPU_DEVICE_HPP

#include "core/pci/mapped_register_window.hpp"
#include "core/platform/platform.hpp"
#include "core/platform/wait_mutex.hpp"
#include "core/support/status.hpp"
#include "core/virtio/gpu_command_queue.hpp"
#include "core/virtio/virtio_pci_transport.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BarMappings;
class VirtioPciLayout;
class VirtqSegment;
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

    /* PASSIVE, while running: gives doorbell `doorbell`'s fence queue
     * (queue 2 + doorbell) the message its interrupts arrive on, answered in
     * `message`. Past the three sources' own vectors the fence queues share
     * what was granted, round the doorbells; with fewer, every one shares
     * vector 0, as the sources do; with a line interrupt nothing is assigned
     * and the ISR's queue bit is the only signal (docs/protocol.md). */
    [[nodiscard]] Status connect_fence_queue(uint32_t doorbell, uint32_t &message);
    uint8_t              read_isr();
    /* A virtio-gpu request, not waited on: invalid_state unless running. */
    [[nodiscard]] Status send_control(const uint8_t *request, size_t length);
    /* A control request and the answer to it, waited for at PASSIVE: what the
     * driver uses before dxgkrnl has connected the interrupt. One at a time,
     * whoever asks: a second waits for the first's answer. */
    [[nodiscard]] Status ask_control(const uint8_t *request, size_t length, uint8_t *response, size_t capacity,
                                     uint32_t &written);
    /* The same, the request continuing in `payload`, memory of the caller's
     * the device reads after it (GpuCommandQueue::exchange_carrying). */
    [[nodiscard]] Status ask_control_carrying(const uint8_t *request, size_t length, const VirtqSegment &payload,
                                              uint8_t *response, size_t capacity, uint32_t &written);
    [[nodiscard]] Status send_cursor(const uint8_t *request, size_t length);
    /* Takes every completed request of the queue, logging refusals; a device
     * that lies about one is failed. */
    [[nodiscard]] Status harvest_control(uint32_t &completed);
    [[nodiscard]] Status harvest_cursor(uint32_t &completed);

    uint64_t features() const;
    /* virtio_gpu_config.num_scanouts as the device reported it at start, which
     * is how many displays it has; zero before a start and after a stop. */
    uint32_t scanouts() const;
    /* Up to DISPATCH_LEVEL: clears VIRTIO_GPU_EVENT_DISPLAY once the driver has
     * taken the configuration change it came with. */
    [[nodiscard]] Status acknowledge_display_event();
    /* Any thread, including an ISR: whether start() finished. A device that is
     * being built is not one an interrupt may harvest from. */
    bool     running() const;
    bool     invariants_hold() const;

private:
    enum : uint16_t { config_source = 0u, control_source = 1u, cursor_source = 2u, source_count = 3u };
    enum : uint16_t { control_queue = 0u, cursor_queue = 1u, first_fence_queue = 2u };
    static constexpr uint16_t no_vector = 0xFFFFu;
    static constexpr uint64_t num_scanouts_offset = 8u;   /* struct virtio_gpu_config */
    static constexpr uint64_t events_clear_offset = 4u;
    static constexpr uint32_t event_display = 1u;          /* VIRTIO_GPU_EVENT_DISPLAY */

    uint16_t vector_for(uint16_t source, uint32_t message_interrupts) const;
    uint16_t fence_vector_for(uint32_t doorbell, uint32_t message_interrupts) const;
    /* Writes every fence queue's message before DRIVER_OK. QEMU attaches a
     * queue's irqfd when its vector is set before DRIVER_OK or when the MSI-X
     * entry is unmasked; one set later, on an entry Windows already unmasked,
     * gets a route and no irqfd, and every fence interrupt on it is lost
     * (qemu hw/virtio/virtio-pci.c kvm_virtio_pci_vector_use_one, for a device
     * with no guest_notifier_mask; feature 044). A refusal is not judged here:
     * the display needs no fence queue, and connect_fence_queue refuses the
     * context that would have used it. */
    void     assign_fence_vectors(uint32_t message_interrupts);
    [[nodiscard]] Status harvest(GpuCommandQueue &queue, uint32_t &completed, const char *refusal);

    Platform            &platform_;
    MappedRegisterWindow common_;
    MappedRegisterWindow notify_;
    MappedRegisterWindow isr_;
    MappedRegisterWindow device_;
    VirtioPciTransport   transport_;
    GpuCommandQueue      control_;
    GpuCommandQueue      cursor_;
    /* A control exchange waits for its own answer, and the queue holds one
     * such wait at a time; the rest queue here instead of being refused. */
    WaitMutex            asks_;
    uint32_t             scanouts_;
    uint32_t             message_interrupts_;
    bool                 running_;

    friend struct VirtioGpuDeviceProbe;
};

}  // namespace av

#endif  // AV_VIRTIO_GPU_DEVICE_HPP
