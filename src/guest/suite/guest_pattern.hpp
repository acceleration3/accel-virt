/* The bytes a guest writes into memory someone else will read (features 041
 * and 045): the byte at offset n is 1 + (n * 7) % 251, so no byte is zero and
 * no page of it repeats another, and a reader can tell what was written from
 * what the memory held before. Views are volatile, so every read is of the
 * memory and not a value the compiler knows was just stored there. */
#ifndef AV_GUEST_PATTERN_HPP
#define AV_GUEST_PATTERN_HPP

#include <stdint.h>

namespace av {

class GuestPattern {
public:
    explicit GuestPattern(uint32_t bytes);

    uint32_t bytes() const;
    void     write(volatile unsigned char *view) const;
    void     clear(volatile unsigned char *view) const;
    /* How many of the pattern's bytes the view holds where they belong. */
    uint32_t matched(const volatile unsigned char *view) const;
    /* How many bytes of the pattern's length are not zero. */
    uint32_t filled(const volatile unsigned char *view) const;

private:
    unsigned char at(uint32_t offset) const;

    uint32_t bytes_;
};

}  // namespace av

#endif  // AV_GUEST_PATTERN_HPP
