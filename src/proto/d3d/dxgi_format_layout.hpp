#ifndef AV_DXGI_FORMAT_LAYOUT_HPP
#define AV_DXGI_FORMAT_LAYOUT_HPP

#include "proto/resources/texel_layout.hpp"

#include <stdint.h>

namespace av {

class DxgiFormatLayout : public TexelLayout {
public:
    explicit DxgiFormatLayout(uint32_t format);

    bool known() const;

private:
    uint32_t format_;
};

}

#endif
