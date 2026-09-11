/* The standard garbage-data sweep, so "feed it hostile input" is one object at
 * the call site rather than boilerplate per test.
 *
 * Every case asserts the same contract: an error is returned, nothing outside
 * the destination is written, the call terminates, and ASan and UBSan stay
 * quiet. Never an assert - a malformed input is a reachable state. */
#ifndef AV_HOSTILE_SWEEP_HPP
#define AV_HOSTILE_SWEEP_HPP

#include <cstddef>
#include <cstdint>

namespace av {

class HostileSweep {
public:
    /* Consumes `length` bytes at `bytes`; returns true if it rejected them. */
    using Parser = bool (*)(const std::uint8_t *bytes, std::size_t length);

    HostileSweep(Parser parser, const std::uint8_t *valid, std::size_t length);

    /* Empty, every truncation, oversized length, all-zero, all-ones, and a
     * bit-flip sweep over the valid input. */
    void run() const;

    std::uint32_t cases_run() const;

private:
    void feed(const std::uint8_t *bytes, std::size_t length) const;

    Parser              parser_;
    const std::uint8_t *valid_;
    std::size_t         length_;
    mutable std::uint32_t cases_run_;
};

}  // namespace av

#endif  // AV_HOSTILE_SWEEP_HPP
