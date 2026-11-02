#ifndef AV_IMAGE_RANGE_HPP
#define AV_IMAGE_RANGE_HPP

#include <stdint.h>

namespace av {

struct ImageRangeProbe;

class ImageRange {
public:
    ImageRange(uint64_t base, uint64_t size);

    bool     contains(uint64_t address, uint64_t bytes) const;
    uint64_t offset_of(uint64_t address) const;
    uint64_t end() const;

    bool invariants_hold() const;

private:
    uint64_t base_;
    uint64_t size_;

    friend struct ImageRangeProbe;
};

}

#endif
