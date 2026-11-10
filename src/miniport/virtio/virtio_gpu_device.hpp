#ifndef AV_VIRTIO_GPU_DEVICE_HPP
#define AV_VIRTIO_GPU_DEVICE_HPP

#include "miniport/pci/mapped_register_window.hpp"
#include "core/platform/platform.hpp"
#include "core/platform/wait_mutex.hpp"
#include "core/support/status.hpp"
#include "miniport/virtio/gpu_command_queue.hpp"
#include "miniport/virtio/virtio_pci_transport.hpp"
#include "proto/context/context_capacity.hpp"

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

    VirtioGpuDevice(Platform &platform, const BarMappings &mappings, const VirtioPciLayout &layout);
    ~VirtioGpuDevice();
    VirtioGpuDevice(const VirtioGpuDevice &) = delete;
    VirtioGpuDevice &operator=(const VirtioGpuDevice &) = delete;

    [[nodiscard]] Status start(uint32_t message_interrupts);
    [[nodiscard]] Status stop();
    void                 reset_immediately();

    [[nodiscard]] Status connect_fence_queue(uint32_t doorbell, uint32_t &message);
    uint8_t              read_isr();
    [[nodiscard]] Status send_control(const uint8_t *request, size_t length);
    [[nodiscard]] Status send_control_carrying(const uint8_t *request, size_t length, const VirtqSegment &payload);
    [[nodiscard]] Status ask_control(const uint8_t *request, size_t length, uint8_t *response, size_t capacity,
                                     uint32_t &written);
    [[nodiscard]] Status ask_control_carrying(const uint8_t *request, size_t length, const VirtqSegment &payload,
                                              uint8_t *response, size_t capacity, uint32_t &written);
    [[nodiscard]] Status send_cursor(const uint8_t *request, size_t length);
    [[nodiscard]] Status harvest_control(uint32_t &completed);
    [[nodiscard]] Status harvest_cursor(uint32_t &completed);

    uint64_t features() const;
    uint32_t scanouts() const;
    [[nodiscard]] Status acknowledge_display_event();
    bool     running() const;
    bool     invariants_hold() const;

private:
    enum : uint16_t { config_source = 0u, control_source = 1u, cursor_source = 2u, source_count = 3u };
    enum : uint16_t { control_queue = 0u, cursor_queue = 1u, first_fence_queue = 2u };
    static constexpr uint32_t doorbells = ContextCapacity::contexts;
    static constexpr uint16_t no_vector = 0xFFFFu;
    static constexpr uint64_t num_scanouts_offset = 8u;
    static constexpr uint64_t events_clear_offset = 4u;
    static constexpr uint32_t event_display = 1u;

    uint16_t vector_for(uint16_t source, uint32_t message_interrupts) const;
    uint16_t fence_vector_for(uint32_t doorbell, uint32_t message_interrupts) const;
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
    WaitMutex            asks_;
    uint32_t             scanouts_;
    uint32_t             message_interrupts_;
    bool                 running_;

    friend struct VirtioGpuDeviceProbe;
};

}

#endif
