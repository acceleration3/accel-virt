#ifndef AV_D3D12_RESOURCE_FLAGS_HPP
#define AV_D3D12_RESOURCE_FLAGS_HPP

#include <stdint.h>

namespace av {

class D3d12ResourceFlags {
public:
    explicit D3d12ResourceFlags(uint32_t guest);

    uint32_t kept() const;

private:
    uint32_t guest_;
};

}

#endif
