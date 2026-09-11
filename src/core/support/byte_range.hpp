/* A [base, base + size) span that knows whether it is representable.
 *
 * Every overflow check in the address-space code lives here, so no caller has
 * to remember to write one. */
#ifndef AV_BYTE_RANGE_HPP
#define AV_BYTE_RANGE_HPP

#include <stdint.h>

namespace av {

class ByteRange {
public:
    ByteRange();
    ByteRange(uint64_t base, uint64_t size);

    /* Non-empty and base + size does not wrap. */
    bool is_representable() const;

    uint64_t base() const;
    uint64_t size() const;

    /* Precondition: is_representable(). */
    uint64_t end() const;

    bool contains(uint64_t address) const;
    bool encloses(const ByteRange &inner) const;
    bool starts_below(const ByteRange &other) const;

private:
    uint64_t base_;
    uint64_t size_;
};

}  // namespace av

#endif  // AV_BYTE_RANGE_HPP
