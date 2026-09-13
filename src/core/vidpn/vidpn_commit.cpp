#include "core/vidpn/vidpn_commit.hpp"

#include "core/ddi/dxgkarg_commit_vidpn_record.hpp"
#include "core/display/display_mode_list.hpp"
#include "core/display/display_sources.hpp"
#include "core/vidpn/vidpn_mode_set.hpp"

namespace av {

VidPnCommit::VidPnCommit(Platform &platform, DxgkrnlInterface &dxgkrnl, const DisplayModeList &modes,
                         DisplaySources &sources, const DxgkargCommitVidPnRecord &request)
    : platform_(platform), modes_(modes), sources_(sources), request_(request),
      vidpn_(platform, dxgkrnl, request.hFunctionalVidPn) {}

/* A path powering off keeps its mode for when it powers on again. */
NtStatus VidPnCommit::apply()
{
    platform_.require_irql_at_most(Irql::passive, "VidPnCommit::apply");
    const uint32_t source = request_.AffectedVidPnSourceId;
    if (source >= sources_.count()) return NtStatus(Status::invalid_parameter);
    if ((request_.Flags & path_powered_off) != 0u) return NtStatus(Status::ok);
    uint64_t paths = 0u;
    Status   status = vidpn_.initialize();
    if (status == Status::ok) status = vidpn_.source_paths(source, paths);
    if (status != Status::ok) return NtStatus(status);
    if (paths == 0u) return NtStatus(sources_.commit(source, nullptr));
    SourceModeSet                set(platform_, vidpn_, source);
    D3dkmdtVidPnSourceModeRecord pinned = {};
    status = set.acquire();
    if (status == Status::ok) status = set.copy_pinned(pinned);
    if (status == Status::not_found) return NtStatus(sources_.commit(source, nullptr));
    if (status != Status::ok) return NtStatus(status);
    const DisplayMode *mode = modes_.find(pinned);
    if (mode == nullptr) return NtStatus(NtStatus::invalid_video_present_source_mode);
    return NtStatus(sources_.commit(source, mode));
}

}  // namespace av
