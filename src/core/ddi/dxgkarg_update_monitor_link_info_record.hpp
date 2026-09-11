/* DXGKARG_UPDATEMONITORLINKINFO, as DxgkDdiUpdateMonitorLinkInfo receives and
 * answers it, its DXGK_MONITORLINKINFO flattened. A DDI record;
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_UPDATE_MONITOR_LINK_INFO_RECORD_HPP
#define AV_DXGKARG_UPDATE_MONITOR_LINK_INFO_RECORD_HPP

#include <stdint.h>

namespace av {

struct DxgkargUpdateMonitorLinkInfoRecord {
    uint32_t VideoPresentTargetId;
    uint32_t MonitorLinkInfoUsageHints;
    uint32_t MonitorLinkInfoCapabilities;
    uint32_t MonitorLinkInfoDitheringSupport;
};

}  // namespace av

#endif  // AV_DXGKARG_UPDATE_MONITOR_LINK_INFO_RECORD_HPP
