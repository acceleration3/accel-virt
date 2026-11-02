#ifndef AV_WINDOWS_D3D_ADAPTER_SOURCE_HPP
#define AV_WINDOWS_D3D_ADAPTER_SOURCE_HPP

#include "icd/opencl/d3d_adapter_source.hpp"

#include <dxgi.h>
#include <stdint.h>

namespace av {

class WindowsD3dAdapterSource final : public clicd::D3dAdapterSource {
public:
    WindowsD3dAdapterSource();

    uint64_t adapter_luid(void *dxgi_adapter) override;
    uint64_t device_luid(void *d3d10_device) override;

private:
    uint64_t luid_of(IDXGIAdapter &adapter);

    IID adapter_iid_;
    IID device_iid_;
};

}

#endif
