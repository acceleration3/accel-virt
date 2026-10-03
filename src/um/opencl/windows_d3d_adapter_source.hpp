/* D3dAdapterSource over DXGI: an object is asked for the interface it is
 * named as - IDXGIAdapter, or IDXGIDevice for an ID3D10Device - rather than
 * trusted to be one, and an adapter answers its LUID in DXGI_ADAPTER_DESC. */
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
    /* The LUID of an adapter already asked for as one. */
    uint64_t luid_of(IDXGIAdapter &adapter);

    /* What each object is asked to be. */
    IID adapter_iid_;
    IID device_iid_;
};

}  // namespace av

#endif  // AV_WINDOWS_D3D_ADAPTER_SOURCE_HPP
