#include "miniport/vidpn/vidpn_support_check.hpp"

#include "miniport/ddi/d3dkmdt_vidpn_present_path_record.hpp"
#include "miniport/ddi/dxgkarg_is_supported_vidpn_record.hpp"
#include "miniport/display/display_mode_list.hpp"
#include "miniport/display/display_sources.hpp"
#include "core/trace/function_scope.hpp"
#include "miniport/vidpn/path_transformation.hpp"
#include "miniport/vidpn/vidpn_mode_set.hpp"

namespace av {

VidPnSupportCheck::VidPnSupportCheck(Platform &platform, DxgkrnlInterface &dxgkrnl, const DisplayModeList &modes,
                                     const DisplaySources &sources, DxgkargIsSupportedVidPnRecord &request)
    : platform_(platform), modes_(modes), sources_(sources), request_(request), vidpn_(platform, dxgkrnl, request.hDesiredVidPn), paths_(0u),
      supported_(true), refusal_(nullptr), refused_mode_{}
      {
          AV_SCOPE("VidPnSupportCheck::VidPnSupportCheck");
      }

NtStatus VidPnSupportCheck::decide()
{
    AV_SCOPE("VidPnSupportCheck::decide");
    platform_.require_irql_at_most(Irql::passive, "VidPnSupportCheck::decide");
    request_.IsVidPnSupported = 0u;
    if (request_.hDesiredVidPn == nullptr) {
        request_.IsVidPnSupported = 1u;
        return NtStatus(Status::ok);
    }
    Status status = vidpn_.initialize();
    if (status == Status::ok) status = vidpn_.for_each_path(*this);
    if (!supported_) return NtStatus(Status::ok);
    if (status != Status::ok) return NtStatus(status);
    request_.IsVidPnSupported = 1u;
    return NtStatus(Status::ok);
}

Status VidPnSupportCheck::visit(const D3dkmdtVidPnPresentPathRecord &path)
{
    AV_SCOPE("VidPnSupportCheck::visit");
    platform_.require_irql_at_most(Irql::passive, "VidPnSupportCheck::visit");
    paths_++;
    refusal_ = paths_ > max_paths                                   ? "paths"
               : path.VidPnSourceId >= sources_.count() ||
                         path.VidPnTargetId >= sources_.count()      ? "path ends"
               : !PathTransformation(path).supportable()             ? "transformation"
                                                                     : nullptr;
    supported_ = refusal_ == nullptr;
    if (!supported_) return Status::not_supported;
    const Status offered = offers_pinned_source(path.VidPnSourceId);
    supported_ = offered != Status::not_supported;
    if (!supported_) refusal_ = "source mode";
    if (offered != Status::ok) return offered;
    const Status timed = offers_pinned_target(path.VidPnTargetId);
    supported_ = timed != Status::not_supported;
    if (!supported_) refusal_ = "target mode";
    return timed;
}

const char *VidPnSupportCheck::refusal() const
{
    AV_SCOPE("VidPnSupportCheck::refusal");
    return refusal_;
}

const D3dkmdtVidPnSourceModeRecord &VidPnSupportCheck::refused_mode() const
{
    AV_SCOPE("VidPnSupportCheck::refused_mode");
    return refused_mode_;
}

Status VidPnSupportCheck::offers_pinned_source(uint32_t source)
{
    AV_SCOPE("VidPnSupportCheck::offers_pinned_source");
    SourceModeSet                set(platform_, vidpn_, source);
    D3dkmdtVidPnSourceModeRecord pinned = {};
    Status                       status = set.acquire();
    if (status == Status::ok) status = set.copy_pinned(pinned);
    if (status == Status::not_found) return Status::ok;
    if (status != Status::ok) return status;
    if (modes_.find(pinned) != nullptr) return Status::ok;
    refused_mode_ = pinned;
    return Status::not_supported;
}

Status VidPnSupportCheck::offers_pinned_target(uint32_t target)
{
    AV_SCOPE("VidPnSupportCheck::offers_pinned_target");
    TargetModeSet                set(platform_, vidpn_, target);
    D3dkmdtVidPnTargetModeRecord pinned = {};
    if (set.acquire() != Status::ok || set.copy_pinned(pinned) != Status::ok) return Status::ok;
    if (modes_.find(pinned.VideoSignalInfo) != nullptr) return Status::ok;
    return Status::not_supported;
}

}
