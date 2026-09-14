/* virtio-pci's device side in front of the real device core: the driver's
 * status reaches the core as virtio-pci hands it on - every write to
 * set_status, and a write of 0 to reset as well - and a store to the notify
 * region names the queue the driver published to, and the bridge takes every chain
 * from its split ring in guest memory at once. A control request goes to the
 * device core first and to the base virtio-gpu device when the core leaves it,
 * as QEMU dispatches it; a cursor request completes with nothing written, as
 * QEMU's cursor queue does. A queue that completed something is left pending
 * an interrupt for InterruptPump, because a register store inside a DDI is no
 * place to run the ISR. Everything runs on the thread that stores, which is
 * the device's main loop. */
#ifndef AV_VIRTIO_BRIDGE_HPP
#define AV_VIRTIO_BRIDGE_HPP

#include "core/platform/register_window.hpp"
#include "harness/device/fake_common_config.hpp"
#include "harness/device/fake_virtqueue_device.hpp"
#include "harness/fullstack/base_virtio_gpu.hpp"

#include <cstdint>
#include <optional>

namespace av {

class LoopbackDevice;
class MockPlatform;

class VirtioBridge final : public RegisterWindow {
public:
    static constexpr std::uint16_t control_queue = 0u;
    static constexpr std::uint16_t cursor_queue = 1u;
    static constexpr std::uint16_t served_queues = 2u;
    /* GpuRequestWriter::max_backing_request_size: the widest request the driver sends. */
    static constexpr std::uint32_t request_capacity = 0x9000u;

    VirtioBridge(MockPlatform &platform, FakeCommonConfig &common, LoopbackDevice &device);
    ~VirtioBridge();
    VirtioBridge(const VirtioBridge &) = delete;
    VirtioBridge &operator=(const VirtioBridge &) = delete;

    std::uint64_t length() const override;
    std::uint8_t  read_u8(std::uint64_t offset) override;
    std::uint16_t read_u16(std::uint64_t offset) override;
    std::uint32_t read_u32(std::uint64_t offset) override;
    void          write_u8(std::uint64_t offset, std::uint8_t value) override;
    void          write_u16(std::uint64_t offset, std::uint16_t value) override;
    void          write_u32(std::uint64_t offset, std::uint32_t value) override;
    void          write_u64(std::uint64_t offset, std::uint64_t value) override;

    /* Whether `queue` completed a request since the last time this was asked. */
    bool          take_completion(std::uint16_t queue);
    /* Control requests the device core answered, and those it left to the base device. */
    std::uint32_t answered_by_core() const;
    std::uint32_t answered_by_base() const;

private:
    static constexpr std::uint64_t region_bytes = 0x1000u;

    /* The device side of `queue` where the driver last placed its rings. */
    FakeVirtqueueDevice &view(std::uint16_t queue);
    void                 serve(std::uint16_t queue);
    std::uint32_t        answer(const std::uint8_t *request, std::uint32_t length, std::uint8_t *answer,
                                std::uint32_t room);
    void                 unexpected(const char *access, std::uint64_t offset);
    void                 follow_status(std::uint8_t status);

    MockPlatform                       &platform_;
    FakeCommonConfig                   &common_;
    LoopbackDevice                     &device_;
    BaseVirtioGpu                       base_;
    std::optional<FakeVirtqueueDevice>  views_[served_queues];
    FakeCommonConfig::Queue             placed_[served_queues];
    bool                                completed_[served_queues];
    std::uint32_t                       by_core_;
    std::uint8_t                        request_[request_capacity];
};

}  // namespace av

#endif  // AV_VIRTIO_BRIDGE_HPP
