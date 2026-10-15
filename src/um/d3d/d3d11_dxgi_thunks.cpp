/* The DXGI slots a device presents through, over the ones the root published:
 * the ABI boundary C requires, each one line into the device the argument's
 * hDevice names. */
#include "um/d3d/d3d11_dxgi_thunks.hpp"

#include "core/trace/function_scope.hpp"
#include "um/d3d/d3d11_device.hpp"
#include "um/d3d/device_call.hpp"
#include "um/d3d/device_handle.hpp"

namespace av::d3d11 {

namespace {

extern "C" HRESULT APIENTRY avumd_dxgi11_Present(DXGI_DDI_ARG_PRESENT *args)
{ return d3d::DeviceCall<D3d11Device>(DeviceHandle(reinterpret_cast<void *>(args->hDevice)).device()).traced("Present", &D3d11Device::present, args); }

extern "C" HRESULT APIENTRY avumd_dxgi11_Present1(DXGI1_6_1_DDI_ARG_PRESENT *args)
{ return d3d::DeviceCall<D3d11Device>(DeviceHandle(reinterpret_cast<void *>(args->hDevice)).device()).traced("Present1", &D3d11Device::present1, args); }

extern "C" HRESULT APIENTRY avumd_dxgi11_PresentMultiplaneOverlay(DXGI_DDI_ARG_PRESENTMULTIPLANEOVERLAY *args)
{ return d3d::DeviceCall<D3d11Device>(DeviceHandle(reinterpret_cast<void *>(args->hDevice)).device()).traced("PresentMultiplaneOverlay", &D3d11Device::present_multiplane_overlay, args); }

extern "C" HRESULT APIENTRY avumd_dxgi11_PresentMultiplaneOverlay1(DXGI1_6_1_DDI_ARG_PRESENTMULTIPLANEOVERLAY *args)
{ return d3d::DeviceCall<D3d11Device>(DeviceHandle(reinterpret_cast<void *>(args->hDevice)).device()).traced("PresentMultiplaneOverlay1", &D3d11Device::present_multiplane_overlay1, args); }

extern "C" HRESULT APIENTRY avumd_dxgi11_Blt(DXGI_DDI_ARG_BLT *args)
{ return d3d::DeviceCall<D3d11Device>(DeviceHandle(reinterpret_cast<void *>(args->hDevice)).device()).traced("Blt", &D3d11Device::blt, args); }

extern "C" HRESULT APIENTRY avumd_dxgi11_Blt1(DXGI_DDI_ARG_BLT1 *args)
{ return d3d::DeviceCall<D3d11Device>(DeviceHandle(reinterpret_cast<void *>(args->hDevice)).device()).traced("Blt1", &D3d11Device::blt1, args); }

extern "C" HRESULT APIENTRY avumd_dxgi11_RotateResourceIdentities(DXGI_DDI_ARG_ROTATE_RESOURCE_IDENTITIES *args)
{ return d3d::DeviceCall<D3d11Device>(DeviceHandle(reinterpret_cast<void *>(args->hDevice)).device()).traced("RotateResourceIdentities", &D3d11Device::rotate_resource_identities, args); }

extern "C" HRESULT APIENTRY avumd_dxgi11_ResolveSharedResource(DXGI_DDI_ARG_RESOLVESHAREDRESOURCE *args)
{ return d3d::DeviceCall<D3d11Device>(DeviceHandle(reinterpret_cast<void *>(args->hDevice)).device()).traced("ResolveSharedResource", &D3d11Device::resolve_shared_resource, args); }

extern "C" HRESULT APIENTRY avumd_dxgi11_SetDisplayMode(DXGI_DDI_ARG_SETDISPLAYMODE *args)
{ return d3d::DeviceCall<D3d11Device>(DeviceHandle(reinterpret_cast<void *>(args->hDevice)).device()).traced("SetDisplayMode", &D3d11Device::set_display_mode, args); }

extern "C" HRESULT APIENTRY avumd_dxgi11_QueryResourceResidency(DXGI_DDI_ARG_QUERYRESOURCERESIDENCY *args)
{ return d3d::DeviceCall<D3d11Device>(DeviceHandle(reinterpret_cast<void *>(args->hDevice)).device()).traced("QueryResourceResidency", &D3d11Device::query_resource_residency, args); }

}  // namespace

void fill_dxgi_funcs(DXGI1_6_1_DDI_BASE_FUNCTIONS &table)
{
    AV_SCOPE("fill_dxgi_funcs");
    table.pfnPresent = &avumd_dxgi11_Present;
    table.pfnPresent1 = &avumd_dxgi11_Present1;
    table.pfnPresentMultiplaneOverlay = &avumd_dxgi11_PresentMultiplaneOverlay;
    table.pfnPresentMultiplaneOverlay1 = &avumd_dxgi11_PresentMultiplaneOverlay1;
    table.pfnBlt = &avumd_dxgi11_Blt;
    table.pfnBlt1 = &avumd_dxgi11_Blt1;
    table.pfnRotateResourceIdentities = &avumd_dxgi11_RotateResourceIdentities;
    table.pfnResolveSharedResource = &avumd_dxgi11_ResolveSharedResource;
    table.pfnSetDisplayMode = &avumd_dxgi11_SetDisplayMode;
    table.pfnQueryResourceResidency = &avumd_dxgi11_QueryResourceResidency;
}

}  // namespace av::d3d11
