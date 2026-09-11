/* DXGKARG_SETVIDPNSOURCEADDRESS, as DxgkDdiSetVidPnSourceAddress receives it.
 * A DDI record: Flags is the DXGK_SETVIDPNSOURCEADDRESS_FLAGS Value;
 * src/km/ddi_record_layout.cpp proves each offset. */
#ifndef AV_DXGKARG_SET_VIDPN_SOURCE_ADDRESS_RECORD_HPP
#define AV_DXGKARG_SET_VIDPN_SOURCE_ADDRESS_RECORD_HPP

#include "dxgk_primary_data_record.hpp"

#include <stdint.h>

namespace av {

struct DxgkargSetVidPnSourceAddressRecord {
    uint32_t              VidPnSourceId;
    uint32_t              PrimarySegment;
    int64_t               PrimaryAddress;
    void                  *hAllocation;
    uint32_t              ContextCount;
    uint32_t              ContextCountPadding;
    void                  *Context[65];   /* 1 + D3DDDI_MAX_BROADCAST_CONTEXT */
    uint32_t              Flags;
    uint32_t              Duration;
    DxgkPrimaryDataRecord PrimaryData[64];
    uint32_t              DriverPrivateDataSize;
    uint32_t              DriverPrivateDataSizePadding;
    void                  *pDriverPrivateData;
};

}  // namespace av

#endif  // AV_DXGKARG_SET_VIDPN_SOURCE_ADDRESS_RECORD_HPP
