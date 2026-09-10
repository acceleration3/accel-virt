/* A power-of-two alignment, validated once at construction instead of at every
 * use, and able to round up without wrapping. */
#ifndef AV_ALIGNMENT_HPP
#define AV_ALIGNMENT_HPP

#include <stdint.h>

namespace av {

class Alignment {
public:
    explicit Alignment(uint64_t value);

    bool          is_valid() const;
    uint64_t value() const;

    /* Rounds `address` up to this alignment. Returns false on overflow rather
     * than wrapping, which is the whole reason this is not an expression at
     * the call site. */
    bool align_up(uint64_t address, uint64_t &result) const;

    bool divides(uint64_t value) const;

private:
    uint64_t value_;
};

}  // namespace av

#endif  // AV_ALIGNMENT_HPP
