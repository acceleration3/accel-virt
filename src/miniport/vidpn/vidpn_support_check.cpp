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
      supported_(true)
      {
          AV_SCOPE("VidPnSupportCheck::VidPnSupportCheck");
      }

/* A null VidPn asks whether the adapter can have no display at all. */
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

/* Refusing the path ends the walk; supported_ tells the refusal from a failure. */
Status VidPnSupportCheck::visit(const D3dkmdtVidPnPresentPathRecord &path)
{
    AV_SCOPE("VidPnSupportCheck::visit");
    platform_.require_irql_at_most(Irql::passive, "VidPnSupportCheck::visit");
    paths_++;
    supported_ = paths_ <= max_paths && path.VidPnSourceId < sources_.count() &&
                 path.VidPnTargetId < sources_.count() && PathTransformation(path).supportable();
    if (!supported_) return Status::not_supported;
    const Status offered = offers_pinned_source(path.VidPnSourceId);
    supported_ = offered != Status::not_supported;
    return offered;
}

/* CommitVidPn refuses a source mode the adapter does not offer, but by then DMM
 * has chosen it: at boot it pins what it saved last time, which the host may no
 * longer offer, or which is an older driver's X8R8G8B8. Refused here, DMM asks
 * for the recommended VidPn instead. */
Status VidPnSupportCheck::offers_pinned_source(uint32_t source)
{
    AV_SCOPE("VidPnSupportCheck::offers_pinned_source");
    SourceModeSet                set(platform_, vidpn_, source);
    D3dkmdtVidPnSourceModeRecord pinned = {};
    Status                       status = set.acquire();
    if (status == Status::ok) status = set.copy_pinned(pinned);
    if (status == Status::not_found) return Status::ok;
    if (status != Status::ok) return status;
    return modes_.find(pinned) != nullptr ? Status::ok : Status::not_supported;
}

}  // namespace av
