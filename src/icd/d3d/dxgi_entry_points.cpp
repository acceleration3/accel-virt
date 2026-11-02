#include "icd/d3d/dxgi_entry_points.hpp"

#include "core/support/runtime_call.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/d3d/ddi/dxgi1_6_1_ddi_base_functions_record.hpp"
#include "icd/d3d/ddi/dxgi_ddi_args_record.hpp"
#include "icd/d3d/dxgi_base_table.hpp"
#include "icd/d3d/umd_binding.hpp"
#include "icd/d3d/umd_device.hpp"
#include "icd/d3d/umd_root.hpp"

namespace av::d3d {
namespace {
UmdDevice *device_behind(const void *args)
{
    AV_SCOPE("device_behind");
    if (args == nullptr) return nullptr;
    return umd_binding.root().device_of(static_cast<const DxgiDdiArgRecord *>(args)->hDevice);
}

#define AV_DXGI_LATER(name)                                                                                        \
    extern "C" Hresult AV_RUNTIME_CALL name(DxgiDdiArgRecord *args)                                                                           \
    {                                                                                                              \
        UmdDevice *const device = device_behind(args);                                                             \
        return device == nullptr ? hr_invalid_arg : device->dxgi().until_later(args);                       \
    }

AV_DXGI_LATER(avumd_dxgi_present)
AV_DXGI_LATER(avumd_dxgi_set_display_mode)
AV_DXGI_LATER(avumd_dxgi_query_resource_residency)
AV_DXGI_LATER(avumd_dxgi_rotate_resource_identities)
AV_DXGI_LATER(avumd_dxgi_blt)
AV_DXGI_LATER(avumd_dxgi_resolve_shared_resource)
AV_DXGI_LATER(avumd_dxgi_blt1)
AV_DXGI_LATER(avumd_dxgi_reserved1)
AV_DXGI_LATER(avumd_dxgi_present_multiplane_overlay)
AV_DXGI_LATER(avumd_dxgi_reserved2)
AV_DXGI_LATER(avumd_dxgi_present1)
AV_DXGI_LATER(avumd_dxgi_present_multiplane_overlay1)

#undef AV_DXGI_LATER

extern "C" Hresult AV_RUNTIME_CALL avumd_dxgi_get_gamma_caps(DxgiDdiArgGetGammaControlCapsRecord *args)
{
    UmdDevice *const device = device_behind(args);
    return device == nullptr ? hr_invalid_arg : device->dxgi().gamma_caps(args);
}

extern "C" Hresult AV_RUNTIME_CALL avumd_dxgi_set_resource_priority(DxgiDdiArgRecord *args)
{
    UmdDevice *const device = device_behind(args);
    return device == nullptr ? hr_invalid_arg : device->dxgi().resource_priority(args);
}

extern "C" Hresult AV_RUNTIME_CALL avumd_dxgi_get_multiplane_overlay_caps(DxgiDdiArgGetMultiplaneOverlayCapsRecord *args)
{
    UmdDevice *const device = device_behind(args);
    return device == nullptr ? hr_invalid_arg : device->dxgi().overlay_caps(args);
}

extern "C" Hresult AV_RUNTIME_CALL avumd_dxgi_get_multiplane_overlay_group_caps(DxgiDdiArgGetMultiplaneOverlayGroupCapsRecord *args)
{
    UmdDevice *const device = device_behind(args);
    return device == nullptr ? hr_invalid_arg : device->dxgi().overlay_group_caps(args);
}

extern "C" Hresult AV_RUNTIME_CALL avumd_dxgi_check_present_duration_support(DxgiDdiArgCheckPresentDurationSupportRecord *args)
{
    UmdDevice *const device = device_behind(args);
    return device == nullptr ? hr_invalid_arg : device->dxgi().present_duration(args);
}

extern "C" Hresult AV_RUNTIME_CALL avumd_dxgi_offer_resources1(DxgiDdiArgRecord *args)
{
    UmdDevice *const device = device_behind(args);
    return device == nullptr ? hr_invalid_arg : device->dxgi().offer_resources(args);
}

extern "C" Hresult AV_RUNTIME_CALL avumd_dxgi_reclaim_resources(DxgiDdiArgReclaimResourcesRecord *args)
{
    UmdDevice *const device = device_behind(args);
    return device == nullptr ? hr_invalid_arg : device->dxgi().reclaim_resources(args);
}

extern "C" Hresult AV_RUNTIME_CALL avumd_dxgi_reclaim_resources1(DxgiDdiArgReclaimResources1Record *args)
{
    UmdDevice *const device = device_behind(args);
    return device == nullptr ? hr_invalid_arg : device->dxgi().reclaim_resources1(args);
}

extern "C" Hresult AV_RUNTIME_CALL avumd_dxgi_trim_residency_set(DxgiDdiArgRecord *args)
{
    UmdDevice *const device = device_behind(args);
    return device == nullptr ? hr_invalid_arg : device->dxgi().trim_residency(args);
}

extern "C" Hresult AV_RUNTIME_CALL avumd_dxgi_check_multiplane_overlay_color_space_support(
    DxgiDdiArgCheckMultiplaneOverlayColorSpaceSupportRecord *args)
{
    UmdDevice *const device = device_behind(args);
    return device == nullptr ? hr_invalid_arg : device->dxgi().overlay_color_space(args);
}

}

void fill_dxgi_base_functions(Dxgi161DdiBaseFunctionsRecord &table)
{
    AV_SCOPE("fill_dxgi_base_functions");
    table.pfnPresent = reinterpret_cast<void *>(&avumd_dxgi_present);
    table.pfnGetGammaCaps = reinterpret_cast<void *>(&avumd_dxgi_get_gamma_caps);
    table.pfnSetDisplayMode = reinterpret_cast<void *>(&avumd_dxgi_set_display_mode);
    table.pfnSetResourcePriority = reinterpret_cast<void *>(&avumd_dxgi_set_resource_priority);
    table.pfnQueryResourceResidency = reinterpret_cast<void *>(&avumd_dxgi_query_resource_residency);
    table.pfnRotateResourceIdentities = reinterpret_cast<void *>(&avumd_dxgi_rotate_resource_identities);
    table.pfnBlt = reinterpret_cast<void *>(&avumd_dxgi_blt);
    table.pfnResolveSharedResource = reinterpret_cast<void *>(&avumd_dxgi_resolve_shared_resource);
    table.pfnBlt1 = reinterpret_cast<void *>(&avumd_dxgi_blt1);
    table.pfnOfferResources1 = reinterpret_cast<void *>(&avumd_dxgi_offer_resources1);
    table.pfnReclaimResources = reinterpret_cast<void *>(&avumd_dxgi_reclaim_resources);
    table.pfnGetMultiplaneOverlayCaps = reinterpret_cast<void *>(&avumd_dxgi_get_multiplane_overlay_caps);
    table.pfnGetMultiplaneOverlayGroupCaps = reinterpret_cast<void *>(&avumd_dxgi_get_multiplane_overlay_group_caps);
    table.pfnReserved1 = reinterpret_cast<void *>(&avumd_dxgi_reserved1);
    table.pfnPresentMultiplaneOverlay = reinterpret_cast<void *>(&avumd_dxgi_present_multiplane_overlay);
    table.pfnReserved2 = reinterpret_cast<void *>(&avumd_dxgi_reserved2);
    table.pfnPresent1 = reinterpret_cast<void *>(&avumd_dxgi_present1);
    table.pfnCheckPresentDurationSupport = reinterpret_cast<void *>(&avumd_dxgi_check_present_duration_support);
    table.pfnTrimResidencySet = reinterpret_cast<void *>(&avumd_dxgi_trim_residency_set);
    table.pfnCheckMultiplaneOverlayColorSpaceSupport =
        reinterpret_cast<void *>(&avumd_dxgi_check_multiplane_overlay_color_space_support);
    table.pfnPresentMultiplaneOverlay1 = reinterpret_cast<void *>(&avumd_dxgi_present_multiplane_overlay1);
    table.pfnReclaimResources1 = reinterpret_cast<void *>(&avumd_dxgi_reclaim_resources1);
}

}
