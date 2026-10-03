/* Which adapter a Direct3D object an application hands cl_khr_d3d10_sharing
 * is on, as the LUID the adapter enumerations name it by: an IDXGIAdapter's
 * own, or the adapter an ID3D10Device was made on. Only COM can answer either,
 * and portable code cannot include it.
 *
 * Two implementations: the Windows one in the shim, which asks the object
 * itself through DXGI, and one in the tests that answers what the test
 * scripted. */
#ifndef AV_CLICD_D3D_ADAPTER_SOURCE_HPP
#define AV_CLICD_D3D_ADAPTER_SOURCE_HPP

#include <stdint.h>

namespace av::clicd {

class D3dAdapterSource {
public:
    D3dAdapterSource(const D3dAdapterSource &) = delete;
    D3dAdapterSource &operator=(const D3dAdapterSource &) = delete;

    /* The LUID of `dxgi_adapter`, an IDXGIAdapter; 0 for an object that is
     * not one or will not say. */
    virtual uint64_t adapter_luid(void *dxgi_adapter) = 0;
    /* The LUID of the adapter `d3d10_device`, an ID3D10Device, was made on;
     * 0 for an object that is not one or will not say. */
    virtual uint64_t device_luid(void *d3d10_device) = 0;

protected:
    D3dAdapterSource() = default;
    ~D3dAdapterSource() = default;
};

}  // namespace av::clicd

#endif  // AV_CLICD_D3D_ADAPTER_SOURCE_HPP
