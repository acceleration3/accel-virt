#ifndef AV_VIDPN_SUPPORT_CHECK_HPP
#define AV_VIDPN_SUPPORT_CHECK_HPP

#include "core/platform/platform.hpp"
#include "core/support/nt_status.hpp"
#include "miniport/vidpn/path_visitor.hpp"
#include "miniport/ddi/d3dkmdt_vidpn_source_mode_record.hpp"
#include "miniport/vidpn/vidpn.hpp"

#include <stdint.h>

namespace av {

class DisplayModeList;
class DisplaySources;
class DxgkrnlInterface;
struct DxgkargIsSupportedVidPnRecord;

class VidPnSupportCheck final : public PathVisitor {
public:
    VidPnSupportCheck(Platform &platform, DxgkrnlInterface &dxgkrnl, const DisplayModeList &modes,
                      const DisplaySources &sources, DxgkargIsSupportedVidPnRecord &request);

    NtStatus decide();
    [[nodiscard]] Status visit(const D3dkmdtVidPnPresentPathRecord &path) override;
    const char *refusal() const;
    const D3dkmdtVidPnSourceModeRecord &refused_mode() const;

private:
    static constexpr uint32_t max_paths = 1u;

    [[nodiscard]] Status offers_pinned_source(uint32_t source);
    [[nodiscard]] Status offers_pinned_target(uint32_t target);

    Platform                      &platform_;
    const DisplayModeList         &modes_;
    const DisplaySources          &sources_;
    DxgkargIsSupportedVidPnRecord &request_;
    VidPn                          vidpn_;
    uint32_t                       paths_;
    bool                           supported_;
    const char                    *refusal_;
    D3dkmdtVidPnSourceModeRecord   refused_mode_;
};

}

#endif
