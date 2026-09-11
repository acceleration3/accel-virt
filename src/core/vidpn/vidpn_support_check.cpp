#include "core/vidpn/vidpn_support_check.hpp"

#include "core/ddi/d3dkmdt_vidpn_present_path_record.hpp"
#include "core/ddi/dxgkarg_is_supported_vidpn_record.hpp"
#include "core/display/display_sources.hpp"

namespace av {

VidPnSupportCheck::VidPnSupportCheck(Platform &platform, DxgkrnlInterface &dxgkrnl,
                                     DxgkargIsSupportedVidPnRecord &request)
    : platform_(platform), request_(request), vidpn_(platform, dxgkrnl, request.hDesiredVidPn), paths_(0u),
      supported_(true) {}

/* A null VidPn asks whether the adapter can have no display at all. */
NtStatus VidPnSupportCheck::decide()
{
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

/* Refusing the path ends the walk; supported_ tells the refusal from a failure. */
Status VidPnSupportCheck::visit(const D3dkmdtVidPnPresentPathRecord &path)
{
    platform_.require_irql_at_most(Irql::passive, "VidPnSupportCheck::visit");
    paths_++;
    supported_ = paths_ <= max_paths && path.VidPnSourceId < DisplaySources::source_count &&
                 path.VidPnTargetId < DisplaySources::target_count;
    return supported_ ? Status::ok : Status::not_supported;
}

}  // namespace av
