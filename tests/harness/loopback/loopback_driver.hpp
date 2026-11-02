#ifndef AV_LOOPBACK_DRIVER_HPP
#define AV_LOOPBACK_DRIVER_HPP

#include "core/support/status.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class CapsetQuery;
class GuestContext;
class LoopbackDevice;
class MockPlatform;

class LoopbackDriver {
public:
    static constexpr std::size_t answer_capacity = 256u;

    LoopbackDriver(MockPlatform &platform, LoopbackDevice &device);

    Status        query(CapsetQuery &query);
    std::uint32_t converse(GuestContext &context);
    bool          serve_until(GuestContext &context, std::uint32_t line, std::uint64_t fence);
    void          tamper_answer(std::uint32_t nth, std::size_t offset, std::uint8_t value);
    void          truncate_answer(std::uint32_t nth, std::size_t length);

private:
    std::size_t exchange(const std::uint8_t *request, std::size_t length, std::uint8_t *answer);

    MockPlatform   &platform_;
    LoopbackDevice &device_;
    std::uint32_t   tamper_at_;
    std::size_t     tamper_offset_;
    std::uint8_t    tamper_value_;
    std::uint32_t   truncate_at_;
    std::size_t     truncate_length_;
};

}

#endif
