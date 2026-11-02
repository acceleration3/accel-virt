#ifndef AV_D3D12_DISPATCH_LIMIT_HPP
#define AV_D3D12_DISPATCH_LIMIT_HPP

#include <stdint.h>

struct ID3D12Device;

namespace av {

class D3d12DispatchLimit {
public:
    D3d12DispatchLimit();

    void measure(ID3D12Device &device);
    bool admits(uint32_t x, uint32_t y, uint32_t z) const;

private:
    uint32_t most_x_;
    uint32_t most_y_;
    uint32_t most_z_;
};

}

#endif
