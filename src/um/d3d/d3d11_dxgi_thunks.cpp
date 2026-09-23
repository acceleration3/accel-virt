/* The DXGI slots a device presents through, over the ones the root published:
 * the ABI boundary C requires, each one line into the device the argument's
 * hDevice names. */
#include "um/d3d/d3d11_dxgi_thunks.hpp"

#include "core/trace/function_scope.hpp"
#include "um/d3d/d3d11_device.hpp"
#include "um/d3d/device_handle.hpp"

namespace av::d3d11 {

namespace {

HRESULT APIENTRY dxgi_Present(DXGI_DDI_ARG_PRESENT *args)
{ return DeviceHandle(reinterpret_cast<void *>(args->hDevice)).device().present(args); }

HRESULT APIENTRY dxgi_Present1(DXGI1_6_1_DDI_ARG_PRESENT *args)
{ return DeviceHandle(reinterpret_cast<void *>(args->hDevice)).device().present1(args); }

HRESULT APIENTRY dxgi_Blt(DXGI_DDI_ARG_BLT *args)
{ return DeviceHandle(reinterpret_cast<void *>(args->hDevice)).device().blt(args); }

HRESULT APIENTRY dxgi_Blt1(DXGI_DDI_ARG_BLT1 *args)
{ return DeviceHandle(reinterpret_cast<void *>(args->hDevice)).device().blt1(args); }

HRESULT APIENTRY dxgi_RotateResourceIdentities(DXGI_DDI_ARG_ROTATE_RESOURCE_IDENTITIES *args)
{ return DeviceHandle(reinterpret_cast<void *>(args->hDevice)).device().rotate_resource_identities(args); }

HRESULT APIENTRY dxgi_ResolveSharedResource(DXGI_DDI_ARG_RESOLVESHAREDRESOURCE *args)
{ return DeviceHandle(reinterpret_cast<void *>(args->hDevice)).device().resolve_shared_resource(args); }

HRESULT APIENTRY dxgi_SetDisplayMode(DXGI_DDI_ARG_SETDISPLAYMODE *args)
{ return DeviceHandle(reinterpret_cast<void *>(args->hDevice)).device().set_display_mode(args); }

HRESULT APIENTRY dxgi_QueryResourceResidency(DXGI_DDI_ARG_QUERYRESOURCERESIDENCY *args)
{ return DeviceHandle(reinterpret_cast<void *>(args->hDevice)).device().query_resource_residency(args); }

}  // namespace

void fill_dxgi_funcs(DXGI1_6_1_DDI_BASE_FUNCTIONS &table)
{
    AV_SCOPE("fill_dxgi_funcs");
    table.pfnPresent = &dxgi_Present;
    table.pfnPresent1 = &dxgi_Present1;
    table.pfnBlt = &dxgi_Blt;
    table.pfnBlt1 = &dxgi_Blt1;
    table.pfnRotateResourceIdentities = &dxgi_RotateResourceIdentities;
    table.pfnResolveSharedResource = &dxgi_ResolveSharedResource;
    table.pfnSetDisplayMode = &dxgi_SetDisplayMode;
    table.pfnQueryResourceResidency = &dxgi_QueryResourceResidency;
}

}  // namespace av::d3d11
