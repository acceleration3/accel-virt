/* DxgkDdiEnumVidPnCofuncModality for one constraining VidPn, as basicdisplay.sys
 * does it: on every path, a mode set that is neither pinned nor the pivot is
 * replaced by the adapter's modes - the target's narrowed to a pinned source
 * mode - and a transformation not yet chosen supports identity only. */
#ifndef AV_COFUNC_MODALITY_HPP
#define AV_COFUNC_MODALITY_HPP

#include "core/ddi/d3dkmdt_vidpn_source_mode_record.hpp"
#include "core/platform/platform.hpp"
#include "core/support/nt_status.hpp"
#include "core/vidpn/path_visitor.hpp"
#include "core/vidpn/vidpn.hpp"

#include <stdint.h>

namespace av {

class DisplayModeList;
class DxgkrnlInterface;
struct DxgkargEnumVidPnCofuncModalityRecord;

class CofuncModality final : public PathVisitor {
public:
    CofuncModality(Platform &platform, DxgkrnlInterface &dxgkrnl, const DisplayModeList &modes,
                   const DxgkargEnumVidPnCofuncModalityRecord &request);

    NtStatus enumerate();
    [[nodiscard]] Status visit(const D3dkmdtVidPnPresentPathRecord &path) override;

private:
    enum : int32_t { pivot_source = 1, pivot_target = 2, pivot_scaling = 3, pivot_rotation = 4 };   /* D3DKMDT_EPT_* */
    enum : uint32_t {
        preferred = 1u,                /* D3DKMDT_MP_PREFERRED */
        not_preferred = 2u,
        rgb_8_bits_per_channel = 0x02u << 2u,   /* the Rgb field above the 2-bit preference */
        identity_supported = 1u,       /* the Identity bit of both support fields */
    };

    [[nodiscard]] Status copy_pinned_source(uint32_t source, D3dkmdtVidPnSourceModeRecord &mode);
    [[nodiscard]] Status pinned_target(uint32_t target);
    [[nodiscard]] Status offer_sources(uint32_t source);
    /* Every mode, or only the one matching `pinned_source` when it is not null. */
    [[nodiscard]] Status offer_targets(uint32_t target, const D3dkmdtVidPnSourceModeRecord *pinned_source);
    [[nodiscard]] Status update_support(const D3dkmdtVidPnPresentPathRecord &path);

    Platform                                   &platform_;
    const DisplayModeList                      &modes_;
    const DxgkargEnumVidPnCofuncModalityRecord &request_;
    VidPn                                       vidpn_;
};

}  // namespace av

#endif  // AV_COFUNC_MODALITY_HPP
