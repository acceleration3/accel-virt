#ifndef AV_D3D9_FORMAT_LAYOUT_HPP
#define AV_D3D9_FORMAT_LAYOUT_HPP

#include "proto/resources/texel_layout.hpp"

#include <stdint.h>

namespace av {

class D3d9FormatLayout : public TexelLayout {
public:
    explicit D3d9FormatLayout(uint32_t format);

private:
    void four_cc();

    uint32_t format_;
};

}

#endif
