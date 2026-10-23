#ifndef AV_MIP_EXTENT_HPP
#define AV_MIP_EXTENT_HPP

#include <stdint.h>

namespace av {

class MipExtent {
public:
    MipExtent(uint64_t width, uint32_t height, uint32_t depth, uint32_t level);

    uint64_t width() const;
    uint32_t height() const;
    uint32_t depth() const;

private:
    uint64_t at_level(uint64_t edge) const;

    uint64_t width_;
    uint32_t height_;
    uint32_t depth_;
    uint32_t level_;
};

}  // namespace av

#endif  // AV_MIP_EXTENT_HPP
