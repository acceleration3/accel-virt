#ifndef AV_FAKE_VIRTQUEUE_DEVICE_HPP
#define AV_FAKE_VIRTQUEUE_DEVICE_HPP

#include "harness/platform/mock_platform.hpp"

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

    std::uint16_t complete_all(std::uint32_t written);
    std::uint16_t respond_all(std::uint32_t type);
    std::uint16_t respond_all(const std::uint8_t *answer, std::uint32_t length);
    using Answerer = std::uint32_t (*)(void *context, const std::uint8_t *request, std::uint32_t length,
                                       std::uint8_t *answer, std::uint32_t room);
    std::uint16_t answer_all(std::uint8_t *request, std::uint32_t capacity, Answerer answerer, void *context);
    static constexpr std::uint32_t max_requests = 64u;
    static constexpr std::uint32_t request_bytes = 128u;
    std::uint32_t        pending_type() const;
    std::uint32_t        requests() const;
    const std::uint8_t  *request(std::uint32_t index) const;
    std::uint32_t        request_type(std::uint32_t index) const;

    std::uint16_t available_index() const;
    const Seen   &seen(std::uint32_t index) const;
    std::uint32_t seen_count() const;
    std::uint16_t head(std::uint16_t available_position) const;

    void write_used(std::uint16_t position, std::uint32_t id, std::uint32_t length);
    void set_used_index(std::uint16_t index);

private:
    void          walk(std::uint16_t chain, bool keep_request, std::uint64_t &writable, std::uint32_t &writable_length);
    void          walk_chain(std::uint16_t chain, bool keep_request, std::uint8_t *joined, std::uint32_t capacity,
                             std::uint32_t &joined_length, std::uint64_t &writable, std::uint32_t &writable_length);
    void          complete(std::uint16_t chain, std::uint32_t written);
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

}

#endif
