#ifndef AV_CROSS_ADAPTER_LAYOUT_HPP
#define AV_CROSS_ADAPTER_LAYOUT_HPP

#include "proto/resources/surface_layout.hpp"

#include <stdint.h>

namespace av {

class CrossAdapterLayout {
public:
    static constexpr uint32_t pitch_alignment = 256u;
    static constexpr uint32_t height_alignment = 4u;

    CrossAdapterLayout(uint32_t width, uint32_t height, uint32_t format, uint32_t bytes_per_pixel);

    bool     valid() const;
    uint32_t pitch() const;
    uint64_t bytes() const;
    SurfaceLayout surface() const;

private:
    uint32_t width_;
    uint32_t height_;
    uint32_t format_;
    uint32_t pitch_;
    uint64_t bytes_;
    bool     valid_;
};

}

#endif
