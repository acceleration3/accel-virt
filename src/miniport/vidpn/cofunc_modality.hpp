#ifndef AV_COFUNC_MODALITY_HPP
#define AV_COFUNC_MODALITY_HPP

#include "miniport/ddi/d3dkmdt_vidpn_source_mode_record.hpp"
#include "core/platform/platform.hpp"
#include "core/support/nt_status.hpp"
#include "miniport/vidpn/path_visitor.hpp"
#include "miniport/vidpn/vidpn.hpp"

#include <stdint.h>

namespace av {

class DisplayModeList;
class DisplaySources;
class DxgkrnlInterface;
class LogBudget;
struct DxgkargEnumVidPnCofuncModalityRecord;

class CofuncModality final : public PathVisitor {
public:
    CofuncModality(Platform &platform, DxgkrnlInterface &dxgkrnl, LogBudget &budget,
                   const DisplayModeList &modes, const DisplaySources &sources,
                   const DxgkargEnumVidPnCofuncModalityRecord &request);

    NtStatus enumerate();
    [[nodiscard]] Status visit(const D3dkmdtVidPnPresentPathRecord &path) override;

private:
    enum : int32_t { pivot_source = 1, pivot_target = 2, pivot_scaling = 3, pivot_rotation = 4 };
    enum : uint32_t {
        preferred = 1u,
        not_preferred = 2u,
        rgb_8_bits_per_channel = 0x02u << 2u,
        identity_supported = 1u,
        rotation_offset_zero = 0x10u,
    };

    [[nodiscard]] Status copy_pinned_source(uint32_t source, D3dkmdtVidPnSourceModeRecord &mode);
    [[nodiscard]] Status pinned_target(uint32_t target);
    [[nodiscard]] Status offer_sources(uint32_t source);
    [[nodiscard]] Status offer_targets(uint32_t target, const D3dkmdtVidPnSourceModeRecord *pinned_source);
    [[nodiscard]] Status update_support(const D3dkmdtVidPnPresentPathRecord &path);
    void                 log_path(const D3dkmdtVidPnPresentPathRecord &path,
                                  const D3dkmdtVidPnSourceModeRecord *pinned_source, bool target_pinned);

    Platform                                   &platform_;
    const DisplaySources                       &sources_;
    LogBudget                                  &budget_;
    const DisplayModeList                      &modes_;
    const DxgkargEnumVidPnCofuncModalityRecord &request_;
    VidPn                                       vidpn_;
};

}

#endif
