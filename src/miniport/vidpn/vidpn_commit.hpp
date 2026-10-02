/* DxgkDdiCommitVidPn for one source of one functional VidPn: the pinned source
 * mode becomes the source's mode, or the source has none when the VidPn gives
 * it no path or no pinned mode. */
#ifndef AV_VIDPN_COMMIT_HPP
#define AV_VIDPN_COMMIT_HPP

#include "core/platform/platform.hpp"
#include "core/support/nt_status.hpp"
#include "miniport/vidpn/vidpn.hpp"

#include <stdint.h>

namespace av {

class DisplayModeList;
class DisplaySources;
class DxgkrnlInterface;
struct DxgkargCommitVidPnRecord;

class VidPnCommit {
public:
    VidPnCommit(Platform &platform, DxgkrnlInterface &dxgkrnl, const DisplayModeList &modes, DisplaySources &sources,
                const DxgkargCommitVidPnRecord &request);
    VidPnCommit(const VidPnCommit &) = delete;
    VidPnCommit &operator=(const VidPnCommit &) = delete;

    NtStatus apply();

private:
    static constexpr uint32_t path_powered_off = 2u;   /* DXGKARG_COMMITVIDPN_FLAGS.PathPoweredOff */

    Platform                       &platform_;
    const DisplayModeList          &modes_;
    DisplaySources                 &sources_;
    const DxgkargCommitVidPnRecord &request_;
    VidPn                           vidpn_;
};

}  // namespace av

#endif  // AV_VIDPN_COMMIT_HPP
