#ifndef AV_D3D12_COPY_PAIR_HPP
#define AV_D3D12_COPY_PAIR_HPP

#include <stdint.h>

struct D3D12_RESOURCE_DESC;

namespace av {

class D3d12CopyPair {
public:
    D3d12CopyPair(const D3D12_RESOURCE_DESC &destination, const D3D12_RESOURCE_DESC &source);

    bool whole() const;
    bool texels() const;

private:
    const D3D12_RESOURCE_DESC &destination_;
    const D3D12_RESOURCE_DESC &source_;
};

}

#endif
