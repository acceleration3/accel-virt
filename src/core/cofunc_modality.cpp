#include "cofunc_modality.hpp"

#include "ddi/d3dkmdt_vidpn_present_path_record.hpp"
#include "ddi/dxgkarg_enum_vidpn_cofunc_modality_record.hpp"
#include "display_mode_list.hpp"
#include "display_sources.hpp"
#include "vidpn_mode_set.hpp"

namespace av {

CofuncModality::CofuncModality(Platform &platform, DxgkrnlInterface &dxgkrnl, const DisplayModeList &modes,
                               const DxgkargEnumVidPnCofuncModalityRecord &request)
    : platform_(platform), modes_(modes), request_(request), vidpn_(platform, dxgkrnl, request.hConstrainingVidPn) {}

NtStatus CofuncModality::enumerate()
{
    platform_.require_irql_at_most(Irql::passive, "CofuncModality::enumerate");
    Status status = vidpn_.initialize();
    if (status == Status::ok) status = vidpn_.for_each_path(*this);
    return NtStatus(status);
}

Status CofuncModality::visit(const D3dkmdtVidPnPresentPathRecord &path)
{
    platform_.require_irql_at_most(Irql::passive, "CofuncModality::visit");
    if (path.VidPnSourceId >= DisplaySources::source_count || path.VidPnTargetId >= DisplaySources::target_count)
        return Status::invalid_parameter;
    D3dkmdtVidPnSourceModeRecord pinned = {};
    const Status source_pin = copy_pinned_source(path.VidPnSourceId, pinned);
    if (source_pin != Status::ok && source_pin != Status::not_found) return source_pin;
    const bool source_pivot = request_.EnumPivotType == pivot_source && request_.EnumPivotVidPnSourceId == path.VidPnSourceId;
    Status status = source_pin == Status::not_found && !source_pivot ? offer_sources(path.VidPnSourceId) : Status::ok;
    if (status != Status::ok) return status;
    const Status target_pin = pinned_target(path.VidPnTargetId);
    if (target_pin != Status::ok && target_pin != Status::not_found) return target_pin;
    const bool target_pivot = request_.EnumPivotType == pivot_target && request_.EnumPivotVidPnTargetId == path.VidPnTargetId;
    if (target_pin == Status::not_found && !target_pivot)
        status = offer_targets(path.VidPnTargetId, source_pin == Status::ok ? &pinned : nullptr);
    return status != Status::ok ? status : update_support(path);
}

Status CofuncModality::copy_pinned_source(uint32_t source, D3dkmdtVidPnSourceModeRecord &mode)
{
    SourceModeSet set(platform_, vidpn_, source);
    const Status status = set.acquire();
    return status != Status::ok ? status : set.copy_pinned(mode);
}

Status CofuncModality::pinned_target(uint32_t target)
{
    TargetModeSet set(platform_, vidpn_, target);
    D3dkmdtVidPnTargetModeRecord mode = {};
    const Status status = set.acquire();
    return status != Status::ok ? status : set.copy_pinned(mode);
}

Status CofuncModality::offer_sources(uint32_t source)
{
    SourceModeSet set(platform_, vidpn_, source);
    Status status = set.create();
    for (uint32_t index = 0u; status == Status::ok && index < modes_.count(); index++) {
        D3dkmdtVidPnSourceModeRecord description = {};
        modes_.mode(index).describe(description);
        status = set.add(description);
    }
    return status != Status::ok ? status : set.assign();
}

Status CofuncModality::offer_targets(uint32_t target, const D3dkmdtVidPnSourceModeRecord *pinned_source)
{
    TargetModeSet      set(platform_, vidpn_, target);
    const DisplayMode *only = pinned_source != nullptr ? modes_.find(*pinned_source) : nullptr;
    Status             status = set.create();
    for (uint32_t index = 0u; status == Status::ok && index < modes_.count(); index++) {
        const DisplayMode &mode = modes_.mode(index);
        if (pinned_source != nullptr && &mode != only) continue;
        D3dkmdtVidPnTargetModeRecord description = {};
        mode.describe(description.VideoSignalInfo);
        description.WireFormatAndPreference = (modes_.is_preferred(mode) ? preferred : not_preferred) |
                                              rgb_8_bits_per_channel;
        status = set.add(description);
    }
    return status != Status::ok ? status : set.assign();
}

/* Zero scaling or rotation is D3DKMDT_VPPS_UNINITIALIZED / D3DKMDT_VPPR_UNINITIALIZED. */
Status CofuncModality::update_support(const D3dkmdtVidPnPresentPathRecord &path)
{
    D3dkmdtVidPnPresentPathRecord update = path;
    const bool offer_scaling = path.ContentTransformationScaling == 0 && request_.EnumPivotType != pivot_scaling;
    const bool offer_rotation = path.ContentTransformationRotation == 0 && request_.EnumPivotType != pivot_rotation;
    if (offer_scaling) update.ContentTransformationScalingSupport = identity_supported;
    if (offer_rotation) update.ContentTransformationRotationSupport = identity_supported;
    return offer_scaling || offer_rotation ? vidpn_.update_path_support(update) : Status::ok;
}

}  // namespace av
