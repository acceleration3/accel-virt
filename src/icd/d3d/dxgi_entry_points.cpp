/* The ABI boundary C requires (constitution III): each thunk finds the device
 * behind its argument's hDevice and asks it. No logic here. */
#include "icd/d3d/dxgi_entry_points.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/d3d/ddi/dxgi1_6_1_ddi_base_functions_record.hpp"
#include "icd/d3d/ddi/dxgi_ddi_args_record.hpp"
#include "icd/d3d/dxgi_base_table.hpp"
#include "icd/d3d/umd_binding.hpp"
#include "icd/d3d/umd_device.hpp"
#include "icd/d3d/umd_root.hpp"

namespace av::d3d {
namespace {

/* A device the root did not make: the argument names nothing, so there is no
 * device to answer for and no log to write it in. */
UmdDevice *device_behind(const void *args)
{
    AV_SCOPE("device_behind");
    if (args == nullptr) return nullptr;
    return umd_binding.root().device_of(static_cast<const DxgiDdiArgRecord *>(args)->hDevice);
}

#define AV_DXGI_LATER(name, entry)                                                                                 \
    extern "C" Hresult name(DxgiDdiArgRecord *args)                                                                           \
    {                                                                                                              \
        UmdDevice *const device = device_behind(args);                                                             \
        return device == nullptr ? hr_invalid_arg : device->dxgi().until_later(args, entry);                       \
    }

AV_DXGI_LATER(rvkumd_dxgi_present, "pfnPresent")
AV_DXGI_LATER(rvkumd_dxgi_set_display_mode, "pfnSetDisplayMode")
AV_DXGI_LATER(rvkumd_dxgi_query_resource_residency, "pfnQueryResourceResidency")
AV_DXGI_LATER(rvkumd_dxgi_rotate_resource_identities, "pfnRotateResourceIdentities")
AV_DXGI_LATER(rvkumd_dxgi_blt, "pfnBlt")
AV_DXGI_LATER(rvkumd_dxgi_resolve_shared_resource, "pfnResolveSharedResource")
AV_DXGI_LATER(rvkumd_dxgi_blt1, "pfnBlt1")
AV_DXGI_LATER(rvkumd_dxgi_offer_resources1, "pfnOfferResources1")
AV_DXGI_LATER(rvkumd_dxgi_reclaim_resources, "pfnReclaimResources")
AV_DXGI_LATER(rvkumd_dxgi_reserved1, "pfnReserved1")
AV_DXGI_LATER(rvkumd_dxgi_present_multiplane_overlay, "pfnPresentMultiplaneOverlay")
AV_DXGI_LATER(rvkumd_dxgi_reserved2, "pfnReserved2")
AV_DXGI_LATER(rvkumd_dxgi_present1, "pfnPresent1")
AV_DXGI_LATER(rvkumd_dxgi_present_multiplane_overlay1, "pfnPresentMultiplaneOverlay1")
AV_DXGI_LATER(rvkumd_dxgi_reclaim_resources1, "pfnReclaimResources1")

#undef AV_DXGI_LATER

extern "C" Hresult rvkumd_dxgi_get_gamma_caps(DxgiDdiArgGetGammaControlCapsRecord *args)
{
    UmdDevice *const device = device_behind(args);
    return device == nullptr ? hr_invalid_arg : device->dxgi().gamma_caps(args);
}

extern "C" Hresult rvkumd_dxgi_set_resource_priority(DxgiDdiArgRecord *args)
{
    UmdDevice *const device = device_behind(args);
    return device == nullptr ? hr_invalid_arg : device->dxgi().resource_priority(args);
}

extern "C" Hresult rvkumd_dxgi_get_multiplane_overlay_caps(DxgiDdiArgGetMultiplaneOverlayCapsRecord *args)
{
    UmdDevice *const device = device_behind(args);
    return device == nullptr ? hr_invalid_arg : device->dxgi().overlay_caps(args);
}

extern "C" Hresult rvkumd_dxgi_get_multiplane_overlay_group_caps(DxgiDdiArgGetMultiplaneOverlayGroupCapsRecord *args)
{
    UmdDevice *const device = device_behind(args);
    return device == nullptr ? hr_invalid_arg : device->dxgi().overlay_group_caps(args);
}

extern "C" Hresult rvkumd_dxgi_check_present_duration_support(DxgiDdiArgCheckPresentDurationSupportRecord *args)
{
    UmdDevice *const device = device_behind(args);
    return device == nullptr ? hr_invalid_arg : device->dxgi().present_duration(args);
}

extern "C" Hresult rvkumd_dxgi_trim_residency_set(DxgiDdiArgRecord *args)
{
    UmdDevice *const device = device_behind(args);
    return device == nullptr ? hr_invalid_arg : device->dxgi().trim_residency(args);
}

extern "C" Hresult rvkumd_dxgi_check_multiplane_overlay_color_space_support(
    DxgiDdiArgCheckMultiplaneOverlayColorSpaceSupportRecord *args)
{
    UmdDevice *const device = device_behind(args);
    return device == nullptr ? hr_invalid_arg : device->dxgi().overlay_color_space(args);
}

}  // namespace

void fill_dxgi_base_functions(Dxgi161DdiBaseFunctionsRecord &table)
{
    AV_SCOPE("fill_dxgi_base_functions");
    table.pfnPresent = reinterpret_cast<void *>(&rvkumd_dxgi_present);
    table.pfnGetGammaCaps = reinterpret_cast<void *>(&rvkumd_dxgi_get_gamma_caps);
    table.pfnSetDisplayMode = reinterpret_cast<void *>(&rvkumd_dxgi_set_display_mode);
    table.pfnSetResourcePriority = reinterpret_cast<void *>(&rvkumd_dxgi_set_resource_priority);
    table.pfnQueryResourceResidency = reinterpret_cast<void *>(&rvkumd_dxgi_query_resource_residency);
    table.pfnRotateResourceIdentities = reinterpret_cast<void *>(&rvkumd_dxgi_rotate_resource_identities);
    table.pfnBlt = reinterpret_cast<void *>(&rvkumd_dxgi_blt);
    table.pfnResolveSharedResource = reinterpret_cast<void *>(&rvkumd_dxgi_resolve_shared_resource);
    table.pfnBlt1 = reinterpret_cast<void *>(&rvkumd_dxgi_blt1);
    table.pfnOfferResources1 = reinterpret_cast<void *>(&rvkumd_dxgi_offer_resources1);
    table.pfnReclaimResources = reinterpret_cast<void *>(&rvkumd_dxgi_reclaim_resources);
    table.pfnGetMultiplaneOverlayCaps = reinterpret_cast<void *>(&rvkumd_dxgi_get_multiplane_overlay_caps);
    table.pfnGetMultiplaneOverlayGroupCaps = reinterpret_cast<void *>(&rvkumd_dxgi_get_multiplane_overlay_group_caps);
    table.pfnReserved1 = reinterpret_cast<void *>(&rvkumd_dxgi_reserved1);
    table.pfnPresentMultiplaneOverlay = reinterpret_cast<void *>(&rvkumd_dxgi_present_multiplane_overlay);
    table.pfnReserved2 = reinterpret_cast<void *>(&rvkumd_dxgi_reserved2);
    table.pfnPresent1 = reinterpret_cast<void *>(&rvkumd_dxgi_present1);
    table.pfnCheckPresentDurationSupport = reinterpret_cast<void *>(&rvkumd_dxgi_check_present_duration_support);
    table.pfnTrimResidencySet = reinterpret_cast<void *>(&rvkumd_dxgi_trim_residency_set);
    table.pfnCheckMultiplaneOverlayColorSpaceSupport =
        reinterpret_cast<void *>(&rvkumd_dxgi_check_multiplane_overlay_color_space_support);
    table.pfnPresentMultiplaneOverlay1 = reinterpret_cast<void *>(&rvkumd_dxgi_present_multiplane_overlay1);
    table.pfnReclaimResources1 = reinterpret_cast<void *>(&rvkumd_dxgi_reclaim_resources1);
}

}  // namespace av::d3d
