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

    /* Empty, every truncation, the valid input itself, a bit-flip sweep over
     * it, all-zero and all-ones: `length + 1 + 8 * length + 2` cases. */
    void run() const;

    std::uint32_t cases_run() const;

    /* How many of those cases the parser refused. A sweep that never measures
     * this proves only that the decoder did not crash. */
    std::uint32_t rejections() const;

private:
    void feed(const std::uint8_t *bytes, std::size_t length) const;

    Parser              parser_;
    const std::uint8_t *valid_;
    std::size_t         length_;
    mutable std::uint32_t cases_run_;
    mutable std::uint32_t rejections_;
};

}  // namespace av

#endif  // AV_HOSTILE_SWEEP_HPP
