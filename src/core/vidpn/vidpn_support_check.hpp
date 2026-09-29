/* DxgkDdiIsSupportedVidPn for one proposed VidPn: supported when it has at most
 * one path, that path joins a source to a target the adapter has, it pins no
 * transformation the adapter cannot do, as basicdisplay.sys decides, and the
 * source mode it pins, if any, is one the adapter offers. */
#ifndef AV_VIDPN_SUPPORT_CHECK_HPP
#define AV_VIDPN_SUPPORT_CHECK_HPP

#include "core/platform/platform.hpp"
#include "core/support/nt_status.hpp"
#include "core/vidpn/path_visitor.hpp"
#include "core/vidpn/vidpn.hpp"

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

    /* Success whether or not the VidPn is supported; IsVidPnSupported says. */
    NtStatus decide();
    [[nodiscard]] Status visit(const D3dkmdtVidPnPresentPathRecord &path) override;

private:
    static constexpr uint32_t max_paths = 1u;

    /* ok with no source mode pinned, not_supported for one the adapter does not offer. */
    [[nodiscard]] Status offers_pinned_source(uint32_t source);

    Platform                      &platform_;
    const DisplayModeList         &modes_;
    const DisplaySources          &sources_;
    DxgkargIsSupportedVidPnRecord &request_;
    VidPn                          vidpn_;
    uint32_t                       paths_;
    bool                           supported_;
};

}  // namespace av

#endif  // AV_VIDPN_SUPPORT_CHECK_HPP
