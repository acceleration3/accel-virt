#include "core/vidpn/cofunc_modality.hpp"

#include "core/ddi/d3dkmdt_vidpn_present_path_record.hpp"
#include "core/ddi/dxgkarg_enum_vidpn_cofunc_modality_record.hpp"
#include "core/display/display_mode.hpp"
#include "core/display/display_mode_list.hpp"
#include "core/display/display_sources.hpp"
#include "core/platform/log_budget.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/vidpn/vidpn_mode_set.hpp"

namespace av {

CofuncModality::CofuncModality(Platform &platform, DxgkrnlInterface &dxgkrnl, LogBudget &budget,
                               const DisplayModeList &modes, const DisplaySources &sources,
                               const DxgkargEnumVidPnCofuncModalityRecord &request)
    : platform_(platform), sources_(sources), budget_(budget), modes_(modes), request_(request),
      vidpn_(platform, dxgkrnl, request.hConstrainingVidPn)
      {
          AV_SCOPE("CofuncModality::CofuncModality");
      }

NtStatus CofuncModality::enumerate()
{
    AV_SCOPE("CofuncModality::enumerate");
    platform_.require_irql_at_most(Irql::passive, "CofuncModality::enumerate");
    Status status = vidpn_.initialize();
    if (status == Status::ok) status = vidpn_.for_each_path(*this);
    return NtStatus(status);
}

Status CofuncModality::visit(const D3dkmdtVidPnPresentPathRecord &path)
{
    AV_SCOPE("CofuncModality::visit");
    platform_.require_irql_at_most(Irql::passive, "CofuncModality::visit");
    if (path.VidPnSourceId >= sources_.count() || path.VidPnTargetId >= sources_.count())
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
    if (status != Status::ok) return status;
    log_path(path, source_pin == Status::ok ? &pinned : nullptr, target_pin == Status::ok);
    return update_support(path);
}

Status CofuncModality::copy_pinned_source(uint32_t source, D3dkmdtVidPnSourceModeRecord &mode)
{
    AV_SCOPE("CofuncModality::copy_pinned_source");
    SourceModeSet set(platform_, vidpn_, source);
    const Status status = set.acquire();
    return status != Status::ok ? status : set.copy_pinned(mode);
}

Status CofuncModality::pinned_target(uint32_t target)
{
    AV_SCOPE("CofuncModality::pinned_target");
    TargetModeSet set(platform_, vidpn_, target);
    D3dkmdtVidPnTargetModeRecord mode = {};
    const Status status = set.acquire();
    return status != Status::ok ? status : set.copy_pinned(mode);
}

Status CofuncModality::offer_sources(uint32_t source)
{
    AV_SCOPE("CofuncModality::offer_sources");
    SourceModeSet set(platform_, vidpn_, source);
    Status status = set.create();
    for (const int32_t format : DisplayMode::source_formats)
        for (uint32_t index = 0u; status == Status::ok && index < modes_.count(); index++) {
            D3dkmdtVidPnSourceModeRecord description = {};
            modes_.mode(index).describe(description, format);
            status = set.add(description);
        }
    return status != Status::ok ? status : set.assign();
}

/* A pinned source mode the adapter does not have leaves the target set alone:
 * DMMVIDPNTARGET::SetCofuncModeSet (dxgkrnl.sys @14005004c) refuses an empty
 * cofunctional set with STATUS_GRAPHICS_EMPTY_ADAPTER_MONITOR_MODE_SUPPORT_INTERSECTION. */
Status CofuncModality::offer_targets(uint32_t target, const D3dkmdtVidPnSourceModeRecord *pinned_source)
{
    AV_SCOPE("CofuncModality::offer_targets");
    const DisplayMode *only = pinned_source != nullptr ? modes_.find(*pinned_source) : nullptr;
    if (pinned_source != nullptr && only == nullptr) return Status::ok;
    TargetModeSet set(platform_, vidpn_, target);
    Status        status = set.create();
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

/* What the adapter can do with a path is identity scaling and identity rotation
 * at offset zero, and every path is told so on every enumeration, pivot or not.
 *
 * Once per path per enumeration is what dxgkrnl counts: UpdatePathSupportInfo
 * (dxgkrnl.sys @1403ab840) decrements a countdown and only calls
 * DisallowModifyingAction when it reaches zero, which is what closes the
 * enumeration. Skip a path and the topology stays modifiable, and DMM walks its
 * pivots - source, target, scaling, rotation - and then throws the VidPn away
 * and starts again, which is the loop the guest log shows.
 *
 * Offset0 is not optional either: IsDriverSupportPathIndependentRotation
 * (@1403d9d78) answers yes for any driver whose declared interface version is
 * above 0x4002, so always for us, and SetRotationSupport (@14004b80c) then
 * bugchecks 0x113 subcode 0x1C on a test-signed machine when the mask for a
 * primary clone path - the only path we have - lacks it. */
Status CofuncModality::update_support(const D3dkmdtVidPnPresentPathRecord &path)
{
    AV_SCOPE("CofuncModality::update_support");
    D3dkmdtVidPnPresentPathRecord update = path;
    update.ContentTransformationScalingSupport = identity_supported;
    update.ContentTransformationRotationSupport = identity_supported | rotation_offset_zero;
    return vidpn_.update_path_support(update);
}

void CofuncModality::log_path(const D3dkmdtVidPnPresentPathRecord &path,
                              const D3dkmdtVidPnSourceModeRecord *pinned_source, bool target_pinned)
{
    AV_SCOPE("CofuncModality::log_path");
    if (!budget_.allows()) return;
    char       bytes[128];
    TextBuffer line(bytes, sizeof bytes);
    line.append("avgpu: cofunc path ");
    line.append_decimal(path.VidPnSourceId);
    line.append("->");
    line.append_decimal(path.VidPnTargetId);
    line.append(" pivot ");
    line.append_decimal(static_cast<uint64_t>(request_.EnumPivotType));
    line.append(" scaling ");
    line.append_decimal(static_cast<uint64_t>(path.ContentTransformationScaling));
    line.append(" rotation ");
    line.append_decimal(static_cast<uint64_t>(path.ContentTransformationRotation));
    if (pinned_source != nullptr) {
        line.append(" source ");
        line.append_decimal(pinned_source->PrimSurfSizeCx);
        line.append("x");
        line.append_decimal(pinned_source->PrimSurfSizeCy);
    } else {
        line.append(" source unpinned");
    }
    line.append(target_pinned ? " target pinned" : " target unpinned");
    platform_.log(line.text());
}

}  // namespace av
