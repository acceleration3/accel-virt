/* A power-of-two alignment, validated once at construction instead of at every
 * use, and able to round up without wrapping. */
#ifndef AV_ALIGNMENT_HPP
#define AV_ALIGNMENT_HPP

#include <cstdint>

namespace av {

class Alignment {
public:
    explicit Alignment(std::uint64_t value);

    bool          is_valid() const;
    std::uint64_t value() const;

    /* Rounds `address` up to this alignment. Returns false on overflow rather
     * than wrapping, which is the whole reason this is not an expression at
     * the call site. */
    bool align_up(std::uint64_t address, std::uint64_t &result) const;

    bool divides(std::uint64_t value) const;

private:
    std::uint64_t value_;
};

}  // namespace av

#endif  // AV_ALIGNMENT_HPP
