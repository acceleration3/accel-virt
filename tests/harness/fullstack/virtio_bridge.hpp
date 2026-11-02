#ifndef AV_VIRTIO_BRIDGE_HPP
#define AV_VIRTIO_BRIDGE_HPP

#include "core/platform/register_window.hpp"
#include "harness/virtio/fake_common_config.hpp"
#include "harness/virtio/fake_virtqueue_device.hpp"
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

    bool          take_completion(std::uint16_t queue);
    std::uint32_t answered_by_core() const;
    std::uint32_t answered_by_base() const;

private:
    static constexpr std::uint64_t region_bytes = 0x1000u;

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

}

#endif
