#ifndef AV_DXGKARG_RECOMMEND_MONITOR_MODES_RECORD_HPP
#define AV_DXGKARG_RECOMMEND_MONITOR_MODES_RECORD_HPP

#include "miniport/ddi/dxgk_vidpn_interface_records.hpp"

#include <stdint.h>

namespace av {

struct DxgkargRecommendMonitorModesRecord {
    uint32_t                                      VideoPresentTargetId;
    uint32_t                                      VideoPresentTargetIdPadding;
    void                                          *hMonitorSourceModeSet;
    const DxgkMonitorSourceModeSetInterfaceRecord *pMonitorSourceModeSetInterface;
};

}

#endif
