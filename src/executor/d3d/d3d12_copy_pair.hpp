#ifndef AV_D3D12_COPY_PAIR_HPP
#define AV_D3D12_COPY_PAIR_HPP

#include "executor/d3d/d3d12_copy_formats.hpp"

#include <stdint.h>

struct D3D12_RESOURCE_DESC;

namespace av {

class D3d12CopyPair {
public:
    D3d12CopyPair(const D3D12_RESOURCE_DESC &destination, const D3D12_RESOURCE_DESC &source);

    bool whole() const;
    bool texels() const;

private:
    D3d12CopyFormats formats() const;

    const D3D12_RESOURCE_DESC &destination_;
    const D3D12_RESOURCE_DESC &source_;
};

}

#endif
