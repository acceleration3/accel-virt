/* DxgkDdiIsSupportedVidPn for one proposed VidPn: supported when it has at most
 * one path and that path joins the adapter's one source to its one target, as
 * basicdisplay.sys decides. */
#ifndef AV_VIDPN_SUPPORT_CHECK_HPP
#define AV_VIDPN_SUPPORT_CHECK_HPP

#include "nt_status.hpp"
#include "path_visitor.hpp"
#include "platform.hpp"
#include "vidpn.hpp"

#include <stdint.h>

namespace av {

class DxgkrnlInterface;
struct DxgkargIsSupportedVidPnRecord;

class VidPnSupportCheck final : public PathVisitor {
public:
    VidPnSupportCheck(Platform &platform, DxgkrnlInterface &dxgkrnl, DxgkargIsSupportedVidPnRecord &request);

    /* Success whether or not the VidPn is supported; IsVidPnSupported says. */
    NtStatus decide();
    [[nodiscard]] Status visit(const D3dkmdtVidPnPresentPathRecord &path) override;

private:
    static constexpr uint32_t max_paths = 1u;

    Platform                      &platform_;
    DxgkargIsSupportedVidPnRecord &request_;
    VidPn                          vidpn_;
    uint32_t                       paths_;
    bool                           supported_;
};

}  // namespace av

#endif  // AV_VIDPN_SUPPORT_CHECK_HPP
