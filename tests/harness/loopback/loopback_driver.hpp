/* The driver's half of the loopback suite: carries a guest core's control
 * requests to a LoopbackDevice and its answers back, at the IRQL the driver
 * would, and serves a context on its fence interrupts. An answer can be cut
 * short or have a byte rewritten on the way, a device that lies. */
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

    /* Runs the query to its end: the status of the last answer accepted. */
    Status        query(CapsetQuery &query);
    /* Sends every request the context has, each answered by the device: how many. */
    std::uint32_t converse(GuestContext &context);
    /* Serves the context on each interrupt of `line` until `fence` completes:
     * false once it faults, fails, or the device goes quiet. */
    bool          serve_until(GuestContext &context, std::uint32_t line, std::uint64_t fence);
    /* The Nth answer from now (1-based): byte `offset` becomes `value`. */
    void          tamper_answer(std::uint32_t nth, std::size_t offset, std::uint8_t value);
    /* The Nth answer from now (1-based) cut to `length` bytes. */
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

}  // namespace av

#endif  // AV_LOOPBACK_DRIVER_HPP
