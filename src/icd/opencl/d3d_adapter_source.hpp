#ifndef AV_CLICD_D3D_ADAPTER_SOURCE_HPP
#define AV_CLICD_D3D_ADAPTER_SOURCE_HPP

#include <stdint.h>

namespace av::clicd {

class D3dAdapterSource {
public:
    D3dAdapterSource(const D3dAdapterSource &) = delete;
    D3dAdapterSource &operator=(const D3dAdapterSource &) = delete;

    virtual uint64_t adapter_luid(void *dxgi_adapter) = 0;
    virtual uint64_t device_luid(void *d3d10_device) = 0;

protected:
    D3dAdapterSource() = default;
    ~D3dAdapterSource() = default;
};

}

#endif
