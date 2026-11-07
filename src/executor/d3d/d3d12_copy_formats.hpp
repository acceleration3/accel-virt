#ifndef AV_D3D12_COPY_FORMATS_HPP
#define AV_D3D12_COPY_FORMATS_HPP

#include "proto/d3d/dxgi_format.hpp"

#include <stdint.h>

namespace av {

class D3d12CopyFormats {
public:
    D3d12CopyFormats(uint32_t destination, uint32_t source);

    bool same_texels() const;
    bool plane_of_destination() const;

private:
    DxgiFormat destination_;
    DxgiFormat source_;
};

}

#endif
