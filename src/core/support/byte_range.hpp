#ifndef AV_BYTE_RANGE_HPP
#define AV_BYTE_RANGE_HPP

#include <stdint.h>

namespace av {

class ByteRange {
public:
    ByteRange();
    ByteRange(uint64_t base, uint64_t size);

    bool is_representable() const;

    uint64_t base() const;
    uint64_t size() const;

    uint64_t end() const;

    bool contains(uint64_t address) const;
    bool encloses(const ByteRange &inner) const;

private:
    uint64_t base_;
    uint64_t size_;
};

}

#endif
