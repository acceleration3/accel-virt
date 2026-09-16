/* The address range of the binary a trace table belongs to. A crash handler
 * formats its own table, whose frames name functions by pointers into this
 * image; a pointer outside it - a table overwritten by the crash - is never
 * read. */
#ifndef AV_IMAGE_RANGE_HPP
#define AV_IMAGE_RANGE_HPP

#include <stdint.h>

namespace av {

struct ImageRangeProbe;

class ImageRange {
public:
    ImageRange(uint64_t base, uint64_t size);

    /* Whether `bytes` bytes at `address` lie inside, with no wrap. */
    bool     contains(uint64_t address, uint64_t bytes) const;
    /* The offset of an address inside; the address itself outside. */
    uint64_t offset_of(uint64_t address) const;
    uint64_t end() const;

    bool invariants_hold() const;

private:
    uint64_t base_;
    uint64_t size_;

    friend struct ImageRangeProbe;
};

}  // namespace av

#endif  // AV_IMAGE_RANGE_HPP
