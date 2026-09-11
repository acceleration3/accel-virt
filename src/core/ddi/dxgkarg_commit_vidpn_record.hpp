/* DXGKARG_COMMITVIDPN, as DxgkDdiCommitVidPn receives it. Flags is the
 * DXGKARG_COMMITVIDPN_FLAGS bitfield UINT. A DDI record;
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_COMMIT_VIDPN_RECORD_HPP
#define AV_DXGKARG_COMMIT_VIDPN_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargCommitVidPnRecord {
    void     *hFunctionalVidPn;
    uint32_t AffectedVidPnSourceId;
    int32_t  MonitorConnectivityChecks;
    void     *hPrimaryAllocation;
    uint32_t Flags;
    uint32_t Padding;
};

}  // namespace av

#endif  // AV_DXGKARG_COMMIT_VIDPN_RECORD_HPP
