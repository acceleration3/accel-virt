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

}

#endif
