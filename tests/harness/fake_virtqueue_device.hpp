/* The device side of one split virtqueue (virtio 1.2 2.7), reading the rings
 * through MockPlatform's physical-address translation exactly as QEMU reads
 * guest memory. It completes what the driver published, records the segments
 * it saw, and can write any used element or index a hostile device would. */
#ifndef AV_FAKE_VIRTQUEUE_DEVICE_HPP
#define AV_FAKE_VIRTQUEUE_DEVICE_HPP

#include "mock_platform.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class FakeVirtqueueDevice {
public:
    static constexpr std::uint32_t max_seen = 64u;

    struct Seen {
        std::uint64_t address;
        std::uint32_t length;
        std::uint16_t flags;
    };

    FakeVirtqueueDevice(MockPlatform &platform, std::uint64_t descriptors, std::uint64_t driver_area,
                        std::uint64_t device_area, std::uint16_t size);

    /* Completes every published chain, reporting `written` bytes for each.
     * Returns the number completed. */
    std::uint16_t complete_all(std::uint32_t written);
    /* Completes every published chain as virtio-gpu does: keeps the first
     * bytes of what the driver sent, writes a response header of `type` into
     * the chain's writable buffer, and reports that as written (nothing
     * written for a chain without one). */
    std::uint16_t respond_all(std::uint32_t type);
    static constexpr std::uint32_t max_requests = 64u;
    static constexpr std::uint32_t request_bytes = 64u;
    std::uint32_t        requests() const;
    const std::uint8_t  *request(std::uint32_t index) const;
    std::uint32_t        request_type(std::uint32_t index) const;

    std::uint16_t available_index() const;
    const Seen   &seen(std::uint32_t index) const;
    std::uint32_t seen_count() const;
    std::uint16_t head(std::uint16_t available_position) const;

    /* Hostile writes. */
    void write_used(std::uint16_t position, std::uint32_t id, std::uint32_t length);
    void set_used_index(std::uint16_t index);

private:
    /* Walks one chain, recording each segment (and keeping the first bytes the
     * driver sent, if asked); answers its writable buffer's address and length. */
    void          walk(std::uint16_t chain, bool keep_request, std::uint64_t &writable, std::uint32_t &writable_length);
    std::uint8_t *at(std::uint64_t address, std::size_t bytes) const;
    std::uint64_t read(std::uint64_t address, unsigned width) const;
    void          write(std::uint64_t address, std::uint64_t value, unsigned width);

    MockPlatform &platform_;
    std::uint64_t descriptors_;
    std::uint64_t driver_area_;
    std::uint64_t device_area_;
    std::uint16_t size_;
    std::uint16_t last_available_;
    std::uint16_t used_index_;
    Seen          seen_[max_seen];
    std::uint32_t seen_count_;
    std::uint8_t  requests_[max_requests][request_bytes];
    std::uint32_t request_count_;
};

}  // namespace av

#endif  // AV_FAKE_VIRTQUEUE_DEVICE_HPP
