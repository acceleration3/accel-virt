#ifndef AV_GPU_RECT_HPP
#define AV_GPU_RECT_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;

class GpuRect {
public:
    static constexpr size_t encoded_size = 16u;

    GpuRect();

    [[nodiscard]] Status decode(WireReader &reader);
    bool                 inside(uint32_t width, uint32_t height) const;

    uint32_t x() const;
    uint32_t y() const;
    uint32_t width() const;
    uint32_t height() const;
    bool     decoded() const;

private:
    uint32_t x_;
    uint32_t y_;
    uint32_t width_;
    uint32_t height_;
    bool     decoded_;
};

}

#endif
