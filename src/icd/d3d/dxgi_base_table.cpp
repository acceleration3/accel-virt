#include "icd/d3d/dxgi_base_table.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/d3d/ddi/dxgi1_6_1_ddi_base_functions_record.hpp"
#include "icd/d3d/ddi/dxgi_ddi_args_record.hpp"
#include "icd/d3d/ddi/dxgi_ddi_base_args_record.hpp"
#include "icd/d3d/ddi/dxgi_ddi_base_callbacks_record.hpp"
#include "icd/d3d/dxgi_entry_points.hpp"
#include "icd/d3d/umd_adapter.hpp"
#include "icd/d3d/umd_device.hpp"

namespace av::d3d {

namespace {

/* D3DDDI_RECLAIM_RESULT_OK: reclaimed with its contents. */
constexpr uint32_t reclaim_ok = 0u;

}  // namespace

DxgiBaseTable::DxgiBaseTable(UmdDevice &device) : device_(device), revision_(Revision::none), callbacks_(nullptr)
{
    AV_SCOPE("DxgiBaseTable::DxgiBaseTable");
}

Hresult DxgiBaseTable::publish(DxgiDdiBaseArgsRecord *args, uint32_t interface_version)
{
    AV_SCOPE("DxgiBaseTable::publish");
    if (args == nullptr || args->pDXGIBaseCallbacks == nullptr || args->pDXGIDDIBaseFunctions == nullptr)
        return hr_invalid_arg;
    if (!device_.adapter().versions().serves(interface_version)) return hr_no_interface;
    fill_dxgi_base_functions(*static_cast<Dxgi161DdiBaseFunctionsRecord *>(args->pDXGIDDIBaseFunctions));
    callbacks_ = static_cast<const DxgiDdiBaseCallbacksRecord *>(args->pDXGIBaseCallbacks);
    revision_ = Revision::wddm_3_2;
    return hr_ok;
}

DxgiBaseTable::Revision DxgiBaseTable::revision() const
{
    AV_SCOPE("DxgiBaseTable::revision");
    return revision_;
}

bool DxgiBaseTable::published() const
{
    AV_SCOPE("DxgiBaseTable::published");
    return revision_ != Revision::none;
}

const DxgiDdiBaseCallbacksRecord *DxgiBaseTable::callbacks() const
{
    AV_SCOPE("DxgiBaseTable::callbacks");
    return callbacks_;
}

Hresult DxgiBaseTable::usable(const DxgiDdiArgRecord *args, const char *entry)
{
    AV_SCOPE("DxgiBaseTable::usable");
    if (args == nullptr) return hr_invalid_arg;
    return device_.lost(entry);
}

Hresult DxgiBaseTable::gamma_caps(DxgiDdiArgGetGammaControlCapsRecord *args)
{
    AV_SCOPE("DxgiBaseTable::gamma_caps");
    const Hresult refused = usable(reinterpret_cast<const DxgiDdiArgRecord *>(args), "pfnGetGammaCaps");
    if (refused != hr_ok) return refused;
    if (args->pGammaCapabilities == nullptr) return hr_invalid_arg;
    DxgiGammaControlCapabilitiesRecord *const caps =
        static_cast<DxgiGammaControlCapabilitiesRecord *>(args->pGammaCapabilities);
    caps->ScaleAndOffsetSupported = 0;
    caps->MaxConvertedValue = 0.0f;
    caps->MinConvertedValue = 0.0f;
    /* Zero control points, so the 1025-entry array behind these four fields is
     * the runtime's and is not touched. */
    caps->NumGammaControlPoints = 0u;
    return hr_ok;
}

Hresult DxgiBaseTable::resource_priority(DxgiDdiArgRecord *args)
{
    AV_SCOPE("DxgiBaseTable::resource_priority");
    /* Advisory, and this driver has no eviction order of its own to apply it
     * to: accepted and dropped, which is what a driver without one does. */
    return usable(args, "pfnSetResourcePriority");
}

Hresult DxgiBaseTable::overlay_caps(DxgiDdiArgGetMultiplaneOverlayCapsRecord *args)
{
    AV_SCOPE("DxgiBaseTable::overlay_caps");
    const Hresult refused = usable(reinterpret_cast<const DxgiDdiArgRecord *>(args), "pfnGetMultiplaneOverlayCaps");
    if (refused != hr_ok) return refused;
    args->MaxPlanes = 0u;
    args->MaxRGBPlanes = 0u;
    args->MaxYUVPlanes = 0u;
    args->MaxStretchFactor = 0.0f;
    args->MaxShrinkFactor = 0.0f;
    return hr_ok;
}

Hresult DxgiBaseTable::overlay_group_caps(DxgiDdiArgGetMultiplaneOverlayGroupCapsRecord *args)
{
    AV_SCOPE("DxgiBaseTable::overlay_group_caps");
    const Hresult refused =
        usable(reinterpret_cast<const DxgiDdiArgRecord *>(args), "pfnGetMultiplaneOverlayGroupCaps");
    if (refused != hr_ok) return refused;
    args->MaxPlanes = 0u;
    args->MaxStretchFactor = 0u;
    args->MaxShrinkFactor = 0u;
    args->OverlayCaps = 0u;
    return hr_ok;
}

Hresult DxgiBaseTable::present_duration(DxgiDdiArgCheckPresentDurationSupportRecord *args)
{
    AV_SCOPE("DxgiBaseTable::present_duration");
    const Hresult refused = usable(reinterpret_cast<const DxgiDdiArgRecord *>(args), "pfnCheckPresentDurationSupport");
    if (refused != hr_ok) return refused;
    args->ClosestSmallerDuration = 0u;
    args->ClosestLargerDuration = 0u;
    return hr_ok;
}

Hresult DxgiBaseTable::overlay_color_space(DxgiDdiArgCheckMultiplaneOverlayColorSpaceSupportRecord *args)
{
    AV_SCOPE("DxgiBaseTable::overlay_color_space");
    const Hresult refused =
        usable(reinterpret_cast<const DxgiDdiArgRecord *>(args), "pfnCheckMultiplaneOverlayColorSpaceSupport");
    if (refused != hr_ok) return refused;
    args->Supported = 0;
    return hr_ok;
}

Hresult DxgiBaseTable::trim_residency(DxgiDdiArgRecord *args)
{
    AV_SCOPE("DxgiBaseTable::trim_residency");
    /* Residency is the KMD's here, so there is nothing of the driver's to
     * trim; the runtime asks and is told it happened. */
    return usable(args, "pfnTrimResidencySet");
}

Hresult DxgiBaseTable::offer_resources(DxgiDdiArgRecord *args)
{
    AV_SCOPE("DxgiBaseTable::offer_resources");
    return usable(args, "pfnOfferResources1");
}

Hresult DxgiBaseTable::reclaim_resources(DxgiDdiArgReclaimResourcesRecord *args)
{
    AV_SCOPE("DxgiBaseTable::reclaim_resources");
    const Hresult refused = usable(reinterpret_cast<const DxgiDdiArgRecord *>(args), "pfnReclaimResources");
    if (refused != hr_ok) return refused;
    if (args->Resources != 0u && args->pResources == nullptr) return hr_invalid_arg;
    if (args->pDiscarded != nullptr)
        for (uint32_t index = 0u; index < args->Resources; index++) args->pDiscarded[index] = 0;
    return hr_ok;
}

Hresult DxgiBaseTable::reclaim_resources1(DxgiDdiArgReclaimResources1Record *args)
{
    AV_SCOPE("DxgiBaseTable::reclaim_resources1");
    const Hresult refused = usable(reinterpret_cast<const DxgiDdiArgRecord *>(args), "pfnReclaimResources1");
    if (refused != hr_ok) return refused;
    if (args->Resources != 0u && (args->pResources == nullptr || args->pResults == nullptr)) return hr_invalid_arg;
    for (uint32_t index = 0u; index < args->Resources; index++) args->pResults[index] = reclaim_ok;
    return hr_ok;
}

Hresult DxgiBaseTable::until_later(DxgiDdiArgRecord *args, const char *entry)
{
    AV_SCOPE("DxgiBaseTable::until_later");
    const Hresult refused = usable(args, entry);
    if (refused != hr_ok) return refused;
    return hr_not_impl;
}

void DxgiBaseTable::report(StateWriter &writer) const
{
    AV_SCOPE("DxgiBaseTable::report");
    writer.number("dxgi_revision", static_cast<uint64_t>(revision_));
    writer.address("dxgi_callbacks", callbacks_);
}

bool DxgiBaseTable::invariants_hold() const
{
    AV_SCOPE("DxgiBaseTable::invariants_hold");
    return (revision_ == Revision::none) == (callbacks_ == nullptr);
}

}  // namespace av::d3d
