#include "harness/framework/hostile_sweep.hpp"

#include "harness/framework/guarded_buffer.hpp"

#include <cstring>
#include <cstddef>
#include <cstdint>

namespace av {

HostileSweep::HostileSweep(Parser parser, const std::uint8_t *valid,
                           std::size_t length)
    : parser_(parser), valid_(valid), length_(length), cases_run_(0u) {}

std::uint32_t HostileSweep::cases_run() const { return cases_run_; }

void HostileSweep::feed(const std::uint8_t *bytes, std::size_t length) const
{
    cases_run_++;
    parser_(bytes, length);
}

void HostileSweep::run() const
{
    GuardedBuffer buffer(length_ != 0u ? length_ : 1u);

    for (std::size_t prefix = 0u; prefix <= length_; prefix++) {
        std::memcpy(buffer.data(), valid_, prefix);
        feed(buffer.data(), prefix);
    }

    for (std::size_t byte = 0u; byte < length_; byte++) {
        for (unsigned bit = 0u; bit < 8u; bit++) {
            std::memcpy(buffer.data(), valid_, length_);
            buffer.data()[byte] ^= static_cast<std::uint8_t>(1u << bit);
            feed(buffer.data(), length_);
        }
    }

    std::memset(buffer.data(), 0x00, length_);
    feed(buffer.data(), length_);
    std::memset(buffer.data(), 0xFF, length_);
    feed(buffer.data(), length_);
}

}  // namespace av
